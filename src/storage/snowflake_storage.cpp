#include "snowflake_debug.hpp"
#include "storage/snowflake_storage.hpp"
#include "storage/snowflake_catalog.hpp"
#include "snowflake_transaction.hpp"
#include "snowflake_secrets.hpp"

#include "duckdb.hpp"

namespace duckdb {
namespace snowflake {

static unique_ptr<Catalog> SnowflakeAttach(optional_ptr<StorageExtensionInfo> storage_info, ClientContext &context,
                                           AttachedDatabase &db, const string &name, AttachInfo &info,
                                           AttachOptions &options) {
	DPRINT("SnowflakeAttach called with name: %s\n", name.c_str());

	SnowflakeConfig config;

	// Check if SECRET option is provided (try both cases as DuckDB might
	// lowercase options)
	auto secret_entry = info.options.find("secret");
	if (secret_entry == info.options.end()) {
		secret_entry = info.options.find("SECRET");
	}

	if (secret_entry != info.options.end()) {
		// Use SECRET to get credentials
		string secret_name = secret_entry->second.ToString();
		DPRINT("Using SECRET: %s\n", secret_name.c_str());

		try {
			config = SnowflakeSecretsHelper::GetCredentials(context, secret_name);
			DPRINT("Retrieved config from secret - Database: %s\n", config.database.c_str());
		} catch (const std::exception &e) {
			throw InvalidInputException("Failed to retrieve Snowflake credentials from secret '%s': %s",
			                            secret_name.c_str(), e.what());
		}
	} else if (!info.path.empty()) {
		// Fall back to connection string parsing
		DPRINT("Using connection string from path\n");
		config = SnowflakeConfig::ParseConnectionString(info.path);
		DPRINT("Parsed config - Database: %s\n", config.database.c_str());
	} else {
		throw InvalidInputException("Snowflake ATTACH requires either a connection string or SECRET "
		                            "option. "
		                            "Usage: ATTACH 'connection_string' AS name (TYPE snowflake) "
		                            "or ATTACH '' AS name (TYPE snowflake, SECRET secret_name)");
	}

	if (options.access_mode != AccessMode::READ_ONLY) {
		throw NotImplementedException("Snowflake currently only supports read-only access");
	}

	// Parse enable_pushdown option
	SnowflakeOptions snowflake_options;
	snowflake_options.access_mode = options.access_mode;

	auto pushdown_entry = info.options.find("enable_pushdown");
	if (pushdown_entry == info.options.end()) {
		pushdown_entry = info.options.find("ENABLE_PUSHDOWN");
	}

	if (pushdown_entry != info.options.end()) {
		string pushdown_value = pushdown_entry->second.ToString();
		// Parse boolean value (supports "true", "1", "false", "0")
		if (pushdown_value == "true" || pushdown_value == "TRUE" || pushdown_value == "1") {
			snowflake_options.enable_pushdown = true;
			DPRINT("Pushdown ENABLED by user option\n");
		} else if (pushdown_value == "false" || pushdown_value == "FALSE" || pushdown_value == "0") {
			snowflake_options.enable_pushdown = false;
			DPRINT("Pushdown DISABLED by user option\n");
		} else {
			throw InvalidInputException("Invalid value for enable_pushdown: '%s'. "
			                            "Expected true/false or 1/0.",
			                            pushdown_value.c_str());
		}
	} else {
		DPRINT("Pushdown DISABLED by default (no enable_pushdown option provided)\n");
	}

	DPRINT("Creating SnowflakeCatalog\n");
	return make_uniq<SnowflakeCatalog>(db, config, snowflake_options);
}

SnowflakeStorageExtension::SnowflakeStorageExtension() {
	attach = SnowflakeAttach;
	create_transaction_manager = SnowflakeCreateTransactionManager;
}

} // namespace snowflake
} // namespace duckdb
