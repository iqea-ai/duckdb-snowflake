#include "snowflake_query_builder.hpp"
#include "snowflake_debug.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/conjunction_expression.hpp"
#include "duckdb/parser/expression/operator_expression.hpp"
#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/filter/optional_filter.hpp"
#include "duckdb/planner/filter/in_filter.hpp"
#include "duckdb/planner/filter/dynamic_filter.hpp"

namespace duckdb {
namespace snowflake {

string SnowflakeQueryBuilder::BuildQuery(const string &table_name, const vector<string> &projection_columns,
                                         TableFilterSet *filter_set, const vector<string> &column_names) {
	// Create a SelectStatement AST
	auto select_stmt = make_uniq<SelectStatement>();
	auto select_node = make_uniq<SelectNode>();

	// 1. Build the FROM clause (table reference)
	auto table_parts = StringUtil::Split(table_name, '.');
	auto table_ref = make_uniq<BaseTableRef>();

	// Assign parts from right to left: table <- schema <- catalog
	size_t n = table_parts.size();
	if (n == 0) {
		throw InvalidInputException("Invalid table name format: %s", table_name);
	}

	table_ref->table_name = table_parts[n - 1];
	if (n >= 2) {
		table_ref->schema_name = table_parts[n - 2];
	}
	if (n >= 3) {
		table_ref->catalog_name = table_parts[n - 3];
	}

	select_node->from_table = std::move(table_ref);

	// 2. Build the SELECT clause (projection list)
	auto projection_list = BuildProjectionList(projection_columns);
	if (!projection_list.empty()) {
		select_node->select_list = std::move(projection_list);
	}
	// If empty, SelectNode defaults to SELECT *

	// 3. Build the WHERE clause (filters)
	auto where_expr = BuildWhereExpression(filter_set, column_names);
	if (where_expr) {
		select_node->where_clause = std::move(where_expr);
	}

	// 4. Attach the select node to the statement
	select_stmt->node = std::move(select_node);

	// 5. Serialize AST to SQL string
	return select_stmt->ToString();
}

unique_ptr<ParsedExpression> SnowflakeQueryBuilder::BuildWhereExpression(TableFilterSet *filter_set,
                                                                         const vector<string> &column_names) {
	if (!filter_set || filter_set->filters.empty()) {
		return nullptr;
	}

	vector<unique_ptr<ParsedExpression>> conditions;

	// Transform each filter to a ParsedExpression
	for (const auto &filter_pair : filter_set->filters) {
		idx_t column_idx = filter_pair.first;
		const auto &filter = filter_pair.second;

		if (column_idx >= column_names.size()) {
			throw InternalException("Filter column index %llu out of range (have %llu columns)", column_idx,
			                        column_names.size());
		}

		string column_name = column_names[column_idx];
		auto condition = TransformFilter(*filter, column_name);
		// Note: TransformFilter returns nullptr for filters that should be skipped (e.g., uninitialized DYNAMIC_FILTER)
		// These filters will be applied by DuckDB locally after fetching data
		if (condition) {
			conditions.push_back(std::move(condition));
		}
	}

	// Combine conditions with AND
	if (conditions.empty()) {
		return nullptr;
	} else if (conditions.size() == 1) {
		return std::move(conditions[0]);
	} else {
		// Create AND conjunction of all conditions
		auto result = std::move(conditions[0]);
		for (size_t i = 1; i < conditions.size(); i++) {
			result = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_AND, std::move(result),
			                                          std::move(conditions[i]));
		}
		return result;
	}
}

unique_ptr<ParsedExpression> SnowflakeQueryBuilder::TransformFilter(const TableFilter &filter,
                                                                    const string &column_name) {
	// Create column reference
	auto column_ref = make_uniq<ColumnRefExpression>(column_name);

	switch (filter.filter_type) {
	case TableFilterType::CONSTANT_COMPARISON: {
		auto &const_filter = filter.Cast<ConstantFilter>();

		// Map comparison type to expression type
		ExpressionType comparison_type;
		switch (const_filter.comparison_type) {
		case ExpressionType::COMPARE_EQUAL:
		case ExpressionType::COMPARE_NOTEQUAL:
		case ExpressionType::COMPARE_LESSTHAN:
		case ExpressionType::COMPARE_LESSTHANOREQUALTO:
		case ExpressionType::COMPARE_GREATERTHAN:
		case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
			comparison_type = const_filter.comparison_type;
			break;
		default:
			throw NotImplementedException("Comparison type not supported for pushdown: %s",
			                              ExpressionTypeToString(const_filter.comparison_type));
		}

		// Create constant value expression
		auto constant = make_uniq<ConstantExpression>(const_filter.constant);

		// Create comparison expression
		return make_uniq<ComparisonExpression>(comparison_type, std::move(column_ref), std::move(constant));
	}

	case TableFilterType::IS_NULL: {
		// column IS NULL
		return make_uniq<OperatorExpression>(ExpressionType::OPERATOR_IS_NULL, std::move(column_ref));
	}

	case TableFilterType::IS_NOT_NULL: {
		// column IS NOT NULL
		return make_uniq<OperatorExpression>(ExpressionType::OPERATOR_IS_NOT_NULL, std::move(column_ref));
	}

	case TableFilterType::CONJUNCTION_AND: {
		// Handle AND of multiple filters (e.g., date >= X AND date < Y)
		auto &conj_filter = filter.Cast<ConjunctionAndFilter>();

		if (conj_filter.child_filters.empty()) {
			throw InternalException("CONJUNCTION_AND filter has no child filters for column '%s'",
			                        column_name.c_str());
		}

		// Recursively transform all child filters
		// Skip only unpushable filters (like uninitialized DYNAMIC_FILTERs)
		// Push whatever we can, let DuckDB handle the rest
		vector<unique_ptr<ParsedExpression>> conditions;
		for (const auto &child : conj_filter.child_filters) {
			auto condition = TransformFilter(*child, column_name);
			if (condition) {
				// This filter can be pushed
				conditions.push_back(std::move(condition));
			} else {
				// This filter can't be pushed (e.g., uninitialized DYNAMIC_FILTER)
				// Skip it - DuckDB will apply it locally
				DPRINT("Skipping unpushable child filter in CONJUNCTION_AND on column '%s'\n", column_name.c_str());
			}
		}

		// If no conditions could be pushed, return nullptr
		if (conditions.empty()) {
			DPRINT("No pushable conditions in CONJUNCTION_AND for column '%s'\n", column_name.c_str());
			return nullptr;
		}

		// If only one condition, return it directly
		if (conditions.size() == 1) {
			return std::move(conditions[0]);
		}

		// Combine all conditions with AND
		auto result = std::move(conditions[0]);
		for (size_t i = 1; i < conditions.size(); i++) {
			result = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_AND, std::move(result),
			                                          std::move(conditions[i]));
		}
		return result;
	}

	case TableFilterType::OPTIONAL_FILTER: {
		// Unwrap optional filter and process the inner filter
		auto &opt_filter = filter.Cast<OptionalFilter>();
		if (!opt_filter.child_filter) {
			throw InternalException("OPTIONAL_FILTER has no child filter for column '%s'", column_name.c_str());
		}
		auto result = TransformFilter(*opt_filter.child_filter, column_name);
		// Optional filters can return nullptr (e.g., uninitialized DYNAMIC_FILTER)
		// This is acceptable - DuckDB will apply the filter locally if needed
		return result;
	}

	case TableFilterType::IN_FILTER: {
		// Handle IN clause: column IN (val1, val2, ...)
		auto &in_filter = filter.Cast<InFilter>();

		if (in_filter.values.empty()) {
			throw InternalException("IN_FILTER has no values for column '%s'", column_name.c_str());
		}

		// Build as: (column = val1) OR (column = val2) OR ...
		vector<unique_ptr<ParsedExpression>> conditions;
		for (const auto &value : in_filter.values) {
			auto col_ref = make_uniq<ColumnRefExpression>(column_name);
			auto constant = make_uniq<ConstantExpression>(value);
			auto comparison = make_uniq<ComparisonExpression>(ExpressionType::COMPARE_EQUAL,
			                                                   std::move(col_ref), std::move(constant));
			conditions.push_back(std::move(comparison));
		}

		// Combine with OR
		auto result = std::move(conditions[0]);
		for (size_t i = 1; i < conditions.size(); i++) {
			result = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_OR, std::move(result),
			                                          std::move(conditions[i]));
		}
		return result;
	}

	case TableFilterType::CONJUNCTION_OR: {
		// Handle OR of multiple filters (e.g., col = 1 OR col = 2)
		auto &conj_filter = filter.Cast<ConjunctionOrFilter>();

		if (conj_filter.child_filters.empty()) {
			throw InternalException("CONJUNCTION_OR filter has no child filters for column '%s'",
			                        column_name.c_str());
		}

		// Recursively transform all child filters
		// For OR, we can still push partial filters, but the semantics might change
		// So we only push if ALL filters in the OR can be pushed
		vector<unique_ptr<ParsedExpression>> conditions;
		for (const auto &child : conj_filter.child_filters) {
			auto condition = TransformFilter(*child, column_name);
			if (!condition) {
				// For OR, if any child can't be pushed, we skip the entire OR
				// This preserves correctness - partial OR pushdown could change semantics
				DPRINT("Skipping CONJUNCTION_OR on column '%s' - not all children pushable\n", column_name.c_str());
				return nullptr;
			}
			conditions.push_back(std::move(condition));
		}

		// If only one condition, return it directly
		if (conditions.size() == 1) {
			return std::move(conditions[0]);
		}

		// Combine all conditions with OR
		auto result = std::move(conditions[0]);
		for (size_t i = 1; i < conditions.size(); i++) {
			result = make_uniq<ConjunctionExpression>(ExpressionType::CONJUNCTION_OR, std::move(result),
			                                          std::move(conditions[i]));
		}
		return result;
	}

	case TableFilterType::DYNAMIC_FILTER: {
		// Handle DYNAMIC_FILTER: runtime-generated filters from join optimization
		// These filters are created by DuckDB during join execution with concrete values
		auto &dyn_filter = filter.Cast<DynamicFilter>();

		// Check if the dynamic filter has been initialized with a concrete value
		if (dyn_filter.filter_data && dyn_filter.filter_data->initialized && dyn_filter.filter_data->filter) {
			// Extract the underlying ConstantFilter and transform it
			DPRINT("DYNAMIC_FILTER initialized for column '%s', unwrapping to ConstantFilter\n", column_name.c_str());
			return TransformFilter(*dyn_filter.filter_data->filter, column_name);
		}

		// If not initialized yet, skip this filter - DuckDB will apply it locally after fetching data
		// This happens when the filter value hasn't been determined yet during query planning
		DPRINT("DYNAMIC_FILTER not yet initialized for column '%s', skipping pushdown\n", column_name.c_str());
		return nullptr;
	}

	default:
		throw NotImplementedException(
		    "Filter type %d on column '%s' is not supported for Snowflake pushdown. "
		    "Supported types: CONSTANT_COMPARISON, IS_NULL, IS_NOT_NULL, CONJUNCTION_AND, "
		    "CONJUNCTION_OR, OPTIONAL_FILTER, IN_FILTER, DYNAMIC_FILTER. "
		    "To use this query, disable pushdown with: ATTACH '' AS name (TYPE snowflake, ..., enable_pushdown false)",
		    (int)filter.filter_type, column_name.c_str());
	}
}

vector<unique_ptr<ParsedExpression>>
SnowflakeQueryBuilder::BuildProjectionList(const vector<string> &projection_columns) {
	vector<unique_ptr<ParsedExpression>> result;

	if (projection_columns.empty()) {
		// SELECT * - return empty list, SelectNode will handle this
		return result;
	}

	// Create ColumnRefExpression for each projected column
	for (const auto &col : projection_columns) {
		result.push_back(make_uniq<ColumnRefExpression>(col));
	}

	return result;
}

} // namespace snowflake
} // namespace duckdb
