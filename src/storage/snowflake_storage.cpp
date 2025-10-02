#include "snowflake_debug.hpp"
#include "storage/snowflake_storage.hpp"
#include "storage/snowflake_catalog.hpp"
#include "snowflake_transaction.hpp"
#include "snowflake_secrets.hpp"

#include "duckdb.hpp"

namespace duckdb {
namespace snowflake {

static unique_ptr<Catalog> SnowflakeAttach(StorageExtensionInfo *storage_info, ClientContext &context,
                                           AttachedDatabase &db, const string &name, AttachInfo &info,
                                           AccessMode access_mode) {
	DPRINT("SnowflakeAttach called with name: %s\n", name.c_str());

	SnowflakeConfig config;

	// Check if SECRET option is provided (try both cases as DuckDB might lowercase options)
	auto secret_entry = info.options.find("secret");
	if (secret_entry == info.options.end()) {
		secret_entry = info.options.find("SECRET");
	}

	if (secret_entry != info.options.end()) {
		// Use SECRET to get credentials (PREFERRED METHOD)
		string secret_name = secret_entry->second.ToString();
		DPRINT("Using SECRET: %s (PREFERRED METHOD)\n", secret_name.c_str());

		try {
			config = SnowflakeSecretsHelper::GetCredentials(context, secret_name);
			DPRINT("Retrieved config from secret - Database: %s, Auth Type: %s\n", 
			       config.database.c_str(), 
			       config.auth_type == SnowflakeAuthType::OIDC ? "OIDC" : "PASSWORD");
		} catch (const std::exception &e) {
			throw InvalidInputException("Failed to retrieve Snowflake credentials from secret '%s': %s",
			                            secret_name.c_str(), e.what());
		}
	} else if (!info.path.empty()) {
		// Fall back to connection string parsing (LEGACY METHOD)
		DPRINT("Using connection string from path (LEGACY METHOD)\n");
		config = SnowflakeConfig::ParseConnectionString(info.path);
		DPRINT("Parsed config - Database: %s, Auth Type: %s\n", 
		       config.database.c_str(),
		       config.auth_type == SnowflakeAuthType::OIDC ? "OIDC" : "PASSWORD");
	} else {
		throw InvalidInputException("Snowflake ATTACH requires either a SECRET option (PREFERRED) or connection string (LEGACY). "
		                            "Usage: ATTACH '' AS name (TYPE snowflake, SECRET secret_name) "
		                            "or ATTACH 'connection_string' AS name (TYPE snowflake)");
	}

	// Snowflake extension only supports read-only access
	// Force read-only mode regardless of what was requested
	access_mode = AccessMode::READ_ONLY;

	DPRINT("Creating SnowflakeCatalog\n");
	return make_uniq<SnowflakeCatalog>(db, config);
}

SnowflakeStorageExtension::SnowflakeStorageExtension() {
	attach = SnowflakeAttach;
	create_transaction_manager = SnowflakeCreateTransactionManager;
}

} // namespace snowflake
} // namespace duckdb
