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
	if (TryGetValue("role", value)) {
		return value.GetValue<string>();
	}
	return "";
}

// OIDC-specific getter methods
string SnowflakeSecret::GetOIDCToken() const {
	Value value;
	if (TryGetValue("oidc_token", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetOIDCClientId() const {
	Value value;
	if (TryGetValue("oidc_client_id", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetOIDCIssuerUrl() const {
	Value value;
	if (TryGetValue("oidc_issuer_url", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetOIDCRedirectUri() const {
	Value value;
	if (TryGetValue("oidc_redirect_uri", value)) {
		return value.GetValue<string>();
	}
	return "";
}

string SnowflakeSecret::GetOIDCScope() const {
	Value value;
	if (TryGetValue("oidc_scope", value)) {
		return value.GetValue<string>();
	}
	return "openid"; // Default scope
}

//! Validate that all required fields are present
void SnowflakeSecret::Validate() const {
	vector<string> required_fields = {"account", "database"};
	vector<string> missing_fields;

	for (const auto &field : required_fields) {
		Value value;
		if (!TryGetValue(field, value) || value.IsNull()) {
			missing_fields.push_back(field);
		}
	}

	if (!missing_fields.empty()) {
		throw InvalidInputException("Snowflake secret is missing required fields: %s",
		                            StringUtil::Join(missing_fields, ", "));
	}

	// Validate authentication method - must have either password auth or OIDC auth
	bool has_password_auth = false;
	bool has_oidc_auth = false;

	Value user_value, password_value, oidc_token_value, oidc_client_id_value, token_file_value;
	if (TryGetValue("user", user_value) && !user_value.IsNull() &&
	    TryGetValue("password", password_value) && !password_value.IsNull()) {
		has_password_auth = true;
	}
	
	// Check for OIDC authentication (either token or client_id for flow)
	if (TryGetValue("oidc_token", oidc_token_value) && !oidc_token_value.IsNull()) {
		has_oidc_auth = true;
	}
	if (TryGetValue("oidc_client_id", oidc_client_id_value) && !oidc_client_id_value.IsNull()) {
		has_oidc_auth = true;
	}
	if (TryGetValue("token_file_path", token_file_value) && !token_file_value.IsNull()) {
		has_oidc_auth = true;
	}

	if (!has_password_auth && !has_oidc_auth) {
		throw InvalidInputException("Snowflake secret requires either 'user' and 'password' for password authentication, or OIDC parameters ('oidc_token', 'oidc_client_id', or 'token_file_path') for OIDC authentication");
	}

	if (has_password_auth && has_oidc_auth) {
		throw InvalidInputException("Snowflake secret cannot have both password authentication and OIDC authentication - choose one method");
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

//! Create function for Snowflake secrets
unique_ptr<BaseSecret> CreateSnowflakeSecret(ClientContext &context, CreateSecretInput &input) {
	// Create the secret with the provided scope and name
	auto secret = make_uniq<SnowflakeSecret>(input.scope, input.provider, input.name);

	// Extract Snowflake-specific parameters from the input options
	vector<string> required_fields = {"account", "database"};
	vector<string> optional_fields = {"user", "password", "warehouse", "schema", "role", "oidc_token", "oidc_client_id", "oidc_issuer_url", "oidc_redirect_uri", "oidc_scope", "token_file_path", "workload_identity_provider", "private_key"};

	// Process required fields
	for (const auto &field : required_fields) {
		auto it = input.options.find(field);
		if (it == input.options.end()) {
			throw InvalidInputException("Snowflake secret requires field '%s'", field);
		}

		// Store the value in the secret map
		secret->secret_map[field] = it->second;
	}

	// Process optional fields
	for (const auto &field : optional_fields) {
		auto it = input.options.find(field);
		if (it != input.options.end()) {
			secret->secret_map[field] = it->second;
		}
	}

	// Validate the secret
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
	create_function.named_parameters["warehouse"] = LogicalType::VARCHAR;
	create_function.named_parameters["database"] = LogicalType::VARCHAR;
	create_function.named_parameters["schema"] = LogicalType::VARCHAR;
	create_function.named_parameters["role"] = LogicalType::VARCHAR;
	
	// OIDC authentication parameters
	create_function.named_parameters["oidc_token"] = LogicalType::VARCHAR;
	create_function.named_parameters["oidc_client_id"] = LogicalType::VARCHAR;
	create_function.named_parameters["oidc_issuer_url"] = LogicalType::VARCHAR;
	create_function.named_parameters["oidc_redirect_uri"] = LogicalType::VARCHAR;
	create_function.named_parameters["oidc_scope"] = LogicalType::VARCHAR;
	
	// Other authentication methods
	create_function.named_parameters["token_file_path"] = LogicalType::VARCHAR;
	create_function.named_parameters["workload_identity_provider"] = LogicalType::VARCHAR;
	create_function.named_parameters["private_key"] = LogicalType::VARCHAR;

	// Register the create function
	secret_manager.RegisterSecretFunction(create_function, OnCreateConflict::ERROR_ON_CONFLICT);
}

} // namespace duckdb
