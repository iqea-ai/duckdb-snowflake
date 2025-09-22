#include "snowflake_query_builder.hpp"
#include "snowflake_debug.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/date.hpp"
#include "duckdb/common/types/time.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/filter/in_filter.hpp"
#include "duckdb/planner/filter/optional_filter.hpp"

namespace duckdb {
namespace snowflake {

std::string SnowflakeQueryBuilder::BuildWhereClause(const std::vector<TableFilter> &filters,
                                                   const std::vector<std::string> &column_names) {
	if (filters.empty()) {
		return "";
	}

	std::vector<std::string> conditions;
	conditions.reserve(filters.size());

	for (size_t i = 0; i < filters.size(); i++) {
		const auto &filter = filters[i];
		if (i >= column_names.size()) {
			DPRINT("Warning: Filter index %zu exceeds column names size %zu\n", i, column_names.size());
			continue;
		}

		try {
			std::string condition = TransformFilter(filter, column_names[i]);
			if (!condition.empty()) {
				conditions.push_back(condition);
			}
		} catch (const std::exception &e) {
			DPRINT("Warning: Failed to transform filter %zu: %s\n", i, e.what());
			// Continue with other filters - graceful fallback
		}
	}

	if (conditions.empty()) {
		DPRINT("No valid filter conditions could be generated\n");
		return "";
	}

	// Manual string joining since StringUtil::Join has type issues
	std::string result = "WHERE ";
	for (size_t i = 0; i < conditions.size(); i++) {
		if (i > 0) {
			result += " AND ";
		}
		result += conditions[i];
	}
	return result;
}

std::string SnowflakeQueryBuilder::BuildWhereClause(TableFilterSet *filter_set,
                                                   const std::vector<std::string> &column_names) {
	if (!filter_set || filter_set->filters.empty()) {
		return "";
	}

	std::vector<std::string> conditions;
	conditions.reserve(filter_set->filters.size());

	for (const auto &filter_entry : filter_set->filters) {
		idx_t column_index = filter_entry.first;
		const auto &filter = filter_entry.second;

		if (column_index >= column_names.size()) {
			DPRINT("Warning: Filter column index %llu exceeds column names size %zu\n", column_index, column_names.size());
			continue;
		}

		try {
			std::string condition = TransformFilter(*filter, column_names[column_index]);
			if (!condition.empty()) {
				conditions.push_back(condition);
			}
		} catch (const std::exception &e) {
			DPRINT("Warning: Failed to transform filter for column %llu: %s\n", column_index, e.what());
			// Continue with other filters - graceful fallback
		}
	}

	if (conditions.empty()) {
		DPRINT("No valid filter conditions could be generated\n");
		return "";
	}

	// Manual string joining since StringUtil::Join has type issues
	std::string result = "WHERE ";
	for (size_t i = 0; i < conditions.size(); i++) {
		if (i > 0) {
			result += " AND ";
		}
		result += conditions[i];
	}
	return result;
}

std::string SnowflakeQueryBuilder::BuildSelectClause(const std::vector<std::string> &projection_columns,
                                                    const std::vector<std::string> &all_columns) {
	if (projection_columns.empty()) {
		return "";
	}

	std::vector<std::string> escaped_columns;
	escaped_columns.reserve(projection_columns.size());

	for (const auto &column : projection_columns) {
		escaped_columns.push_back(EscapeSqlIdentifier(column));
	}

	// Manual string joining since StringUtil::Join has type issues
	std::string result;
	for (size_t i = 0; i < escaped_columns.size(); i++) {
		if (i > 0) {
			result += ", ";
		}
		result += escaped_columns[i];
	}
	return result;
}

std::string SnowflakeQueryBuilder::ModifyQuery(const std::string &original_query,
                                              const std::string &select_clause,
                                              const std::string &where_clause) {
	// For simple queries like "SELECT * FROM table", we can do basic modifications
	// This is a simplified implementation - in practice, we'd need proper SQL parsing

	if (select_clause.empty() && where_clause.empty()) {
		return original_query;
	}

	std::string modified_query = original_query;
	bool query_modified = false;

	// If we have a WHERE clause, append it to the query
	if (!where_clause.empty()) {
		// Simple approach: if the query doesn't already have WHERE, add it
		if (StringUtil::Upper(modified_query).find("WHERE") == std::string::npos) {
			modified_query += " " + where_clause;
			query_modified = true;
			DPRINT("Added WHERE clause: %s\n", where_clause.c_str());
		}
	}

	// If we have a SELECT clause modification, replace SELECT * with specific columns
	if (!select_clause.empty()) {
		DPRINT("SELECT clause modification requested: '%s'\n", select_clause.c_str());

		// Simple approach: replace "SELECT *" with "SELECT column_list"
		std::string upper_query = StringUtil::Upper(modified_query);
		size_t select_pos = upper_query.find("SELECT");
		if (select_pos != std::string::npos) {
			size_t from_pos = upper_query.find(" FROM ", select_pos);
			if (from_pos != std::string::npos) {
				// Extract the part before SELECT and after FROM
				std::string before_select = modified_query.substr(0, select_pos);
				std::string after_from = modified_query.substr(from_pos);

				// Reconstruct with specific columns
				modified_query = before_select + "SELECT " + select_clause + after_from;
				query_modified = true;
				DPRINT("Modified SELECT clause: '%s'\n", select_clause.c_str());
			}
		}
	}

	if (query_modified) {
		DPRINT("Query pushdown applied - original: '%s', modified: '%s'\n",
		       original_query.c_str(), modified_query.c_str());
	} else {
		DPRINT("No query modifications applied\n");
	}

	return modified_query;
}

std::string SnowflakeQueryBuilder::TransformFilter(const TableFilter &filter, const std::string &column_name) {
	switch (filter.filter_type) {
		case TableFilterType::CONSTANT_COMPARISON:
			return TransformConstantFilter(filter, column_name);
		case TableFilterType::IS_NULL:
			return EscapeSqlIdentifier(column_name) + " IS NULL";
		case TableFilterType::IS_NOT_NULL:
			return EscapeSqlIdentifier(column_name) + " IS NOT NULL";
		case TableFilterType::CONJUNCTION_AND:
			return TransformConjunctionFilter(filter, column_name);
		case TableFilterType::CONJUNCTION_OR:
			return TransformConjunctionFilter(filter, column_name);
		case TableFilterType::IN_FILTER:
			return TransformInFilter(filter, column_name);
		case TableFilterType::OPTIONAL_FILTER:
			// OPTIONAL_FILTER wraps another filter - unwrap it and process the inner filter
			try {
				const auto &optional_filter = filter.Cast<OptionalFilter>();
				return TransformFilter(*optional_filter.child_filter, column_name);
			} catch (const std::exception &e) {
				DPRINT("Failed to unwrap OPTIONAL_FILTER: %s\n", e.what());
				return "";
			}
		default:
			DPRINT("Unsupported filter type: %d\n", static_cast<int>(filter.filter_type));
			return "";
	}
}

std::string SnowflakeQueryBuilder::TransformConstantFilter(const TableFilter &filter, const std::string &column_name) {
	const auto &constant_filter = filter.Cast<ConstantFilter>();
	const std::string &escaped_column = EscapeSqlIdentifier(column_name);
	const std::string &value_literal = ValueToSqlLiteral(constant_filter.constant);

	switch (constant_filter.comparison_type) {
		case ExpressionType::COMPARE_EQUAL:
			return escaped_column + " = " + value_literal;
		case ExpressionType::COMPARE_NOTEQUAL:
			return escaped_column + " != " + value_literal;
		case ExpressionType::COMPARE_LESSTHAN:
			return escaped_column + " < " + value_literal;
		case ExpressionType::COMPARE_GREATERTHAN:
			return escaped_column + " > " + value_literal;
		case ExpressionType::COMPARE_LESSTHANOREQUALTO:
			return escaped_column + " <= " + value_literal;
		case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
			return escaped_column + " >= " + value_literal;
		case ExpressionType::COMPARE_DISTINCT_FROM:
			// Snowflake uses IS DISTINCT FROM
			return escaped_column + " IS DISTINCT FROM " + value_literal;
		case ExpressionType::COMPARE_NOT_DISTINCT_FROM:
			// Snowflake uses IS NOT DISTINCT FROM
			return escaped_column + " IS NOT DISTINCT FROM " + value_literal;
		default:
			DPRINT("Unsupported comparison type: %d\n", static_cast<int>(constant_filter.comparison_type));
			return "";
	}
}

std::string SnowflakeQueryBuilder::TransformConjunctionFilter(const TableFilter &filter, const std::string &column_name) {
	if (filter.filter_type == TableFilterType::CONJUNCTION_AND) {
		const auto &and_filter = filter.Cast<ConjunctionAndFilter>();
		std::vector<std::string> conditions;
		conditions.reserve(and_filter.child_filters.size());

		for (const auto &child_filter : and_filter.child_filters) {
			std::string condition = TransformFilter(*child_filter, column_name);
			if (!condition.empty()) {
				conditions.push_back(condition);
			}
		}

		if (conditions.empty()) {
			return "";
		}

		if (conditions.size() == 1) {
			return conditions[0];
		}

		std::string result = "(";
		for (size_t i = 0; i < conditions.size(); i++) {
			if (i > 0) {
				result += " AND ";
			}
			result += conditions[i];
		}
		result += ")";
		return result;
	} else if (filter.filter_type == TableFilterType::CONJUNCTION_OR) {
		const auto &or_filter = filter.Cast<ConjunctionOrFilter>();
		std::vector<std::string> conditions;
		conditions.reserve(or_filter.child_filters.size());

		for (const auto &child_filter : or_filter.child_filters) {
			std::string condition = TransformFilter(*child_filter, column_name);
			if (!condition.empty()) {
				conditions.push_back(condition);
			}
		}

		if (conditions.empty()) {
			return "";
		}

		if (conditions.size() == 1) {
			return conditions[0];
		}

		std::string result = "(";
		for (size_t i = 0; i < conditions.size(); i++) {
			if (i > 0) {
				result += " OR ";
			}
			result += conditions[i];
		}
		result += ")";
		return result;
	}

	DPRINT("TransformConjunctionFilter: Unsupported conjunction type %d\n", static_cast<int>(filter.filter_type));
	return "";
}

std::string SnowflakeQueryBuilder::TransformInFilter(const TableFilter &filter, const std::string &column_name) {
	const auto &in_filter = filter.Cast<InFilter>();
	const std::string &escaped_column = EscapeSqlIdentifier(column_name);

	if (in_filter.values.empty()) {
		DPRINT("TransformInFilter: Empty IN filter values\n");
		return "";
	}

	std::string result = escaped_column + " IN (";
	for (size_t i = 0; i < in_filter.values.size(); i++) {
		if (i > 0) {
			result += ", ";
		}
		result += ValueToSqlLiteral(in_filter.values[i]);
	}
	result += ")";

	return result;
}

std::string SnowflakeQueryBuilder::TransformRangeFilter(const TableFilter &filter, const std::string &column_name) {
	// Range filters are typically implemented as conjunction of two constant filters
	// This is handled by TransformConjunctionFilter, so this method is not needed
	// but kept for future extensibility
	DPRINT("TransformRangeFilter: column=%s (handled by conjunction filters)\n", column_name.c_str());
	return "";
}

std::string SnowflakeQueryBuilder::TransformLikeFilter(const TableFilter &filter, const std::string &column_name) {
	// LIKE filters are typically implemented as constant filters with LIKE comparison
	// This would need to be handled in TransformConstantFilter for COMPARE_LIKE
	// but DuckDB doesn't have a direct LIKE filter type in TableFilter
	DPRINT("TransformLikeFilter: column=%s (not directly supported in TableFilter)\n", column_name.c_str());
	return "";
}

std::string SnowflakeQueryBuilder::ValueToSqlLiteral(const Value &value) {
	if (value.IsNull()) {
		return "NULL";
	}

	switch (value.type().id()) {
		case LogicalTypeId::BOOLEAN:
			return value.GetValue<bool>() ? "TRUE" : "FALSE";
		case LogicalTypeId::TINYINT:
		case LogicalTypeId::SMALLINT:
		case LogicalTypeId::INTEGER:
		case LogicalTypeId::BIGINT:
			return std::to_string(value.GetValue<int64_t>());
		case LogicalTypeId::UTINYINT:
		case LogicalTypeId::USMALLINT:
		case LogicalTypeId::UINTEGER:
		case LogicalTypeId::UBIGINT:
			return std::to_string(value.GetValue<uint64_t>());
		case LogicalTypeId::FLOAT:
		case LogicalTypeId::DOUBLE:
			return std::to_string(value.GetValue<double>());
		case LogicalTypeId::VARCHAR:
		case LogicalTypeId::CHAR:
			return EscapeSqlLiteral(value.GetValue<std::string>());
		case LogicalTypeId::DATE: {
			// Format as 'YYYY-MM-DD' using proper date formatting
			date_t date_val = value.GetValue<date_t>();
			int32_t year, month, day;
			Date::Convert(date_val, year, month, day);
			return StringUtil::Format("'%04d-%02d-%02d'", year, month, day);
		}
		case LogicalTypeId::TIMESTAMP: {
			// Format as 'YYYY-MM-DD HH:MM:SS' using proper timestamp formatting
			timestamp_t ts_val = value.GetValue<timestamp_t>();
			date_t date_val = Timestamp::GetDate(ts_val);
			dtime_t time_val = Timestamp::GetTime(ts_val);

			int32_t year, month, day;
			Date::Convert(date_val, year, month, day);

			int32_t hour, min, sec, microsec;
			Time::Convert(time_val, hour, min, sec, microsec);

			return StringUtil::Format("'%04d-%02d-%02d %02d:%02d:%02d'", year, month, day, hour, min, sec);
		}
		case LogicalTypeId::TIMESTAMP_TZ: {
			// For timestamp with timezone, use the timestamp value directly
			// Snowflake will handle the timezone conversion
			timestamp_t ts_val = value.GetValue<timestamp_t>();
			date_t date_val = Timestamp::GetDate(ts_val);
			dtime_t time_val = Timestamp::GetTime(ts_val);

			int32_t year, month, day;
			Date::Convert(date_val, year, month, day);

			int32_t hour, min, sec, microsec;
			Time::Convert(time_val, hour, min, sec, microsec);

			return StringUtil::Format("'%04d-%02d-%02d %02d:%02d:%02d'", year, month, day, hour, min, sec);
		}
		case LogicalTypeId::TIME: {
			// Format as 'HH:MM:SS'
			dtime_t time_val = value.GetValue<dtime_t>();
			int32_t hour, min, sec, microsec;
			Time::Convert(time_val, hour, min, sec, microsec);
			return StringUtil::Format("'%02d:%02d:%02d'", hour, min, sec);
		}
		case LogicalTypeId::DECIMAL: {
			// Handle decimal types - convert to string representation
			return value.ToString();
		}
		default:
			DPRINT("Unsupported value type for SQL literal: %s\n", value.type().ToString().c_str());
			return "NULL";
	}
}

std::string SnowflakeQueryBuilder::EscapeSqlIdentifier(const std::string &identifier) {
	// Basic escaping - wrap in double quotes
	return "\"" + identifier + "\"";
}

std::string SnowflakeQueryBuilder::EscapeSqlLiteral(const std::string &literal) {
	// Basic escaping - replace single quotes with double single quotes
	std::string escaped = literal;
	StringUtil::Replace(escaped, "'", "''");
	return "'" + escaped + "'";
}

} // namespace snowflake
} // namespace duckdb
