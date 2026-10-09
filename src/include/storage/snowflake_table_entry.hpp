#pragma once

#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "snowflake_config.hpp"
#include "snowflake_client.hpp"

#include <mutex>

namespace duckdb {
namespace snowflake {

//! SnowflakeTableBindData contains metadata for a snowflake table and informs
//! the scan function the structure of the data it should receive
struct SnowflakeTableBindData : public FunctionData {
	string database_name;
	string schema_name;
	string table_name;

	vector<string> column_names;
	vector<LogicalType> column_types;

	SnowflakeConfig config;

	unique_ptr<FunctionData> Copy() const override {
		auto result = make_uniq<SnowflakeTableBindData>();
		result->database_name = database_name;
		result->schema_name = schema_name;
		result->table_name = table_name;
		result->column_names = column_names;
		result->column_types = column_types;
		result->config = config;
		return result;
	}

	bool Equals(const FunctionData &other) const override {
		auto &other_data = static_cast<const SnowflakeTableBindData &>(other);
		return (database_name == other_data.database_name && schema_name == other_data.schema_name &&
		        table_name == other_data.table_name && column_names == other_data.column_names &&
		        column_types == other_data.column_types && config == other_data.config);
	}
};

//! SnowflakeTableEntry represents a single table in Snowflake
class SnowflakeTableEntry : public TableCatalogEntry {
public:
	SnowflakeTableEntry(Catalog &catalog, SchemaCatalogEntry &schema, CreateTableInfo &info, SnowflakeConfig config)
	    : TableCatalogEntry(catalog, schema, info), config(std::move(config)) {};

	string GetFullyQualifiedName() const {
		return catalog.GetName() + "." + schema.name + "." + name;
	}

	const SnowflakeConfig &GetConfig() const {
		return config;
	}

	TableFunction GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) override;

	unique_ptr<BaseStatistics> GetStatistics(ClientContext &context, column_t column_id) override;

	TableStorageInfo GetStorageInfo(ClientContext &context) override;

	//! Snowflake tables have no rowid, and the Arrow scan cannot produce one.
	//! The inherited default advertises a rowid virtual column, which DuckDB then
	//! picks for queries that reference no columns (count(*), SELECT 1 FROM t):
	//! with pushdown off the planner rejects it, with pushdown on it renders an
	//! empty SELECT list (issue #73). Advertising none makes DuckDB fall back to
	//! column 0, a real column the scan always produces.
	virtual_column_map_t GetVirtualColumns() const override;

private:
	SnowflakeConfig config;
	//! Serializes access to the schema cache and the lazy columns load below.
	//! DuckDB shares catalog table entries across connections; concurrent
	//! GetScanFunction calls on the same entry would otherwise race on the
	//! unique_ptr reassignment (use-after-free) and on columns_loaded.
	std::mutex bind_mutex;
	bool columns_loaded = false;
	//! Cached Arrow schema bytes from the first SnowflakeGetArrowSchema call.
	//! Subsequent GetScanFunction binds deep-copy out of this instead of paying
	//! another Snowflake roundtrip — see issue #33 (CREATE VIEW latency).
	//! Guarded by bind_mutex above.
	unique_ptr<ArrowSchemaWrapper> cached_schema_root;
};
} // namespace snowflake
} // namespace duckdb
