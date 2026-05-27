#include "snowflake_secrets.hpp"
#include "snowflake_secret_provider.hpp"
#include "snowflake_client.hpp"
#include "snowflake_client_manager.hpp"
#include "snowflake_config.hpp"
#include "snowflake_debug.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/secret/secret_manager.hpp"
#include "duckdb/main/secret/secret.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/adbc/adbc.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>

namespace duckdb {

// Store Snowflake credentials as a secret using DuckDB's secrets manager
void SnowflakeSecretsHelper::StoreCredentials(ClientContext &context, const std::string &profile_name,
                                              const std::string &username, const std::string &password,
                                              const std::string &account, const std::string &warehouse,
                                              const std::string &database, const std::string &schema) {

	// Create a snowflake secret with all the Snowflake credentials
	CreateSecretInput input;
	input.type = "snowflake";
	input.provider = "config";
	input.name = profile_name;
	input.persist_type = SecretPersistType::PERSISTENT;

	// Store all credentials as snowflake-specific fields
	input.options["user"] = Value(username);
	input.options["password"] = Value(password);
	input.options["account"] = Value(account);
	input.options["warehouse"] = Value(warehouse);
	input.options["database"] = Value(database);
	input.options["schema"] = Value(schema);

	// Create the secret
	SecretManager::Get(context).CreateSecret(context, input);
}

namespace {

//! Look up a key in a KeyValueSecret using DuckDB's case-insensitive map.
//! Returns empty string if missing or NULL.
std::string GetSecretString(const KeyValueSecret &secret, const std::string &key) {
	Value value;
	if (secret.TryGetValue(key, value) && !value.IsNull()) {
		return value.GetValue<std::string>();
	}
	return "";
}

int32_t GetSecretInt(const KeyValueSecret &secret, const std::string &key, int32_t default_value) {
	Value value;
	if (secret.TryGetValue(key, value) && !value.IsNull()) {
		return value.GetValue<int32_t>();
	}
	return default_value;
}

} // namespace

// Retrieve Snowflake config from a secret
snowflake::SnowflakeConfig SnowflakeSecretsHelper::GetCredentials(ClientContext &context,
                                                                  const std::string &profile_name) {
	snowflake::SnowflakeConfig config;

	try {
		auto &secret_manager = SecretManager::Get(context);
		auto transaction = CatalogTransaction::GetSystemCatalogTransaction(context);

		// Look up the secret by name
		auto secret_entry = secret_manager.GetSecretByName(transaction, profile_name);
		if (!secret_entry) {
			throw InvalidInputException("Snowflake profile not found: " + profile_name);
		}

		// Persistent secrets are deserialized at session start (before this
		// extension loads its typed deserializer), so they may arrive as a plain
		// KeyValueSecret rather than a SnowflakeSecret. Read fields through the
		// KeyValueSecret API to handle both cases uniformly.
		auto *kv_secret = dynamic_cast<const KeyValueSecret *>(secret_entry->secret.get());
		if (!kv_secret || secret_entry->secret->GetType() != "snowflake") {
			throw InvalidInputException("Invalid secret type for profile: " + profile_name);
		}

		config.username = GetSecretString(*kv_secret, "user");
		config.password = GetSecretString(*kv_secret, "password");
		config.account = GetSecretString(*kv_secret, "account");
		config.host = GetSecretString(*kv_secret, "host");
		config.port = GetSecretInt(*kv_secret, "port", 443);
		config.protocol = GetSecretString(*kv_secret, "protocol");
		config.warehouse = GetSecretString(*kv_secret, "warehouse");
		config.database = GetSecretString(*kv_secret, "database");
		config.role = GetSecretString(*kv_secret, "role");
		// Note: schema is not stored in SnowflakeConfig as per the struct
		// definition

		// Extract authentication-specific fields
		std::string auth_type_str = GetSecretString(*kv_secret, "auth_type");
		if (auth_type_str.empty()) {
			auth_type_str = "password";
		}
		if (StringUtil::CIEquals(auth_type_str, "oauth")) {
			config.auth_type = snowflake::SnowflakeAuthType::OAUTH;
			config.oauth_token = GetSecretString(*kv_secret, "token");
		} else if (StringUtil::CIEquals(auth_type_str, "key_pair")) {
			config.auth_type = snowflake::SnowflakeAuthType::KEY_PAIR;
			config.private_key = GetSecretString(*kv_secret, "private_key");
			config.private_key_file = GetSecretString(*kv_secret, "private_key_file");
			config.private_key_password = GetSecretString(*kv_secret, "private_key_password");
		} else if (StringUtil::CIEquals(auth_type_str, "ext_browser") ||
		           StringUtil::CIEquals(auth_type_str, "externalbrowser")) {
			config.auth_type = snowflake::SnowflakeAuthType::EXT_BROWSER;
		} else if (StringUtil::CIEquals(auth_type_str, "okta")) {
			config.auth_type = snowflake::SnowflakeAuthType::OKTA;
			config.okta_url = GetSecretString(*kv_secret, "okta_url");
		} else if (StringUtil::CIEquals(auth_type_str, "mfa")) {
			config.auth_type = snowflake::SnowflakeAuthType::MFA;
		} else {
			config.auth_type = snowflake::SnowflakeAuthType::PASSWORD;
		}

	} catch (const std::exception &e) {
		throw InvalidInputException("Failed to retrieve credentials for profile '" + profile_name + "': " + e.what());
	}

	return config;
}

// Delete a Snowflake credentials secret
bool SnowflakeSecretsHelper::DeleteCredentials(ClientContext &context, const std::string &profile_name) {
	try {
		auto &secret_manager = SecretManager::Get(context);
		auto transaction = CatalogTransaction::GetSystemCatalogTransaction(context);

		// Delete the secret by name
		secret_manager.DropSecretByName(transaction, profile_name, OnEntryNotFound::RETURN_NULL,
		                                SecretPersistType::PERSISTENT);
		return true;

	} catch (const std::exception &e) {
		// Log error but don't throw
		std::cerr << "Failed to delete credentials for profile '" << profile_name << "': " << e.what() << '\n';
		return false;
	}
}

// List all Snowflake profile names
std::vector<std::string> SnowflakeSecretsHelper::ListProfiles(ClientContext &context) {
	std::vector<std::string> profiles;

	try {
		auto &secret_manager = SecretManager::Get(context);
		auto transaction = CatalogTransaction::GetSystemCatalogTransaction(context);

		// Get all secrets
		auto all_secrets = secret_manager.AllSecrets(transaction);

		// Filter for Snowflake profile secrets
		for (const auto &secret_entry : all_secrets) {
			const auto &secret = *secret_entry.secret;

			// Check if this is a Snowflake secret
			if (secret.GetType() == "snowflake") {
				profiles.push_back(secret.GetName());
			}
		}

	} catch (const std::exception &e) {
		// Log error but don't throw
		std::cerr << "Failed to list profiles: " << e.what() << '\n';
	}

	return profiles;
}

// Legacy implementation for backward compatibility
std::string SnowflakeSecrets::StoreCredentials(const std::string &profile_name) {
	// This is deprecated - users should use the new secrets manager approach
	return "Deprecated: Use CREATE SECRET instead of this function";
}

std::string SnowflakeSecrets::ListProfiles() {
	// This is deprecated - users should use the new secrets manager approach
	return "Deprecated: Use SELECT * FROM duckdb_secrets() WHERE name LIKE "
	       "'snowflake_profile_%' instead";
}

std::string SnowflakeSecrets::GetConnectionString(const std::string &profile_name) {
	// This is deprecated - users should use the new secrets manager approach
	return "Deprecated: Use the new secrets manager approach instead";
}

bool SnowflakeSecrets::DeleteProfile(const std::string &profile_name) {
	// This is deprecated - users should use the new secrets manager approach
	return false;
}

// Validate Snowflake credentials by testing connection
bool SnowflakeSecretsHelper::ValidateCredentials(ClientContext &context, const std::string &profile_name,
                                                 int timeout_seconds) {
	try {
		// Get config from secrets manager
		auto config = GetCredentials(context, profile_name);

		// Use SnowflakeClientManager to validate
		auto &client_manager = snowflake::SnowflakeClientManager::GetInstance();

		try {
			// Try to get a connection - this will validate the credentials
			auto connection = client_manager.GetConnection(config);

			// If we got here, connection succeeded - test with a simple query
			AdbcStatement statement;
			AdbcError error_obj;
			std::memset(&error_obj, 0, sizeof(error_obj));
			std::memset(&statement, 0, sizeof(statement));

			AdbcStatusCode status = AdbcStatementNew(connection->GetConnection(), &statement, &error_obj);
			if (status != ADBC_STATUS_OK) {
				if (error_obj.release) {
					error_obj.release(&error_obj);
				}
				return false;
			}

			// Execute simple test query
			status = AdbcStatementSetSqlQuery(&statement, "SELECT 1", &error_obj);
			if (status != ADBC_STATUS_OK) {
				AdbcStatementRelease(&statement, &error_obj);
				if (error_obj.release) {
					error_obj.release(&error_obj);
				}
				return false;
			}

			ArrowArrayStream stream;
			std::memset(&stream, 0, sizeof(stream));
			status = AdbcStatementExecuteQuery(&statement, &stream, nullptr, &error_obj);
			bool success = (status == ADBC_STATUS_OK);

			// Clean up
			if (stream.release) {
				stream.release(&stream);
			}
			AdbcStatementRelease(&statement, &error_obj);
			if (error_obj.release) {
				error_obj.release(&error_obj);
			}

			return success;
		} catch (const IOException &inner_e) {
			// Connection failed - this is expected for invalid credentials
			fprintf(stderr, "[Snowflake Validation] Connection test failed for profile\n");
			fprintf(stderr, "  Error: %s\n", inner_e.what());
			return false;
		} catch (const std::exception &inner_e) {
			// Other connection failures
			fprintf(stderr, "[Snowflake Validation] Unexpected error during connection test\n");
			fprintf(stderr, "  Error: %s\n", inner_e.what());
			return false;
		}
	} catch (const std::exception &e) {
		// Log the error but don't throw
		std::cerr << "Failed to validate credentials for profile '" << profile_name << "': " << e.what() << '\n';
		return false;
	}
}

// Validate credentials with explicit parameters
bool SnowflakeSecretsHelper::ValidateCredentials(ClientContext &context, const std::string &username,
                                                 const std::string &password, const std::string &account,
                                                 const std::string &warehouse, const std::string &database,
                                                 const std::string &schema, int timeout_seconds) {
	try {
		// Build config directly
		snowflake::SnowflakeConfig config;
		config.username = username;
		config.password = password;
		config.account = account;
		config.warehouse = warehouse;
		config.database = database;
		// Note: schema is not stored in SnowflakeConfig

		// Use SnowflakeClientManager like scan does
		auto &client_manager = snowflake::SnowflakeClientManager::GetInstance();

		try {
			// Try to get a connection - this will validate the credentials
			auto connection = client_manager.GetConnection(config);

			// If we got here, connection succeeded
			// Test with a simple query to be sure
			AdbcStatement statement;
			AdbcError error_obj;
			std::memset(&error_obj, 0, sizeof(error_obj));
			std::memset(&statement, 0, sizeof(statement));

			AdbcStatusCode status = AdbcStatementNew(connection->GetConnection(), &statement, &error_obj);
			if (status != ADBC_STATUS_OK) {
				if (error_obj.release) {
					error_obj.release(&error_obj);
				}
				return false;
			}

			// Prepare and execute simple test query
			status = AdbcStatementSetSqlQuery(&statement, "SELECT 1", &error_obj);
			if (status != ADBC_STATUS_OK) {
				AdbcStatementRelease(&statement, &error_obj);
				if (error_obj.release) {
					error_obj.release(&error_obj);
				}
				return false;
			}

			// Execute query
			ArrowArrayStream stream;
			std::memset(&stream, 0, sizeof(stream));
			status = AdbcStatementExecuteQuery(&statement, &stream, nullptr, &error_obj);
			bool success = (status == ADBC_STATUS_OK);

			// Clean up
			if (stream.release) {
				stream.release(&stream);
			}
			AdbcStatementRelease(&statement, &error_obj);
			if (error_obj.release) {
				error_obj.release(&error_obj);
			}

			return success;
		} catch (const IOException &inner_e) {
			// Connection failed - this is expected for invalid credentials
			fprintf(stderr, "[Snowflake Validation] Connection test failed for profile\n");
			fprintf(stderr, "  Error: %s\n", inner_e.what());
			return false;
		} catch (const std::exception &inner_e) {
			// Other connection failures
			fprintf(stderr, "[Snowflake Validation] Unexpected error during connection test\n");
			fprintf(stderr, "  Error: %s\n", inner_e.what());
			return false;
		}
	} catch (const std::exception &e) {
		// Log the error but don't throw
		fprintf(stderr, "[Snowflake Validation] Failed to validate credentials: %s\n", e.what());
		return false;
	}
}

} // namespace duckdb
