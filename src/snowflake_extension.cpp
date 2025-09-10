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
#include "snowflake_secret_provider.hpp"

namespace duckdb {

// Forward declarations
TableFunction GetSnowflakeScanFunction();
void RegisterSnowflakeSecretType(DatabaseInstance &instance);

inline void SnowflakeVersionScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);
	auto val = Value("Snowflake Extension v0.1.0");
	result.SetValue(0, val);
}

// Compatibility layer for different DuckDB versions
static void LoadInternal(DuckDB &db) {
	// Register the custom Snowflake secret type
	RegisterSnowflakeSecretType(*db.instance);

	// Register snowflake_version function using DuckDB 1.3.2 API
	auto snowflake_version_function =
	    ScalarFunction("snowflake_version", {}, LogicalType::VARCHAR, SnowflakeVersionScalarFun);
	
	// Register functions using the catalog
	auto &catalog = Catalog::GetSystemCatalog(*db.instance);
	auto transaction = CatalogTransaction::GetSystemTransaction(*db.instance);
	
	CreateScalarFunctionInfo version_info(std::move(snowflake_version_function));
	version_info.internal = true;
	catalog.CreateFunction(transaction, version_info);

#ifdef ADBC_AVAILABLE
	// Register snowflake_scan table function (only available when ADBC is available)
	auto snowflake_scan_function = GetSnowflakeScanFunction();
	CreateTableFunctionInfo scan_info(std::move(snowflake_scan_function));
	scan_info.internal = true;
	catalog.CreateFunction(transaction, scan_info);

	// Register storage extension (only available when ADBC is available)
	auto &config = DBConfig::GetConfig(*db.instance);
	config.storage_extensions["snowflake"] = make_uniq<snowflake::SnowflakeStorageExtension>();
#else
	// ADBC not available - register a placeholder function that throws an error
	auto snowflake_scan_function =
	    TableFunction("snowflake_scan", {}, [](ClientContext &context, TableFunctionInput &data, DataChunk &output) {
		    throw NotImplementedException(
		        "snowflake_scan is not available on this platform (ADBC driver not supported)");
	    });
	CreateTableFunctionInfo scan_info(std::move(snowflake_scan_function));
	scan_info.internal = true;
	catalog.CreateFunction(transaction, scan_info);
#endif
}

void SnowflakeExtension::Load(DuckDB &db) {
	LoadInternal(db);
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
	duckdb::DuckDB db_wrapper(db);
	duckdb::LoadInternal(db_wrapper);
}

DUCKDB_EXTENSION_API const char *snowflake_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
