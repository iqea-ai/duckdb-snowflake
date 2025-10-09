#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/function/table/arrow/arrow_duck_schema.hpp"
#include "duckdb/common/string_util.hpp"

#include <string>
#include <vector>

namespace duckdb {
namespace snowflake {

// Query builder for translating DuckDB filters and projections to Snowflake SQL
class SnowflakeQueryBuilder {
public:
	// Transform DuckDB table filters to Snowflake WHERE clause
	static std::string BuildWhereClause(const std::vector<TableFilter> &filters,
	                                    const std::vector<std::string> &column_names);

	// Transform DuckDB table filter set to Snowflake WHERE clause
	static std::string BuildWhereClause(TableFilterSet *filter_set, const std::vector<std::string> &column_names);

	// Transform DuckDB projection to Snowflake SELECT clause
	static std::string BuildSelectClause(const std::vector<std::string> &projection_columns,
	                                     const std::vector<std::string> &all_columns);

	// Combine original query with pushdown modifications
	static std::string ModifyQuery(const std::string &original_query, const std::string &select_clause,
	                               const std::string &where_clause);

private:
	// Transform individual filter to SQL condition
	static std::string TransformFilter(const TableFilter &filter, const std::string &column_name);

	// Transform constant filter
	static std::string TransformConstantFilter(const TableFilter &filter, const std::string &column_name);

	// Transform conjunction filter (AND/OR)
	static std::string TransformConjunctionFilter(const TableFilter &filter, const std::string &column_name);

	// Transform IN filter
	static std::string TransformInFilter(const TableFilter &filter, const std::string &column_name);

	// Transform range filter (BETWEEN)
	static std::string TransformRangeFilter(const TableFilter &filter, const std::string &column_name);

	// Transform LIKE filter
	static std::string TransformLikeFilter(const TableFilter &filter, const std::string &column_name);

	// Convert DuckDB value to SQL literal
	static std::string ValueToSqlLiteral(const Value &value);

	// Escape SQL identifiers and literals
	static std::string EscapeSqlIdentifier(const std::string &identifier);
	static std::string EscapeSqlLiteral(const std::string &literal);

	// Helper functions for enhanced query modification
	static bool IsValidSimpleSelectQuery(const std::string &query);
	static bool HasWhereClause(const std::string &query);
	static bool HasSelectStar(const std::string &query);
	static std::string ReplaceSelectStar(const std::string &query, const std::string &select_clause);
};

} // namespace snowflake
} // namespace duckdb
