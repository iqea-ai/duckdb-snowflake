#pragma once

#include "duckdb/storage/storage_extension.hpp"

namespace duckdb {
namespace snowflake {

class SnowflakeStorageExtension : public StorageExtension {
public:
	SnowflakeStorageExtension();
};

} // namespace snowflake
} // namespace duckdb
