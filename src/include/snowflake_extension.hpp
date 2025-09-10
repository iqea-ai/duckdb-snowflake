#pragma once

#include "duckdb.hpp"
#include "duckdb/main/extension.hpp"

namespace duckdb {

class SnowflakeExtension : public Extension {
public:
	void Load(DuckDB &db);
	std::string Name();
	std::string Version() const;
};

} // namespace duckdb

extern "C" {
DUCKDB_EXTENSION_API void snowflake_init(duckdb::DatabaseInstance &db);
DUCKDB_EXTENSION_API const char *snowflake_version();
}
