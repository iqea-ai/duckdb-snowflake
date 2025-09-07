#include "duckdb/common/helper.hpp"
#define DUCKDB_EXTENSION_MAIN

#include "snowflake_extension.hpp"
// #include "snowflake_attach.hpp"
#include "storage/snowflake_storage.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "duckdb/function/table_function.hpp"
#include "snowflake_functions.hpp"
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

static void LoadInternal(ExtensionLoader &loader) {
	// Register the custom Snowflake secret type
	RegisterSnowflakeSecretType(loader.GetDatabaseInstance());

	// Register snowflake_version function
	auto snowflake_version_function =
	    ScalarFunction("snowflake_version", {}, LogicalType::VARCHAR, SnowflakeVersionScalarFun);
	loader.RegisterFunction(snowflake_version_function);

	// Register the snowflake_store_credentials scalar function
	auto store_credentials_func = GetSnowflakeStoreCredentialsFunction();
	loader.RegisterFunction(store_credentials_func);

	// Register the snowflake_list_profiles function
	auto list_profiles_func = GetSnowflakeListProfilesFunction();
	loader.RegisterFunction(list_profiles_func);

	// Register the snowflake_validate_credentials function
	auto validate_credentials_func = GetSnowflakeValidateCredentialsFunction();
	loader.RegisterFunction(validate_credentials_func);

#ifdef ADBC_AVAILABLE
	// Register snowflake_scan table function (only available when ADBC is available)
	auto snowflake_scan_function = GetSnowflakeScanFunction();
	loader.RegisterFunction(snowflake_scan_function);

	// Register storage extension (only available when ADBC is available)
	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
	config.storage_extensions["snowflake"] = make_uniq<snowflake::SnowflakeStorageExtension>();
#else
	// ADBC not available - register a placeholder function that throws an error
	auto snowflake_scan_function =
	    TableFunction("snowflake_scan", {}, [](ClientContext &context, TableFunctionInput &data, DataChunk &output) {
		    throw NotImplementedException(
		        "snowflake_scan is not available on this platform (ADBC driver not supported)");
	    });
	loader.RegisterFunction(snowflake_scan_function);
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

DUCKDB_CPP_EXTENSION_ENTRY(snowflake, loader) {
	duckdb::LoadInternal(loader);
}

DUCKDB_EXTENSION_API const char *snowflake_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
