#include "snowflake_secret_provider.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/serializer/deserializer.hpp"
#include "duckdb/common/serializer/serializer.hpp"

namespace duckdb {

//! Get Snowflake-specific fields
string SnowflakeSecret::GetUser() const {
	Value value;
	if (TryGetValue("user", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetPassword() const {
	Value value;
	if (TryGetValue("password", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetAccount() const {
	Value value;
	if (TryGetValue("account", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetHost() const {
	Value value;
	if (TryGetValue("host", value)) {
		return value.GetValue<string>();
	}
	return "";
}

int32_t SnowflakeSecret::GetPort() const {
	Value value;
	if (TryGetValue("port", value)) {
		return value.GetValue<int32_t>();
	}
	return 443; // Default HTTPS port
}

string SnowflakeSecret::GetProtocol() const {
	Value value;
	if (TryGetValue("protocol", value)) {
		return value.GetValue<string>();
	}
	return ""; // Empty means use default (https)
}

string SnowflakeSecret::GetWarehouse() const {
	Value value;
	if (TryGetValue("warehouse", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetDatabase() const {
	Value value;
	if (TryGetValue("database", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetSchema() const {
	Value value;
	if (TryGetValue("schema", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetRole() const {
	Value value;
	if (TryGetValue("role", value) || TryGetValue("ROLE", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetAuthType() const {
	Value value;
	// Try both lowercase and uppercase variants
	if (TryGetValue("auth_type", value) || TryGetValue("AUTH_TYPE", value)) {
		return value.GetValue<string>();
	}
	return "password"; // default to password auth
}

string SnowflakeSecret::GetPrivateKey() const {
	Value value;
	// Try both lowercase and uppercase variants
	if (TryGetValue("private_key", value) || TryGetValue("PRIVATE_KEY", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetPrivateKeyFile() const {
	Value value;
	// Try both lowercase and uppercase variants
	if (TryGetValue("private_key_file", value) || TryGetValue("PRIVATE_KEY_FILE", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetPrivateKeyPassword() const {
	Value value;
	// Try both lowercase and uppercase variants
	if (TryGetValue("private_key_password", value) || TryGetValue("PRIVATE_KEY_PASSWORD", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetToken() const {
	Value value;
	// Try both lowercase and uppercase variants
	if (TryGetValue("token", value) || TryGetValue("TOKEN", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetOktaUrl() const {
	Value value;
	if (TryGetValue("okta_url", value) || TryGetValue("OKTA_URL", value)) {
		return value.GetValue<string>();
	}
	return "";
}

//! Helper to try getting value with case-insensitive key lookup
static bool TryGetValueCaseInsensitive(const SnowflakeSecret &secret, const string &key, Value &value) {
	return secret.TryGetValue(key, value);
}

//! Validate that all required fields are present
void SnowflakeSecret::Validate() const {
	// Always required fields
	vector<string> always_required = {"user", "account", "database"};
	vector<string> missing_fields;

	for (const auto &field : always_required) {
		Value value;
		if (!TryGetValueCaseInsensitive(*this, field, value) || value.IsNull()) {
			missing_fields.push_back(field);
		}
	}

	if (!missing_fields.empty()) {
		throw InvalidInputException("Snowflake secret is missing required fields: %s",
		                            StringUtil::Join(missing_fields, ", "));
	}

	// Check auth_type and require appropriate credential
	string auth_type = GetAuthType();
	if (auth_type == "key_pair") {
		string pk = GetPrivateKey();
		string pk_file = GetPrivateKeyFile();
		if (pk.empty() && pk_file.empty()) {
			throw InvalidInputException(
			    "Snowflake secret with auth_type 'key_pair' requires 'private_key' or 'private_key_file' field");
		}
	} else if (auth_type == "oauth") {
		// OAuth requires a token
		Value token_value;
		if (!TryGetValueCaseInsensitive(*this, "token", token_value) || token_value.IsNull() ||
		    token_value.GetValue<string>().empty()) {
			throw InvalidInputException("Snowflake secret with auth_type 'oauth' requires 'token' field");
		}
	} else if (StringUtil::CIEquals(auth_type, "ext_browser") || StringUtil::CIEquals(auth_type, "externalbrowser")) {
		// External browser SSO flow — no credential field required on the secret
	} else {
		// password auth (default), and also MFA / Okta which still need a password
		string pw = GetPassword();
		if (pw.empty()) {
			throw InvalidInputException("Snowflake secret requires 'password' field (or use auth_type 'key_pair' with "
			                            "'private_key'/'private_key_file', auth_type 'oauth' with 'token', or "
			                            "auth_type 'ext_browser')");
		}
	}
}

//! Custom serialization for Snowflake secrets
void SnowflakeSecret::Serialize(Serializer &serializer) const {
	// First serialize the base KeyValueSecret
	KeyValueSecret::Serialize(serializer);

	// Add any Snowflake-specific serialization if needed
	// For now, we just use the base KeyValueSecret serialization
}

//! Custom deserialization for Snowflake secrets
unique_ptr<BaseSecret> SnowflakeSecret::Deserialize(Deserializer &deserializer, BaseSecret base_secret) {
	auto result = make_uniq<SnowflakeSecret>(base_secret.GetScope(), base_secret.GetProvider(), base_secret.GetName());

	// Deserialize the secret map
	Value secret_map_value;
	deserializer.ReadProperty(201, "secret_map", secret_map_value);

	for (const auto &entry : ListValue::GetChildren(secret_map_value)) {
		auto kv_struct = StructValue::GetChildren(entry);
		result->secret_map[kv_struct[0].ToString()] = kv_struct[1];
	}

	// Deserialize the redact keys
	Value redact_set_value;
	deserializer.ReadProperty(202, "redact_keys", redact_set_value);
	for (const auto &entry : ListValue::GetChildren(redact_set_value)) {
		result->redact_keys.insert(entry.ToString());
	}

	return std::move(result);
}

//! Helper to find option with case-insensitive key lookup
static case_insensitive_map_t<Value>::const_iterator
FindOptionCaseInsensitive(const case_insensitive_map_t<Value> &options, const string &key) {
	return options.find(key);
}

//! Create function for Snowflake secrets
unique_ptr<BaseSecret> CreateSnowflakeSecret(ClientContext &context, CreateSecretInput &input) {
	// Create the secret with the provided scope and name
	auto secret = make_uniq<SnowflakeSecret>(input.scope, input.provider, input.name);

	// Extract Snowflake-specific parameters from the input options
	// Always required fields
	vector<string> always_required = {"user", "account", "database"};
	// Conditionally required (password OR private_key/private_key_file based on auth_type)
	vector<string> auth_fields = {"password",  "private_key", "private_key_file", "private_key_password",
	                              "auth_type", "token",       "okta_url"};

	// Accept private_key_passphrase as a backward-compatible alias for private_key_password
	auto passphrase_it = FindOptionCaseInsensitive(input.options, "private_key_passphrase");
	if (passphrase_it != input.options.end() &&
	    FindOptionCaseInsensitive(input.options, "private_key_password") == input.options.end()) {
		secret->secret_map["private_key_password"] = passphrase_it->second;
	}
	// Optional fields
	vector<string> optional_fields = {"warehouse", "schema", "role", "host", "port", "protocol"};

	// Process always required fields
	for (const auto &field : always_required) {
		auto it = FindOptionCaseInsensitive(input.options, field);
		if (it == input.options.end()) {
			throw InvalidInputException("Snowflake secret requires field '%s'", field);
		}
		// Store with lowercase key for consistent retrieval
		secret->secret_map[field] = it->second;
	}

	// Process auth-related fields (all optional at this stage, validated later)
	for (const auto &field : auth_fields) {
		auto it = FindOptionCaseInsensitive(input.options, field);
		if (it != input.options.end()) {
			// Store with lowercase key for consistent retrieval
			secret->secret_map[field] = it->second;
		}
	}

	// Process optional fields
	for (const auto &field : optional_fields) {
		auto it = FindOptionCaseInsensitive(input.options, field);
		if (it != input.options.end()) {
			// Store with lowercase key for consistent retrieval
			secret->secret_map[field] = it->second;
		}
	}

	// Validate the secret (checks auth_type and requires password OR private_key)
	secret->Validate();

	return std::move(secret);
}

//! Register the Snowflake secret type with DuckDB
void RegisterSnowflakeSecretType(DatabaseInstance &instance) {
	auto &secret_manager = SecretManager::Get(instance);

	// Create the secret type
	SecretType snowflake_type;
	snowflake_type.name = "snowflake";
	snowflake_type.default_provider = "config";
	snowflake_type.extension = "snowflake";
	snowflake_type.deserializer = SnowflakeSecret::Deserialize;

	// Register the secret type
	secret_manager.RegisterSecretType(snowflake_type);

	// Create the create function
	CreateSecretFunction create_function;
	create_function.secret_type = "snowflake";
	create_function.provider = "config";
	create_function.function = CreateSnowflakeSecret;

	// Define the named parameters for the CREATE SECRET statement
	create_function.named_parameters["user"] = LogicalType::VARCHAR;
	create_function.named_parameters["password"] = LogicalType::VARCHAR;
	create_function.named_parameters["account"] = LogicalType::VARCHAR;
	create_function.named_parameters["host"] = LogicalType::VARCHAR;
	create_function.named_parameters["port"] = LogicalType::INTEGER;
	create_function.named_parameters["protocol"] = LogicalType::VARCHAR;
	create_function.named_parameters["warehouse"] = LogicalType::VARCHAR;
	create_function.named_parameters["database"] = LogicalType::VARCHAR;
	create_function.named_parameters["schema"] = LogicalType::VARCHAR;
	create_function.named_parameters["role"] = LogicalType::VARCHAR;

	// Authentication parameters
	create_function.named_parameters["auth_type"] = LogicalType::VARCHAR;
	create_function.named_parameters["token"] = LogicalType::VARCHAR;
	create_function.named_parameters["okta_url"] = LogicalType::VARCHAR;
	create_function.named_parameters["private_key"] = LogicalType::VARCHAR;
	create_function.named_parameters["private_key_file"] = LogicalType::VARCHAR;
	create_function.named_parameters["private_key_password"] = LogicalType::VARCHAR;
	create_function.named_parameters["private_key_passphrase"] = LogicalType::VARCHAR; // backward-compat alias

	// Register the create function
	secret_manager.RegisterSecretFunction(create_function, OnCreateConflict::ERROR_ON_CONFLICT);
}

} // namespace duckdb
