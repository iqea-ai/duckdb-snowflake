#include "storage/snowflake_table_entry.hpp"
#include "storage/snowflake_catalog.hpp"
#include "snowflake_debug.hpp"
#include "snowflake_client_manager.hpp"
#include "snowflake_scan.hpp"
#include "snowflake_arrow_utils.hpp"
#include "snowflake_settings.hpp"
#include "duckdb/logging/logger.hpp"
#include "snowflake_query_builder.hpp"
#include "duckdb/common/arrow/nanoarrow/nanoarrow.h"
#include "duckdb/storage/table_storage_info.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/function/table/arrow/arrow_duck_schema.hpp"

namespace duckdb {
namespace snowflake {

//! Deep-copy a previously cached ArrowSchema into `dst`. Uses nanoarrow's
//! ArrowSchemaDeepCopy so `dst` ends up with its own release-callback-owned
//! memory, decoupled from the cached source. `src` is taken by non-const ref
//! because the nanoarrow C signature requires a mutable pointer even though
//! the operation only reads from it.
static void CloneCachedSchema(ArrowSchema &src, ArrowSchema &dst) {
	auto rc = duckdb_nanoarrow::ArrowSchemaDeepCopy(&src, &dst);
	if (rc != 0) {
		throw IOException("Failed to deep-copy cached Snowflake Arrow schema (nanoarrow rc=%d)", rc);
	}
}

TableFunction SnowflakeTableEntry::GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) {
	DPRINT("SnowflakeTableEntry::GetScanFunction called for table %s.%s.%s\n", GetConfig().database.c_str(),
	       schema.name.c_str(), name.c_str());

	auto &config = GetConfig();
	string query = "SELECT * FROM " + QuoteSnowflakeIdentifier(config.database) + "." +
	               QuoteSnowflakeIdentifier(schema.name) + "." + QuoteSnowflakeIdentifier(name);
	DPRINT("SnowflakeTableEntry: Query = '%s'\n", query.c_str());

	// Lease a dedicated connection from the pool for this scan; it is owned by the
	// factory and returned to the pool when the bound query is torn down. This is
	// what keeps concurrent scans from sharing one non-thread-safe ADBC connection.
	auto &client_manager = SnowflakeClientManager::GetInstance();
	auto lease = client_manager.Acquire(config);

	auto factory = make_uniq<SnowflakeArrowStreamFactory>(std::move(lease), query);
	DPRINT("SnowflakeTableEntry: Created factory at %p\n", (void *)factory.get());

	// Same snapshot as the snowflake_query() bind: an attached-catalog scan streams
	// its result through the same factory and has the same unbounded read-ahead.
	factory->driver_settings = GetDriverSettings(context);
	if (factory->driver_settings.AnySet()) {
		DUCKDB_LOG_DEBUG(context, "snowflake ATTACH scan: ADBC read-ahead %s", factory->driver_settings.ToLogString());
	}

	// Apply pushdown settings from catalog options
	auto &snowflake_catalog = catalog.Cast<SnowflakeCatalog>();
	const auto &catalog_options = snowflake_catalog.GetOptions();
	factory->filter_pushdown_enabled = catalog_options.enable_pushdown;
	factory->projection_pushdown_enabled = catalog_options.enable_pushdown;
	DPRINT("SnowflakeTableEntry: Pushdown %s (enable_pushdown=%s)\n",
	       catalog_options.enable_pushdown ? "ENABLED" : "DISABLED",
	       catalog_options.enable_pushdown ? "true" : "false");

	auto snowflake_bind_data = make_uniq<SnowflakeScanBindData>(std::move(factory));

	// Set pushdown settings on bind_data (critical for avoiding crashes!)
	snowflake_bind_data->projection_pushdown_enabled = catalog_options.enable_pushdown;

	vector<string> names;
	vector<LogicalType> return_types;

	// Serialize access to the schema cache and the lazy columns population.
	// DuckDB catalog entries are shared across connections, so two concurrent
	// GetScanFunction calls on the same SnowflakeTableEntry can race on the
	// unique_ptr reassignment (use-after-free of the old ArrowSchemaWrapper)
	// and on the columns_loaded flag. Holding bind_mutex for the whole block
	// also collapses two concurrent first-binders into one Snowflake roundtrip
	// rather than two.
	{
		std::lock_guard<std::mutex> lock(bind_mutex);

		// Populate bind_data->schema_root either from cache (no Snowflake roundtrip)
		// or by issuing the schema-probe query and seeding the cache.
		if (cached_schema_root && cached_schema_root->arrow_schema.release) {
			DPRINT("SnowflakeTableEntry: Reusing cached Arrow schema (no SF roundtrip)\n");
			CloneCachedSchema(cached_schema_root->arrow_schema, snowflake_bind_data->schema_root.arrow_schema);
		} else {
			DPRINT("SnowflakeTableEntry: About to call SnowflakeGetArrowSchemaViaQuery\n");
			// Use a 1-row query execution for the bind schema (data-path) so the
			// driver's geoarrow.wkb tags (applied by peeking the first batch) and
			// correct Snowflake TIMESTAMP units (issue #44) reach DuckDB.
			// ExecuteSchema metadata carries neither.
			SnowflakeGetArrowSchemaViaQuery(snowflake_bind_data->factory.get(),
			                                snowflake_bind_data->schema_root.arrow_schema);
			DPRINT("SnowflakeTableEntry: SnowflakeGetArrowSchemaViaQuery completed\n");

			// Seed the cache with a deep copy of the just-fetched schema so future
			// binds on this table entry can skip the Snowflake roundtrip.
			cached_schema_root = make_uniq<ArrowSchemaWrapper>();
			CloneCachedSchema(snowflake_bind_data->schema_root.arrow_schema, cached_schema_root->arrow_schema);
		}

		// Use the new DuckDB API to populate the arrow table schema
		ArrowTableFunction::PopulateArrowTableSchema(context, snowflake_bind_data->arrow_table,
		                                             snowflake_bind_data->schema_root.arrow_schema);
		names = snowflake_bind_data->arrow_table.GetNames();
		return_types = snowflake_bind_data->arrow_table.GetTypes();
		snowflake_bind_data->all_types = return_types;

		// Set column names on factory for filter building (maps column indices to
		// names)
		snowflake_bind_data->factory->column_names = names;

		// Populate columns if not already loaded (first time accessing this table)
		if (!columns_loaded) {
			for (idx_t i = 0; i < static_cast<idx_t>(names.size()); i++) {
				DPRINT("  Column: %s, Type: %s\n", names[i].c_str(), return_types[i].ToString().c_str());
				columns.AddColumn(ColumnDefinition(names[i], return_types[i]));
			}
			columns_loaded = true;
		}
	}

	DPRINT("SnowflakeTableEntry: Setting bind_data at %p\n", (void *)snowflake_bind_data.get());
	bind_data = std::move(snowflake_bind_data);

	DPRINT("SnowflakeTableEntry: Returning GetSnowflakeTableScanFunction "
	       "(pushdown %s)\n",
	       catalog_options.enable_pushdown ? "enabled" : "disabled");
	return GetSnowflakeTableScanFunction(catalog_options.enable_pushdown);
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
