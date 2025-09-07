#pragma once

#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "snowflake_config.hpp"
#include "snowflake_client.hpp"

namespace duckdb {
namespace snowflake {

//! SnowflakeTableBindData contains metadata for a snowflake table and informs the scan function the structure of the
//! data it should receive
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
	SnowflakeTableEntry(Catalog &catalog, SchemaCatalogEntry &schema, CreateTableInfo &info,
	                    shared_ptr<SnowflakeClient> client)
	    : TableCatalogEntry(catalog, schema, info), client(std::move(client)) {};

	string GetFullyQualifiedName() const {
		return catalog.GetName() + "." + schema.name + "." + name;
	}

	const SnowflakeConfig &GetConfig() const {
		return client->GetConfig();
	}

	TableFunction GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) override;

	unique_ptr<BaseStatistics> GetStatistics(ClientContext &context, column_t column_id) override;

	TableStorageInfo GetStorageInfo(ClientContext &context) override;

private:
	shared_ptr<SnowflakeClient> client;
	bool columns_loaded = false;
};
} // namespace snowflake
} // namespace duckdb
