#include "duckdb/common/helper.hpp"
#define DUCKDB_EXTENSION_MAIN

#include "snowflake_extension.hpp"
// #include "snowflake_attach.hpp"
#include "storage/snowflake_storage.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <duckdb/parser/parsed_data/create_table_function_info.hpp>
#include "duckdb/function/table_function.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_transaction.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/common/vector_operations/ternary_executor.hpp"
#include "snowflake_secret_provider.hpp"
#include "snowflake_query_builder.hpp"
#include "snowflake_client_manager.hpp"

namespace duckdb {

// Forward declarations
TableFunction GetSnowflakeScanFunction();
void RegisterSnowflakeSecretType(DatabaseInstance &instance);

inline void SnowflakeVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);
	auto val = Value("Snowflake Extension v0.5.2");
	result.SetValue(0, val);
}

//! Test-only: exposes SnowflakeQueryBuilder::BuildQuery as a scalar so
//! query_builder.test can assert Snowflake-correct identifier quoting on the
//! rendered SQL without needing a Snowflake table with lowercase columns.
inline void SnowflakeRenderPushdownQueryFun(DataChunk &args, ExpressionState &state, Vector &result) {
	TernaryExecutor::Execute<string_t, string_t, string_t, string_t>(
	    args.data[0], args.data[1], args.data[2], result, args.size(),
	    [&](string_t table, string_t projection_csv, string_t filter_eq_column) {
		    auto rendered = snowflake::RenderPushdownQueryForTest(table.GetString(), projection_csv.GetString(),
		                                                          filter_eq_column.GetString());
		    return StringVector::AddString(result, rendered);
	    });
}

// Compatibility layer for different DuckDB versions
static void LoadInternal(ExtensionLoader &loader) {
	// Register the custom Snowflake secret type
	RegisterSnowflakeSecretType(loader.GetDatabaseInstance());

	// Drain pooled ADBC connections when this database closes, not at process
	// exit (issue #69). Keyed per database: each DatabaseInstance has its own
	// object cache, and re-loading into the same database just replaces it.
	loader.GetDatabaseInstance().GetObjectCache().Put(snowflake::SnowflakePoolCloseGuard::ObjectType(),
	                                                  make_shared_ptr<snowflake::SnowflakePoolCloseGuard>());

	// Register snowflake_version function using DuckDB 1.4 API
	auto snowflake_version_function =
	    ScalarFunction("snowflake_version", {}, LogicalType::VARCHAR, SnowflakeVersionScalarFun);
	loader.RegisterFunction(std::move(snowflake_version_function));

	// Test-only: render the SQL that pushdown would emit for a given table +
	// projection + (optional) single equality filter. Used by query_builder.test.
	auto snowflake_render_pushdown_query_function = ScalarFunction(
	    "snowflake_render_pushdown_query", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR},
	    LogicalType::VARCHAR, SnowflakeRenderPushdownQueryFun);
	loader.RegisterFunction(std::move(snowflake_render_pushdown_query_function));

#ifdef ADBC_AVAILABLE
	// Register snowflake_scan table function (only available when ADBC is
	// available)
	auto snowflake_scan_function = GetSnowflakeScanFunction();
	loader.RegisterFunction(std::move(snowflake_scan_function));

	// Register storage extension (only available when ADBC is available)
	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
	StorageExtension::Register(config, "snowflake", make_shared_ptr<snowflake::SnowflakeStorageExtension>());
#else
	// ADBC not available - register a placeholder function that throws an error
	auto snowflake_scan_function =
	    TableFunction("snowflake_scan", {}, [](ClientContext &context, TableFunctionInput &data, DataChunk &output) {
		    throw NotImplementedException("snowflake_scan is not available on this "
		                                  "platform (ADBC driver not supported)");
	    });
	loader.RegisterFunction(std::move(snowflake_scan_function));
#endif
}

void SnowflakeExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string SnowflakeExtension::Name() {
	return "snowflake";
}

std::string SnowflakeExtension::Version() const {
#ifdef EXT_VERSION_SNOWFLAKE
	return EXT_VERSION_SNOWFLAKE;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void snowflake_init(duckdb::DatabaseInstance &db) {
	duckdb::ExtensionLoader loader(db, "snowflake");
	duckdb::LoadInternal(loader);
}

DUCKDB_EXTENSION_API const char *snowflake_version() {
	return duckdb::DuckDB::LibraryVersion();
}

// C++ extension entry point for loadable extensions
#ifdef DUCKDB_BUILD_LOADABLE_EXTENSION
extern "C" {
DUCKDB_CPP_EXTENSION_ENTRY(snowflake, loader) {
	duckdb::LoadInternal(loader);
}
}
#endif
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
