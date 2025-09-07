#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

void SnowflakeStoreCredentialsFunction(DataChunk &args, ExpressionState &state, Vector &result);
void SnowflakeListProfilesFunction(DataChunk &args, ExpressionState &state, Vector &result);
void SnowflakeValidateCredentialsFunction(DataChunk &args, ExpressionState &state, Vector &result);

ScalarFunction GetSnowflakeStoreCredentialsFunction();
ScalarFunction GetSnowflakeListProfilesFunction();
ScalarFunction GetSnowflakeValidateCredentialsFunction();

} // namespace duckdb
