#include "snowflake_scan.hpp"
#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "snowflake_client_manager.hpp"
#include "snowflake_arrow_utils.hpp"
#include "snowflake_config.hpp"
#include "snowflake_secrets.hpp"
#include "snowflake_debug.hpp"
#include <cstdlib>

namespace duckdb {
namespace snowflake {

static unique_ptr<FunctionData> SnowflakeScanBind(ClientContext &context, TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types, vector<string> &names) {
	DPRINT("SnowflakeScanBind invoked\n");
	// Validate parameters
	if (input.inputs.size() < 2) {
		throw BinderException("snowflake_scan requires at least 2 parameters: query and profile");
	}

	// Get query and profile
	auto query = input.inputs[0].GetValue<string>();
	auto profile = input.inputs[1].GetValue<string>();

	// WORKAROUND for column reordering segfault:
	// If query has explicit column list (not SELECT *), we risk segfaults when DuckDB reorders
	// Convert "SELECT col1, col2 FROM ..." to "SELECT * FROM ..." to prevent crashes
	// Projection pushdown will still work - only needed columns are transferred
	std::string upper_query = StringUtil::Upper(query);
	std::string trimmed_query = query;
	StringUtil::Trim(trimmed_query);

	if (StringUtil::StartsWith(upper_query, "SELECT") && upper_query.find(" FROM ") != std::string::npos) {
		size_t select_pos = upper_query.find("SELECT") + 6;
		size_t from_pos = upper_query.find(" FROM ");
		std::string select_part = upper_query.substr(select_pos, from_pos - select_pos);
		StringUtil::Trim(select_part);

		// Check if it's NOT "SELECT *" or "SELECT DISTINCT *"
		if (select_part != "*" && select_part != "DISTINCT *") {
			// Has explicit column list - replace with SELECT * to prevent segfault
			std::string before_select = query.substr(0, query.find("SELECT") + 6);
			std::string after_from = query.substr(query.find(" FROM "));
			query = before_select + " * " + after_from;
			DPRINT("Auto-converted explicit column list to SELECT * to prevent reordering segfault\n");
			DPRINT("Modified query: %s\n", query.c_str());
		}
	}

	// Get config from profile
	SnowflakeConfig config;
	try {
		config = SnowflakeSecretsHelper::GetCredentials(context, profile);
	} catch (const std::exception &e) {
		throw BinderException("Failed to retrieve credentials for profile '%s': %s", profile.c_str(), e.what());
	}

	// Get client manager
	auto &client_manager = SnowflakeClientManager::GetInstance();

	shared_ptr<SnowflakeClient> connection;
	try {
		connection = client_manager.GetConnection(config);
	} catch (const std::exception &e) {
		throw BinderException("Unexpected error connecting to Snowflake with profile '%s': %s", profile.c_str(),
		                      e.what());
	}

	// Create the factory that will manage the ADBC connection and statement
	// This factory will be kept alive throughout the scan operation
	auto factory = make_uniq<SnowflakeArrowStreamFactory>(connection, query);

	// Create the bind data that inherits from ArrowScanFunctionData
	// This allows us to use DuckDB's native Arrow scan implementation
	auto bind_data = make_uniq<SnowflakeScanBindData>(std::move(factory));
	bind_data->projection_pushdown_enabled = true;

	// Check if filter pushdown is enabled (default to enabled for snowflake_scan)
	// This can be controlled via configuration or environment variables
	bool filter_pushdown_enabled = true;

	// Check for environment variable to disable pushdown for testing
	const char *pushdown_env = std::getenv("SNOWFLAKE_DISABLE_PUSHDOWN");
	if (pushdown_env && std::string(pushdown_env) == "true") {
		filter_pushdown_enabled = false;
		DPRINT("Pushdown disabled via SNOWFLAKE_DISABLE_PUSHDOWN environment variable\n");
	}

	DPRINT("Filter pushdown enabled: %s\n", filter_pushdown_enabled ? "true" : "false");

	// Get the schema from Snowflake using ADBC's ExecuteSchema
	// This executes the query with schema-only mode to get column information
	SnowflakeGetArrowSchema(reinterpret_cast<ArrowArrayStream *>(bind_data->factory.get()),
	                        bind_data->schema_root.arrow_schema);

	// Use DuckDB's Arrow integration to populate the table type information
	// This converts Arrow schema to DuckDB types and handles all type mappings
	ArrowTableFunction::PopulateArrowTableSchema(DBConfig::GetConfig(context), bind_data->arrow_table,
	                                             bind_data->schema_root.arrow_schema);
	names = bind_data->arrow_table.GetNames();
	return_types = bind_data->arrow_table.GetTypes();
	bind_data->all_types = return_types;

	// Set column names in the factory for query building
	bind_data->factory->SetColumnNames(names);

	// Set filter pushdown enabled flag
	bind_data->factory->SetFilterPushdownEnabled(filter_pushdown_enabled);

	DPRINT("SnowflakeScanBind returning bind data\n");
	return std::move(bind_data);
}

} // namespace snowflake

TableFunction GetSnowflakeScanFunction() {
	// Create a table function that uses DuckDB's native Arrow scan implementation
	// We only provide our own bind function to set up the Snowflake connection
	// All other operations (init_global, init_local, scan) use DuckDB's implementation
	TableFunction snowflake_scan("snowflake_scan", {LogicalType::VARCHAR, LogicalType::VARCHAR},
	                             ArrowTableFunction::ArrowScanFunction,   // Use DuckDB's scan
	                             snowflake::SnowflakeScanBind,            // Our bind function
	                             ArrowTableFunction::ArrowScanInitGlobal, // Use DuckDB's init
	                             ArrowTableFunction::ArrowScanInitLocal); // Use DuckDB's init

	snowflake_scan.projection_pushdown = true;
	snowflake_scan.filter_pushdown = true;

	return snowflake_scan;
}

} // namespace duckdb
