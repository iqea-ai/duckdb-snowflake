#pragma once

#include <string>
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/secret/secret_manager.hpp"
#include "snowflake_config.hpp"

namespace duckdb {

// Helper class for working with DuckDB's secrets manager
class SnowflakeSecretsHelper {
public:
  // Store Snowflake credentials as a secret
  static void
  StoreCredentials(ClientContext &context, const std::string &profile_name,
                   const std::string &username, const std::string &password,
                   const std::string &account, const std::string &warehouse,
                   const std::string &database, const std::string &schema);

  // Retrieve Snowflake config from a secret
  static snowflake::SnowflakeConfig
  GetCredentials(ClientContext &context, const std::string &profile_name);

  // Delete a Snowflake credentials secret
  static bool DeleteCredentials(ClientContext &context,
                                const std::string &profile_name);

  // List all Snowflake profile names
  static std::vector<std::string> ListProfiles(ClientContext &context);

  // Validate Snowflake credentials by testing connection
  static bool ValidateCredentials(ClientContext &context,
                                  const std::string &profile_name,
                                  int timeout_seconds = 10);

  // Validate credentials with explicit parameters
  static bool
  ValidateCredentials(ClientContext &context, const std::string &username,
                      const std::string &password, const std::string &account,
                      const std::string &warehouse, const std::string &database,
                      const std::string &schema, int timeout_seconds = 10);
};

// Legacy interface for backward compatibility (deprecated)
class SnowflakeSecrets {
public:
  static std::string StoreCredentials(const std::string &profile_name);
  static std::string ListProfiles();
  static std::string GetConnectionString(const std::string &profile_name);
  static bool DeleteProfile(const std::string &profile_name);
};

} // namespace duckdb
