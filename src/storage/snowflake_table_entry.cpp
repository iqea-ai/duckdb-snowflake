#include "storage/snowflake_table_entry.hpp"
#include "snowflake_debug.hpp"
#include "snowflake_client_manager.hpp"
#include "snowflake_scan.hpp"
#include "snowflake_arrow_utils.hpp"
#include "duckdb/storage/table_storage_info.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/function/table/arrow/arrow_duck_schema.hpp"

namespace duckdb {
namespace snowflake {

TableFunction SnowflakeTableEntry::GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) {
	DPRINT("SnowflakeTableEntry::GetScanFunction called for table %s.%s.%s\n", client->GetConfig().database.c_str(),
	       schema.name.c_str(), name.c_str());

	auto &config = client->GetConfig();
	string query = "SELECT * FROM " + config.database + "." + schema.name + "." + name;
	DPRINT("SnowflakeTableEntry: Query = '%s'\n", query.c_str());

	// TODO consider maintaining a thread-safe pool of connections in client, so we can use the client within
	// SnowflakeTableEntry instead of creating a new client
	auto &client_manager = SnowflakeClientManager::GetInstance();
	auto connection = client_manager.GetConnection(config);

	auto factory = make_uniq<SnowflakeArrowStreamFactory>(connection, query);
	DPRINT("SnowflakeTableEntry: Created factory at %p\n", (void *)factory.get());

	auto snowflake_bind_data = make_uniq<SnowflakeScanBindData>(std::move(factory));
	// TODO remove below line after implementing projection pushdown
	snowflake_bind_data->projection_pushdown_enabled = false;

	DPRINT("SnowflakeTableEntry: About to call SnowflakeGetArrowSchema\n");
	SnowflakeGetArrowSchema(reinterpret_cast<ArrowArrayStream *>(snowflake_bind_data->factory.get()),
	                        snowflake_bind_data->schema_root.arrow_schema);
	DPRINT("SnowflakeTableEntry: SnowflakeGetArrowSchema completed\n");

	// Use the new DuckDB API to populate the arrow table schema
	vector<string> names;
	vector<LogicalType> return_types;
        ArrowTableFunction::PopulateArrowTableSchema(DBConfig::GetConfig(context), snowflake_bind_data->arrow_table,
	                                           snowflake_bind_data->schema_root.arrow_schema);
	
	// Get the types and names from the populated ArrowTableSchema
	return_types = snowflake_bind_data->arrow_table.GetTypes();
	names = snowflake_bind_data->arrow_table.GetNames();
	snowflake_bind_data->all_types = return_types;

	// Populate columns if not already loaded (first time accessing this table)
	if (!columns_loaded) {
		for (idx_t i = 0; i < names.size(); i++) {
			DPRINT("  Column: %s, Type: %s\n", names[i].c_str(), return_types[i].ToString().c_str());
			columns.AddColumn(ColumnDefinition(names[i], return_types[i]));
		}
		columns_loaded = true;
	}

	DPRINT("SnowflakeTableEntry: Setting bind_data at %p\n", (void *)snowflake_bind_data.get());
	bind_data = std::move(snowflake_bind_data);

	DPRINT("SnowflakeTableEntry: Returning GetSnowflakeScanFunction\n");
	return GetSnowflakeScanFunction();
}

unique_ptr<BaseStatistics> SnowflakeTableEntry::GetStatistics(ClientContext &context, column_t column_id) {
	throw NotImplementedException("Snowflake does not support getting statistics for tables");
}

TableStorageInfo SnowflakeTableEntry::GetStorageInfo(ClientContext &context) {
	TableStorageInfo result;
	// Don't fetch row count to avoid ADBC statement conflicts
	// Snowflake is read-only, so exact cardinality isn't critical
	result.cardinality = 0;
	result.index_info = vector<IndexInfo>();
	return result;
}

} // namespace snowflake
} // namespace duckdb
