#include "storage/snowflake_schema_set.hpp"
#include "storage/snowflake_table_set.hpp"
#include "snowflake_debug.hpp"

namespace duckdb {
namespace snowflake {
void SnowflakeSchemaSet::LoadEntries(ClientContext &context) {
	DPRINT("SnowflakeSchemaSet::LoadEntries called\n");
	vector<string> schema_names = client->ListSchemas(context);
	DPRINT("Got %zu schemas from ListSchemas\n", schema_names.size());

	for (const auto &schema_name : schema_names) {
		DPRINT("Creating schema entry for: %s\n", schema_name.c_str());
		auto schema_info = make_uniq<CreateSchemaInfo>();
		schema_info->schema = schema_name;
		auto schema_entry = make_uniq<SnowflakeSchemaEntry>(catalog, schema_name, *schema_info, client);
		entries.insert({schema_name, std::move(schema_entry)});
	}
	DPRINT("SnowflakeSchemaSet::LoadEntries completed with %zu entries\n", entries.size());
}
} // namespace snowflake
} // namespace duckdb
