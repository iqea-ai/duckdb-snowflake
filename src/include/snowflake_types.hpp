#pragma once

#include "duckdb/common/types.hpp"

namespace duckdb {
namespace snowflake {
LogicalType SnowflakeTypeToLogicalType(const std::string &snowflake_type_str);
// string LogicalTypeToSnowflakeType(const LogicalType& type);
LogicalType ConvertNumber(uint8_t precision, uint8_t scale);
} // namespace snowflake
} // namespace duckdb
