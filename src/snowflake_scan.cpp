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
#include "snowflake_settings.hpp"
#include "snowflake_debug.hpp"
#include "duckdb/logging/logger.hpp"

namespace duckdb {
namespace snowflake {

// Return the query with leading whitespace and SQL comments removed, for
// classification only — the statement we actually send keeps its comments, so
// query tags from dbt and similar tools still reach Snowflake's query history.
// Without this, a leading comment makes the first token "/*" or "--", the
// statement misses its execution path, and a projected scan reads the wrong
// columns (the #48/#59 failure mode, reachable from an ordinary SELECT).
// Block comments do NOT nest in Snowflake: the first "*/" ends the comment
// (verified — a nested-looking comment is a syntax error server-side), so the
// scan here must terminate at the first "*/" to classify what Snowflake parses.
static std::string StripLeadingCommentsForClassification(const std::string &sql) {
	size_t i = 0;
	while (i < sql.size()) {
		if (std::isspace(static_cast<unsigned char>(sql[i]))) {
			i++;
		} else if (sql.compare(i, 2, "--") == 0) {
			auto newline = sql.find('\n', i);
			if (newline == std::string::npos) {
				return ""; // comment runs to end of input: nothing to classify
			}
			i = newline + 1;
		} else if (sql.compare(i, 2, "/*") == 0) {
			auto close = sql.find("*/", i + 2);
			if (close == std::string::npos) {
				return ""; // unterminated comment: let Snowflake report the syntax error
			}
			i = close + 2;
		} else {
			break;
		}
	}
	return sql.substr(i);
}

static unique_ptr<FunctionData> SnowflakeScanBind(ClientContext &context, TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types, vector<string> &names) {
	DPRINT("SnowflakeScanBind invoked\n");
	// Validate parameters
	if (input.inputs.size() < 2) {
		throw BinderException("snowflake_query requires at least 2 parameters: query and profile");
	}

	// Get query and profile
	auto query = input.inputs[0].GetValue<string>();
	auto profile = input.inputs[1].GetValue<string>();

	// Get config from profile
	SnowflakeConfig config;
	try {
		config = SnowflakeSecretsHelper::GetCredentials(context, profile);
	} catch (const std::exception &e) {
		throw BinderException("Failed to retrieve credentials for profile '%s': %s", profile.c_str(), e.what());
	}

	// Get client manager
	auto &client_manager = SnowflakeClientManager::GetInstance();

	ConnectionLease lease;
	try {
		lease = client_manager.Acquire(config);
	} catch (const std::exception &e) {
		throw BinderException("Unexpected error connecting to Snowflake with profile '%s': %s", profile.c_str(),
		                      e.what());
	}

	// Create the factory that will manage the ADBC connection and statement.
	// The factory owns the leased connection for the whole scan operation.
	auto factory = make_uniq<SnowflakeArrowStreamFactory>(std::move(lease), query);

	// Snapshot the driver read-ahead settings here, while we still have a
	// ClientContext. The scan production path runs as a bare function pointer from
	// DuckDB's arrow scanner and gets no context, so this is the last place they can
	// be read (issue #66).
	factory->driver_settings = GetDriverSettings(context);
	if (factory->driver_settings.AnySet()) {
		DUCKDB_LOG_DEBUG(context, "snowflake_query: ADBC read-ahead %s", factory->driver_settings.ToLogString());
	}

	// Create the bind data that inherits from ArrowScanFunctionData
	// This allows us to use DuckDB's native Arrow scan implementation
	auto bind_data = make_uniq<SnowflakeScanBindData>(std::move(factory));

	// Filters stay client-side: the user controls the query, so we never rewrite
	// its WHERE clause.
	bind_data->factory->filter_pushdown_enabled = false;

	// Determine whether the passthrough query is a row-returning SELECT (projectable)
	// or a DDL/DML statement (CREATE/INSERT/...), which returns a status result and
	// is never column-pruned.
	auto trimmed = StripLeadingCommentsForClassification(query);
	StringUtil::LTrim(trimmed);
	auto upper = StringUtil::Upper(trimmed);
	bool is_select = StringUtil::StartsWith(upper, "SELECT") || StringUtil::StartsWith(upper, "WITH") ||
	                 StringUtil::StartsWith(upper, "(");
	// Row-returning metadata statements, matched on the first keyword (not a raw
	// prefix, so an unrelated statement starting with the same letters can't
	// false-match). Every one of these returns rows but cannot be wrapped as a
	// subquery, and every one is re-readable through RESULT_SCAN, so they all
	// need the re-target below — issue #48 applied to LIST/EXPLAIN/CALL exactly
	// as it did to SHOW/DESC.
	auto first_token = upper.substr(0, upper.find_first_of(" \t\n\r("));
	bool is_metadata_rows = first_token == "SHOW" || first_token == "DESC" || first_token == "DESCRIBE" ||
	                        first_token == "LIST" || first_token == "LS" || first_token == "EXPLAIN" ||
	                        first_token == "CALL";

	if (is_select) {
		// SELECT path: DuckDB's Arrow scanner prunes columns and reads the produced
		// stream POSITIONALLY (child[k] == k-th projected column). Returning the full
		// result regardless of projection caused wrong-column reads / SIGSEGV (#32).
		// So enable projection pushdown and let SnowflakeProduceArrowScan wrap the
		// query as a projected subquery. Fetch the schema cheaply with LIMIT 0
		// (data-path schema) rather than caching the full result.
		bind_data->factory->projection_pushdown_enabled = true;
		bind_data->factory->wrap_as_subquery = true;
		bind_data->projection_pushdown_enabled = true;
		SnowflakeGetArrowSchemaViaQuery(bind_data->factory.get(), bind_data->schema_root.arrow_schema);
	} else if (is_metadata_rows) {
		// These return rows but cannot be wrapped as a subquery, so the SELECT
		// path's projection wrap can't apply to the raw statement — and the cached
		// full-width stream misaligns DuckDB's positional reads on non-prefix
		// projections (issue #48; prefix projections only worked by accident).
		// Instead: execute the statement now, then re-target the scan at
		// TABLE(RESULT_SCAN('<query id>')), which IS wrappable, and rejoin the
		// SELECT path machinery. The factory's exclusive lease guarantees
		// LAST_QUERY_ID() sees our statement.
		auto query_id = SnowflakeExecuteAndGetQueryId(bind_data->factory.get());
		auto result_scan_query = "SELECT * FROM TABLE(RESULT_SCAN('" + query_id + "'))";
		bind_data->factory->query = result_scan_query;
		bind_data->factory->modified_query = result_scan_query;
		bind_data->factory->projection_pushdown_enabled = true;
		bind_data->factory->wrap_as_subquery = true;
		bind_data->projection_pushdown_enabled = true;
		SnowflakeGetArrowSchemaViaQuery(bind_data->factory.get(), bind_data->schema_root.arrow_schema);
	} else {
		// DDL/DML path: execute now and cache the stream. ExecuteSchema would leave
		// the ADBC driver unable to create a second statement (SIGSEGV at produce).
		//
		// The cached stream is full width and cannot be re-projected, so DuckDB must
		// not prune columns for this scan. With pruning on, DuckDB maps projected
		// output positions onto Arrow children positionally, so a non-prefix
		// projection reads the wrong column — silently when the types happen to
		// match, which is the common case for multi-column DML results (projecting
		// `output_bytes` off a COPY INTO returned `rows_unloaded`).
		//
		// Clearing projection_pushdown on the bind's *copy* of the TableFunction
		// disables pruning for this statement only: the binder copies the function
		// into the plan after bind returns, so the registered function is untouched
		// and other snowflake_query() calls still push projections down. DuckDB then
		// scans all columns and applies the projection above the scan.
		//
		// projection_pushdown_enabled stays false so DuckDB clears column_ids and
		// maps output position -> Arrow child positionally. That identity map is
		// only correct because the line above guarantees the scan is full width;
		// the two settings must be changed together.
		input.table_function.projection_pushdown = false;
		bind_data->factory->projection_pushdown_enabled = false;
		bind_data->projection_pushdown_enabled = false;
		SnowflakeExecuteAndCacheStream(bind_data->factory.get(), bind_data->schema_root.arrow_schema);
	}

	// Use DuckDB's Arrow integration to populate the table type information
	// This converts Arrow schema to DuckDB types and handles all type mappings
	ArrowTableFunction::PopulateArrowTableSchema(context, bind_data->arrow_table, bind_data->schema_root.arrow_schema);
	names = bind_data->arrow_table.GetNames();
	return_types = bind_data->arrow_table.GetTypes();
	bind_data->all_types = return_types;

	DPRINT("SnowflakeScanBind returning bind data\n");
	return std::move(bind_data);
}

} // namespace snowflake

TableFunction GetSnowflakeScanFunction() {
	// Create a table function that uses DuckDB's native Arrow scan implementation
	// We only provide our own bind function to set up the Snowflake connection
	// All other operations (init_global, init_local, scan) use DuckDB's
	// implementation Parameters: query (VARCHAR), profile (VARCHAR)
	TableFunction snowflake_query("snowflake_query", {LogicalType::VARCHAR, LogicalType::VARCHAR},
	                              ArrowTableFunction::ArrowScanFunction,   // Use DuckDB's scan
	                              snowflake::SnowflakeScanBind,            // Our bind function
	                              ArrowTableFunction::ArrowScanInitGlobal, // Use DuckDB's init
	                              ArrowTableFunction::ArrowScanInitLocal); // Use DuckDB's init

	// Enable projection pushdown so DuckDB's Arrow scanner prunes columns. This is
	// required in DuckDB v1.5+ where the Arrow scanner reads the produced stream
	// positionally (child[k] == k-th projected column). SnowflakeScanBind decides
	// per query how to honor that: a SELECT is wrapped as a projected subquery so
	// the produced stream's columns match the projection exactly (issue #32), while
	// DDL/DML keeps the full cached stream (it is never column-pruned).
	snowflake_query.projection_pushdown = true;
	snowflake_query.filter_pushdown = false;

	return snowflake_query;
}

TableFunction GetSnowflakeTableScanFunction(bool enable_pushdown) {
	// Create a table function for ATTACH
	// This function is used by SnowflakeTableEntry::GetScanFunction()
	// The enable_pushdown parameter controls whether DuckDB can push filters and
	// projections

	// Create a parameterless function for ATTACH operations
	// TableEntry provides bind_data directly, so we don't need parameters or a
	// bind function
	TableFunction table_scan("snowflake_table_scan", {},
	                         ArrowTableFunction::ArrowScanFunction,   // Use DuckDB's scan
	                         nullptr,                                 // No bind function needed
	                         ArrowTableFunction::ArrowScanInitGlobal, // Use DuckDB's init
	                         ArrowTableFunction::ArrowScanInitLocal); // Use DuckDB's init

	// Set pushdown flags based on the enable_pushdown parameter
	table_scan.projection_pushdown = enable_pushdown;
	table_scan.filter_pushdown = enable_pushdown;

	return table_scan;
}

} // namespace duckdb
