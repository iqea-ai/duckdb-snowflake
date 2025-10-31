#include "storage/snowflake_table_set.hpp"
#include "storage/snowflake_table_entry.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"

namespace duckdb {
namespace snowflake {
void SnowflakeTableSet::LoadEntries(ClientContext &context) {
  auto table_names = client->ListTables(context, schema_name);

  for (const auto &table_name : table_names) {
    CreateTableInfo info;
    info.table = table_name;
    info.schema = schema_name;
    info.catalog = schema.catalog.GetName();
    info.on_conflict = OnCreateConflict::IGNORE_ON_CONFLICT;
    info.temporary = false;

    auto table_entry =
        make_uniq<SnowflakeTableEntry>(schema.catalog, schema, info, client);

    entries[table_name] = std::move(table_entry);
  }
}
} // namespace snowflake
} // namespace duckdb
