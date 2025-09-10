#pragma once

#include "duckdb.hpp"
#include "duckdb/main/extension.hpp"

namespace duckdb {

class SnowflakeExtension : public Extension {
public:
	void Load(DuckDB &db) override;
	std::string Name() override;
	std::string Version() const override;
};

} // namespace duckdb

extern "C" {
DUCKDB_EXTENSION_API void snowflake_init(duckdb::DatabaseInstance &db);
DUCKDB_EXTENSION_API const char *snowflake_version();
}
