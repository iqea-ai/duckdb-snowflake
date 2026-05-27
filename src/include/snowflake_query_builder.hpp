#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/storage/table/scan_state.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/parsed_expression.hpp"

namespace duckdb {
namespace snowflake {

//! SnowflakeQueryBuilder: AST-based query construction for filter and
//! projection pushdown
//!
//! This class builds SQL queries by constructing a DuckDB AST programmatically
//! from DuckDB's pre-parsed filters and projections, then serializing to SQL.
//!
//! Architecture:
//!   User Query → DuckDB Parser → DuckDB Optimizer → Extracts
//!   filters/projections
//!                                                  ↓
//!   We receive: TableFilterSet (pre-parsed!) + vector<string> (column names)
//!                                                  ↓
//!   We build AST: SelectStatement → SelectNode → TableRef + Filters +
//!   Projections
//!                                                  ↓
//!   We serialize: AST.ToString() → "SELECT cols FROM table WHERE conditions"
//!
//! Benefits over string manipulation:
//!   1. Type-safe construction
//!   2. Automatic escaping and formatting
//!   3. Leverages DuckDB's SQL serialization logic
class SnowflakeQueryBuilder {
public:
	//! Build a complete SELECT query using AST construction
	//! Input:
	//!   - table_name: Qualified table name (e.g., "database.schema.table")
	//!   - projection_columns: Columns to select (empty = SELECT *)
	//!   - filter_set: DuckDB's pre-parsed filters
	//!   - column_names: Maps column indices to names
	//! Output: SQL string serialized from AST
	static string BuildQuery(const string &table_name, const vector<string> &projection_columns,
	                         TableFilterSet *filter_set, const vector<string> &column_names);

private:
	//! Build WHERE clause expression from DuckDB filters
	//! Returns nullptr if no filters
	static unique_ptr<ParsedExpression> BuildWhereExpression(TableFilterSet *filter_set,
	                                                         const vector<string> &column_names);

	//! Transform a single DuckDB TableFilter to ParsedExpression
	static unique_ptr<ParsedExpression> TransformFilter(const TableFilter &filter, const string &column_name);

	//! Build projection list (SELECT clause expressions)
	//! Returns empty vector for SELECT *
	static vector<unique_ptr<ParsedExpression>> BuildProjectionList(const vector<string> &projection_columns);
};

//! Quote a Snowflake identifier when its characters or case would otherwise
//! make Snowflake's parser fold it to uppercase or reject it. Snowflake's
//! unquoted-identifier rules: first char A-Z or _, subsequent A-Z, 0-9, _, $.
//! Anything else (including lowercase) gets wrapped in double quotes with
//! internal quotes escaped by doubling.
string QuoteSnowflakeIdentifier(const string &name);

} // namespace snowflake
} // namespace duckdb
