#include "storage/snowflake_table_set.hpp"
#include "storage/snowflake_table_entry.hpp"
#include "snowflake_client_manager.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"

namespace duckdb {
namespace snowflake {
void SnowflakeTableSet::LoadEntries(ClientContext &context) {
	auto lease = SnowflakeClientManager::GetInstance().Acquire(config);
	auto table_names = lease->ListTables(context, schema_name);

	for (const auto &table_name : table_names) {
		CreateTableInfo info(
		    QualifiedName(schema.catalog.GetName(), Identifier(schema_name), Identifier(table_name)));
		info.on_conflict = OnCreateConflict::IGNORE_ON_CONFLICT;
		info.temporary = false;

		auto table_entry = make_uniq<SnowflakeTableEntry>(schema.catalog, schema, info, config);

		entries[table_name] = std::move(table_entry);
	}
}
} // namespace snowflake
} // namespace duckdb
