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
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "duckdb/planner/filter/expression_filter.hpp"
#include "duckdb/planner/filter/table_filter_functions.hpp"
#include "duckdb/planner/table_filter_set.hpp"

namespace duckdb {
namespace snowflake {

string QuoteSnowflakeIdentifier(const string &name) {
	if (name.empty()) {
		return name;
	}
	auto is_first_ok = [](char c) {
		return (c >= 'A' && c <= 'Z') || c == '_';
	};
	auto is_rest_ok = [](char c) {
		return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$';
	};
	bool needs_quotes = !is_first_ok(name[0]);
	for (size_t i = 1; !needs_quotes && i < name.size(); ++i) {
		if (!is_rest_ok(name[i])) {
			needs_quotes = true;
		}
	}
	if (!needs_quotes) {
		return name;
	}
	string out;
	out.reserve(name.size() + 2);
	out += '"';
	for (char ch : name) {
		if (ch == '"') {
			out += '"';
		}
		out += ch;
	}
	out += '"';
	return out;
}

//! Split a dotted identifier path on '.' boundaries while keeping quoted
//! identifiers ("a.b.c") intact.
static vector<string> SplitDottedIdentifier(const string &name) {
	vector<string> out;
	string current;
	bool in_quotes = false;
	for (size_t i = 0; i < name.size(); ++i) {
		char ch = name[i];
		if (ch == '"') {
			// Doubled quote inside a quoted identifier is an escaped quote.
			if (in_quotes && i + 1 < name.size() && name[i + 1] == '"') {
				current += ch;
				current += name[i + 1];
				++i;
				continue;
			}
			in_quotes = !in_quotes;
			current += ch;
		} else if (ch == '.' && !in_quotes) {
			out.push_back(current);
			current.clear();
		} else {
			current += ch;
		}
	}
	out.push_back(current);
	return out;
}

//! If the part is wrapped in double quotes, strip the outer quotes and
//! collapse doubled internal "" back to a single ". Otherwise return as-is.
static string UnquoteSnowflakeIdentifier(const string &part) {
	if (part.size() < 2 || part.front() != '"' || part.back() != '"') {
		return part;
	}
	string out;
	out.reserve(part.size() - 2);
	for (size_t i = 1; i + 1 < part.size(); ++i) {
		if (part[i] == '"' && i + 2 < part.size() && part[i + 1] == '"') {
			out += '"';
			++i;
		} else {
			out += part[i];
		}
	}
	return out;
}

//! Re-quote each dotted segment of a (possibly already-quoted) identifier
//! path per Snowflake's rules.
static string RequoteDottedIdentifier(const string &name) {
	auto parts = SplitDottedIdentifier(name);
	for (auto &p : parts) {
		p = QuoteSnowflakeIdentifier(UnquoteSnowflakeIdentifier(p));
	}
	return StringUtil::Join(parts, ".");
}

string SnowflakeQueryBuilder::BuildQuery(const string &table_name, const vector<string> &projection_columns,
                                         TableFilterSet *filter_set, const vector<string> &column_names) {
	// Snowflake folds unquoted identifiers to uppercase. DuckDB's AST writer
	// emits lowercase identifiers without quotes (which DuckDB itself accepts
	// case-insensitively), so we can't trust it to produce a Snowflake-valid
	// FROM clause. Build the FROM clause manually with Snowflake-correct
	// quoting and substitute it into the AST output via a placeholder that
	// DuckDB will not rewrite. Column identifiers (projection list + filter
	// column refs) have the same problem; we route them through per-column
	// placeholders that get substituted with QuoteSnowflakeIdentifier output
	// after ToString().
	auto split_parts = SplitDottedIdentifier(table_name);
	if (split_parts.empty() || (split_parts.size() == 1 && split_parts[0].empty())) {
		throw InvalidInputException("Invalid table name format: %s", table_name);
	}

	// Build a placeholder for every column name we will reference. Placeholders
	// are uppercase/underscores so DuckDB's serializer leaves them alone, and
	// distinct enough that StringUtil::Replace won't collide.
	std::map<string, string> col_to_placeholder;
	auto placeholder_for = [&](const string &name) -> string {
		auto it = col_to_placeholder.find(name);
		if (it != col_to_placeholder.end()) {
			return it->second;
		}
		string ph = StringUtil::Format("__SF_COL_%llu__", (uint64_t)col_to_placeholder.size());
		col_to_placeholder[name] = ph;
		return ph;
	};

	vector<string> projection_placeholders;
	projection_placeholders.reserve(projection_columns.size());
	for (const auto &c : projection_columns) {
		projection_placeholders.push_back(placeholder_for(c));
	}

	vector<string> column_name_placeholders;
	column_name_placeholders.reserve(column_names.size());
	for (const auto &c : column_names) {
		column_name_placeholders.push_back(placeholder_for(c));
	}

	// Create a SelectStatement AST
	auto select_stmt = make_uniq<SelectStatement>();
	auto select_node = make_uniq<SelectNode>();

	// 1. Build a placeholder FROM clause; we substitute the SF-quoted form
	//    after AST serialization. The placeholder is all-uppercase / underscores
	//    so DuckDB will not wrap it in quotes.
	static const string from_placeholder = "__SNOWFLAKE_FROM_PLACEHOLDER__";
	auto table_ref = make_uniq<BaseTableRef>();
	table_ref->SetTable(Identifier(from_placeholder));
	select_node->from_table = std::move(table_ref);

	// 2. Build the SELECT clause (projection list) with placeholder column names
	auto projection_list = BuildProjectionList(projection_placeholders);
	if (!projection_list.empty()) {
		select_node->select_list = std::move(projection_list);
	}
	// If empty, SelectNode defaults to SELECT *

	// 3. Build the WHERE clause (filters) with placeholder column names
	auto where_expr = BuildWhereExpression(filter_set, column_name_placeholders);
	if (where_expr) {
		select_node->where_clause = std::move(where_expr);
	}

	// 4. Attach the select node to the statement
	select_stmt->node = std::move(select_node);

	// 5. Serialize AST to SQL string and patch in the SF-quoted FROM clause
	string sql = select_stmt->ToString();
	const string sf_from = RequoteDottedIdentifier(table_name);
	sql = StringUtil::Replace(sql, from_placeholder, sf_from);

	// 6. Replace each column placeholder with its Snowflake-quoted form
	for (const auto &kv : col_to_placeholder) {
		sql = StringUtil::Replace(sql, kv.second, QuoteSnowflakeIdentifier(kv.first));
	}
	return sql;
}

unique_ptr<ParsedExpression> SnowflakeQueryBuilder::BuildWhereExpression(TableFilterSet *filter_set,
                                                                         const vector<string> &column_names) {
	if (!filter_set || !filter_set->HasFilters()) {
		return nullptr;
	}

	vector<unique_ptr<ParsedExpression>> conditions;

	// Transform each filter to a ParsedExpression
	for (const auto &entry : *filter_set) {
		idx_t column_idx = entry.GetIndex().GetIndex();
		const auto &filter = entry.Filter();

		if (column_idx >= column_names.size()) {
			throw InternalException("Filter column index %llu out of range (have %llu columns)", column_idx,
			                        column_names.size());
		}

		const string &column_name = column_names[column_idx];
		auto &expr = ExpressionFilter::GetExpressionFilter(filter, "SnowflakeQueryBuilder::BuildWhereExpression").expr;
		auto condition = TransformFilterExpression(*expr, column_name);
		if (condition) {
			conditions.push_back(std::move(condition));
			continue;
		}
		// DuckDB removes a pushed filter from the plan and does not apply it again.
		// Only internal filters (optional, dynamic) may be dropped: they only prune rows.
		if (!IsInternalOnly(*expr)) {
			throw NotImplementedException("Filter '%s' on column '%s' is not supported for Snowflake pushdown. "
			                              "To use this query, disable pushdown with: ATTACH '' AS name (TYPE "
			                              "snowflake, ..., enable_pushdown false)",
			                              expr->ToString(), column_name);
		}
		DPRINT("Skipping internal filter on column '%s'\n", column_name.c_str());
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

static bool IsInternalFilterFunction(const Expression &expr) {
	return expr.GetExpressionClass() == ExpressionClass::BOUND_FUNCTION &&
	       TableFilterFunctions::IsTableFilterFunction(expr.Cast<BoundFunctionExpression>().Function());
}

bool SnowflakeQueryBuilder::IsInternalOnly(const Expression &expr) {
	if (IsInternalFilterFunction(expr)) {
		return true;
	}
	if (expr.GetExpressionType() != ExpressionType::CONJUNCTION_AND) {
		return false;
	}
	for (auto &child : expr.Cast<BoundConjunctionExpression>().GetChildren()) {
		if (!IsInternalOnly(*child)) {
			return false;
		}
	}
	return true;
}

//! The filter subject is the scanned column itself
static bool IsColumnSubject(const Expression &expr) {
	return expr.GetExpressionClass() == ExpressionClass::BOUND_REF ||
	       expr.GetExpressionClass() == ExpressionClass::BOUND_COLUMN_REF;
}

static unique_ptr<ParsedExpression> MakeColumnRef(const string &column_name) {
	return make_uniq<ColumnRefExpression>(Identifier(column_name));
}

static unique_ptr<ParsedExpression> CombineConditions(ExpressionType type,
                                                      vector<unique_ptr<ParsedExpression>> conditions) {
	auto result = std::move(conditions[0]);
	for (size_t i = 1; i < conditions.size(); i++) {
		result = make_uniq<ConjunctionExpression>(type, std::move(result), std::move(conditions[i]));
	}
	return result;
}

unique_ptr<ParsedExpression> SnowflakeQueryBuilder::TransformFilterExpression(const Expression &expr,
                                                                              const string &column_name) {
	if (BoundComparisonExpression::IsComparison(expr)) {
		auto &comparison = expr.Cast<BoundFunctionExpression>();
		auto comparison_type = comparison.GetExpressionType();
		switch (comparison_type) {
		case ExpressionType::COMPARE_EQUAL:
		case ExpressionType::COMPARE_NOTEQUAL:
		case ExpressionType::COMPARE_LESSTHAN:
		case ExpressionType::COMPARE_LESSTHANOREQUALTO:
		case ExpressionType::COMPARE_GREATERTHAN:
		case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
			break;
		default:
			return nullptr;
		}
		auto &left = BoundComparisonExpression::Left(comparison);
		auto &right = BoundComparisonExpression::Right(comparison);
		optional_ptr<const BoundConstantExpression> constant;
		if (IsColumnSubject(left) && right.GetExpressionClass() == ExpressionClass::BOUND_CONSTANT) {
			constant = right.Cast<BoundConstantExpression>();
		} else if (IsColumnSubject(right) && left.GetExpressionClass() == ExpressionClass::BOUND_CONSTANT) {
			constant = left.Cast<BoundConstantExpression>();
			comparison_type = FlipComparisonExpression(comparison_type);
		} else {
			return nullptr;
		}
		return make_uniq<ComparisonExpression>(comparison_type, MakeColumnRef(column_name),
		                                       ConstantExpression::FromValue(constant->GetValue()));
	}

	switch (expr.GetExpressionClass()) {
	case ExpressionClass::BOUND_CONJUNCTION: {
		auto &conjunction = expr.Cast<BoundConjunctionExpression>();
		bool is_and = conjunction.GetExpressionType() == ExpressionType::CONJUNCTION_AND;
		vector<unique_ptr<ParsedExpression>> conditions;
		for (auto &child : conjunction.GetChildren()) {
			auto condition = TransformFilterExpression(*child, column_name);
			if (condition) {
				conditions.push_back(std::move(condition));
			} else if (!is_and || !IsInternalOnly(*child)) {
				// Without this child the conjunction would select other rows
				return nullptr;
			}
		}
		if (conditions.empty()) {
			return nullptr;
		}
		return CombineConditions(conjunction.GetExpressionType(), std::move(conditions));
	}
	case ExpressionClass::BOUND_OPERATOR: {
		auto &op = expr.Cast<BoundOperatorExpression>();
		auto &children = op.GetChildren();
		if (children.empty() || !IsColumnSubject(*children[0])) {
			return nullptr;
		}
		switch (op.GetExpressionType()) {
		case ExpressionType::OPERATOR_IS_NULL:
		case ExpressionType::OPERATOR_IS_NOT_NULL:
			return make_uniq<OperatorExpression>(op.GetExpressionType(), MakeColumnRef(column_name));
		case ExpressionType::COMPARE_IN: {
			// Build as: (column = val1) OR (column = val2) OR ...
			vector<unique_ptr<ParsedExpression>> conditions;
			for (idx_t i = 1; i < children.size(); i++) {
				if (children[i]->GetExpressionClass() != ExpressionClass::BOUND_CONSTANT) {
					return nullptr;
				}
				auto &value = children[i]->Cast<BoundConstantExpression>().GetValue();
				conditions.push_back(make_uniq<ComparisonExpression>(
				    ExpressionType::COMPARE_EQUAL, MakeColumnRef(column_name), ConstantExpression::FromValue(value)));
			}
			if (conditions.empty()) {
				return nullptr;
			}
			return CombineConditions(ExpressionType::CONJUNCTION_OR, std::move(conditions));
		}
		default:
			return nullptr;
		}
	}
	case ExpressionClass::BOUND_FUNCTION: {
		auto &func = expr.Cast<BoundFunctionExpression>();
		auto &function_name = func.Function().GetName();
		if (function_name == OptionalFilterScalarFun::NAME && func.BindInfo()) {
			auto &data = func.BindInfo()->Cast<OptionalFilterFunctionData>();
			return data.child_filter_expr ? TransformFilterExpression(*data.child_filter_expr, column_name) : nullptr;
		}
		if (function_name == SelectivityOptionalFilterScalarFun::NAME && func.BindInfo()) {
			auto &data = func.BindInfo()->Cast<SelectivityOptionalFilterFunctionData>();
			return data.child_filter_expr ? TransformFilterExpression(*data.child_filter_expr, column_name) : nullptr;
		}
		if (function_name == DynamicFilterScalarFun::NAME && func.BindInfo() && !func.GetChildren().empty()) {
			// Runtime filter from a join: push its value once the join has set it
			auto &data = func.BindInfo()->Cast<DynamicFilterFunctionData>();
			if (!data.filter_data || !data.filter_data->initialized) {
				return nullptr;
			}
			auto comparison = data.filter_data->ToExpression(*func.GetChildren()[0]);
			return TransformFilterExpression(*comparison, column_name);
		}
		return nullptr;
	}
	default:
		return nullptr;
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
		result.push_back(MakeColumnRef(col));
	}

	return result;
}

string RenderPushdownQueryForTest(const string &table_name, const string &projection_csv,
                                  const string &filter_eq_column) {
	vector<string> projection_columns;
	if (!projection_csv.empty()) {
		auto parts = StringUtil::Split(projection_csv, ",");
		for (auto &part : parts) {
			StringUtil::Trim(part);
			if (!part.empty()) {
				projection_columns.push_back(part);
			}
		}
	}

	TableFilterSet filter_set;
	TableFilterSet *filter_ptr = nullptr;
	if (!filter_eq_column.empty()) {
		if (projection_columns.empty()) {
			throw InvalidInputException(
			    "RenderPushdownQueryForTest: filter_eq_column requires a non-empty projection_csv");
		}
		idx_t found = projection_columns.size();
		for (idx_t i = 0; i < projection_columns.size(); ++i) {
			if (projection_columns[i] == filter_eq_column) {
				found = i;
				break;
			}
		}
		if (found == projection_columns.size()) {
			throw InvalidInputException("RenderPushdownQueryForTest: filter_eq_column '%s' not in projection list",
			                            filter_eq_column);
		}
		filter_set.PushFilter(ProjectionIndex(found), ExpressionFilter::CreateComparisonFilter(
		                                                  ExpressionType::COMPARE_EQUAL, Value("test_value")));
		filter_ptr = &filter_set;
	}

	return SnowflakeQueryBuilder::BuildQuery(table_name, projection_columns, filter_ptr, projection_columns);
}

} // namespace snowflake
} // namespace duckdb
