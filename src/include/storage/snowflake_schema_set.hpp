#pragma once

#include "snowflake_catalog_set.hpp"
#include "snowflake_schema_entry.hpp"
#include "snowflake_client.hpp"

namespace duckdb {
namespace snowflake {
class SnowflakeSchemaSet : public SnowflakeCatalogSet {
public:
	SnowflakeSchemaSet(Catalog &catalog, shared_ptr<SnowflakeClient> client)
	    : SnowflakeCatalogSet(catalog), client(std::move(client)) {
	}

	//! Fetches all schemas from Snowflake and creates SnowflakeSchemaEntry
	//! objects for each
	void LoadEntries(ClientContext &context) override;

private:
	shared_ptr<SnowflakeClient> client;
};
} // namespace snowflake
} // namespace duckdb
