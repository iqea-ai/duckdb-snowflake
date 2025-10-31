#pragma once

#include "snowflake_catalog_set.hpp"
#include "snowflake_client.hpp"
#include "snowflake_schema_entry.hpp"

namespace duckdb {
namespace snowflake {

//! SnowflakeTableSet represents a set of tables in Snowflake
class SnowflakeTableSet : public SnowflakeCatalogSet {
public:
	SnowflakeTableSet(SnowflakeSchemaEntry &schema, shared_ptr<SnowflakeClient> client, const string &schema_name)
	    : SnowflakeCatalogSet(schema.catalog), schema(schema), client(std::move(client)), schema_name(schema_name) {
	}

protected:
	//! Load tables for this schema
	void LoadEntries(ClientContext &context) override;

private:
	SnowflakeSchemaEntry &schema;
	shared_ptr<SnowflakeClient> client;
	const string schema_name;
};
} // namespace snowflake
} // namespace duckdb
