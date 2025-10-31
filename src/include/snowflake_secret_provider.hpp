#pragma once

#include "duckdb.hpp"
#include "duckdb/main/secret/secret.hpp"
#include "duckdb/main/secret/secret_manager.hpp"

namespace duckdb {

//! Custom Snowflake secret that extends KeyValueSecret
class SnowflakeSecret : public KeyValueSecret {
public:
	SnowflakeSecret(const vector<string> &prefix_paths, const string &provider, const string &name)
	    : KeyValueSecret(prefix_paths, "snowflake", provider, name) {
		// Mark sensitive fields for redaction
		redact_keys.insert("password");
		redact_keys.insert("secret");
		redact_keys.insert("token");
		redact_keys.insert("private_key");
		redact_keys.insert("private_key_passphrase");
	}

	//! Get Snowflake-specific fields
	string GetUser() const;
	string GetPassword() const;
	string GetAccount() const;
	string GetWarehouse() const;
	string GetDatabase() const;
	string GetSchema() const;
	string GetRole() const;

	//! Get authentication-specific fields
	string GetAuthType() const;
	string GetToken() const;
	string GetOktaUrl() const;
	string GetPrivateKey() const;
	string GetPrivateKeyPassphrase() const;

	//! Validate that all required fields are present
	void Validate() const;

	//! Clone the secret
	unique_ptr<const BaseSecret> Clone() const override {
		return make_uniq<SnowflakeSecret>(*this);
	}

	//! Custom serialization for Snowflake secrets
	void Serialize(Serializer &serializer) const override;

	//! Custom deserialization for Snowflake secrets
	static unique_ptr<BaseSecret> Deserialize(Deserializer &deserializer, BaseSecret base_secret);
};

//! Create function for Snowflake secrets
unique_ptr<BaseSecret> CreateSnowflakeSecret(ClientContext &context, CreateSecretInput &input);

//! Register the Snowflake secret type with DuckDB
void RegisterSnowflakeSecretType(DatabaseInstance &instance);

} // namespace duckdb
