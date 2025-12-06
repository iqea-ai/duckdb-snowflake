#include "snowflake_optimizer_extension.hpp"
#include "snowflake_scan.hpp"
#include "snowflake_debug.hpp"
#include "duckdb/planner/operator/logical_limit.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
#include "duckdb/planner/operator/logical_aggregate.hpp"
#include "duckdb/planner/operator/logical_projection.hpp"
#include "duckdb/planner/expression/bound_aggregate_expression.hpp"
#include "duckdb/planner/expression/bound_columnref_expression.hpp"
#include "duckdb/planner/bound_result_modifier.hpp"
#include "duckdb/function/aggregate/distributive_functions.hpp"

namespace duckdb {
namespace snowflake {

OptimizerExtension SnowflakeOptimizerExtension::GetOptimizerExtension() {
	OptimizerExtension ext;
	ext.pre_optimize_function = PreOptimize;
	return ext;
}

void SnowflakeOptimizerExtension::PreOptimize(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan) {
	if (!plan) {
		return;
	}
	OptimizePlan(plan);
}

void SnowflakeOptimizerExtension::OptimizePlan(unique_ptr<LogicalOperator> &op) {
	if (!op) {
		return;
	}

	// Check if this is a LIMIT operator
	if (op->type == LogicalOperatorType::LOGICAL_LIMIT) {
		auto &limit_op = op->Cast<LogicalLimit>();
		DPRINT("SnowflakeOptimizerExtension: Found LOGICAL_LIMIT\n");

		// Extract LIMIT value if it's a constant
		idx_t limit_value = SnowflakeArrowStreamFactory::NO_LIMIT;
		idx_t offset_value = 0;

		if (limit_op.limit_val.Type() == LimitNodeType::CONSTANT_VALUE) {
			limit_value = limit_op.limit_val.GetConstantValue();
			DPRINT("SnowflakeOptimizerExtension: LIMIT = %lu\n", limit_value);
		}

		if (limit_op.offset_val.Type() == LimitNodeType::CONSTANT_VALUE) {
			offset_value = limit_op.offset_val.GetConstantValue();
			DPRINT("SnowflakeOptimizerExtension: OFFSET = %lu\n", offset_value);
		}

		// Only proceed if we have a constant limit
		if (limit_value != SnowflakeArrowStreamFactory::NO_LIMIT) {
			// Find the Snowflake scan in the subtree (may be nested under projections, etc.)
			auto *snowflake_get = FindSnowflakeScan(*op);
			if (snowflake_get) {
				PushLimitToSnowflakeScan(*snowflake_get, limit_value, offset_value);
			}
		}
	}

	// Check if this is an AGGREGATE operator (for COUNT pushdown)
	if (op->type == LogicalOperatorType::LOGICAL_AGGREGATE_AND_GROUP_BY) {
		if (TryPushdownAggregate(op)) {
			// Aggregate was pushed down and plan was modified
			// Continue with the new plan
			OptimizePlan(op);
			return;
		}
	}

	// Recursively process children
	for (auto &child : op->children) {
		OptimizePlan(child);
	}
}

bool SnowflakeOptimizerExtension::IsSnowflakeScan(LogicalOperator &op) {
	if (op.type != LogicalOperatorType::LOGICAL_GET) {
		return false;
	}

	auto &get_op = op.Cast<LogicalGet>();

	// Check if this is a Snowflake table scan by looking at the function name
	// The function name should be "snowflake_table_scan" for ATTACH'd tables
	// or "snowflake_scan" / "snowflake_query" for direct function calls
	const string &func_name = get_op.function.name;
	bool is_snowflake = (func_name == "snowflake_table_scan" || func_name == "snowflake_scan" ||
	                     func_name == "snowflake_query");

	if (is_snowflake) {
		DPRINT("SnowflakeOptimizerExtension: Found Snowflake scan: %s\n", func_name.c_str());
	}

	return is_snowflake;
}

LogicalOperator *SnowflakeOptimizerExtension::FindSnowflakeScan(LogicalOperator &op) {
	// Check if current operator is a Snowflake scan
	if (IsSnowflakeScan(op)) {
		return &op;
	}

	// Recursively search children
	for (auto &child : op.children) {
		auto *result = FindSnowflakeScan(*child);
		if (result) {
			return result;
		}
	}

	return nullptr;
}

void SnowflakeOptimizerExtension::PushLimitToSnowflakeScan(LogicalOperator &get_op, idx_t limit_value,
                                                           idx_t offset_value) {
	auto &get = get_op.Cast<LogicalGet>();

	// The bind_data should be a SnowflakeScanBindData which contains the factory
	if (!get.bind_data) {
		DPRINT("SnowflakeOptimizerExtension: No bind_data found\n");
		return;
	}

	// Try to cast to SnowflakeScanBindData
	auto *snowflake_bind_data = dynamic_cast<SnowflakeScanBindData *>(get.bind_data.get());
	if (!snowflake_bind_data) {
		DPRINT("SnowflakeOptimizerExtension: bind_data is not SnowflakeScanBindData\n");
		return;
	}

	// Set the LIMIT/OFFSET values on the factory
	if (snowflake_bind_data->factory) {
		snowflake_bind_data->factory->limit_value = limit_value;
		snowflake_bind_data->factory->offset_value = offset_value;
		DPRINT("SnowflakeOptimizerExtension: Pushed LIMIT %lu OFFSET %lu to Snowflake factory\n", limit_value,
		       offset_value);
	}
}

bool SnowflakeOptimizerExtension::TryPushdownAggregate(unique_ptr<LogicalOperator> &op) {
	auto &agg_op = op->Cast<LogicalAggregate>();

	// Only push down simple aggregates without GROUP BY
	if (!agg_op.groups.empty()) {
		DPRINT("SnowflakeOptimizerExtension: Aggregate has GROUP BY, skipping pushdown\n");
		return false;
	}

	// Check if we have exactly one aggregate expression
	if (agg_op.expressions.size() != 1) {
		DPRINT("SnowflakeOptimizerExtension: Multiple aggregates (%lu), skipping pushdown\n",
		       agg_op.expressions.size());
		return false;
	}

	// Get the aggregate expression
	auto &expr = agg_op.expressions[0];
	if (expr->expression_class != ExpressionClass::BOUND_AGGREGATE) {
		DPRINT("SnowflakeOptimizerExtension: Expression is not BOUND_AGGREGATE\n");
		return false;
	}

	auto &bound_agg = expr->Cast<BoundAggregateExpression>();
	const string &func_name = bound_agg.function.name;

	DPRINT("SnowflakeOptimizerExtension: Found aggregate function: %s\n", func_name.c_str());

	// Check if this is a COUNT aggregate using DuckDB's constants
	bool is_count_star = (func_name == CountStarFun::Name);
	bool is_count = (func_name == CountFun::Name);

	if (!is_count_star && !is_count) {
		DPRINT("SnowflakeOptimizerExtension: Not a COUNT aggregate, skipping\n");
		return false;
	}

	// Find the Snowflake scan in the subtree
	auto *snowflake_get = FindSnowflakeScan(*op);
	if (!snowflake_get) {
		DPRINT("SnowflakeOptimizerExtension: No Snowflake scan found for aggregate\n");
		return false;
	}

	auto &get = snowflake_get->Cast<LogicalGet>();
	if (!get.bind_data) {
		DPRINT("SnowflakeOptimizerExtension: No bind_data found for aggregate pushdown\n");
		return false;
	}

	auto *snowflake_bind_data = dynamic_cast<SnowflakeScanBindData *>(get.bind_data.get());
	if (!snowflake_bind_data || !snowflake_bind_data->factory) {
		DPRINT("SnowflakeOptimizerExtension: bind_data is not SnowflakeScanBindData\n");
		return false;
	}

	// Set aggregate pushdown info on the factory
	string aggregate_expr;
	if (is_count_star) {
		aggregate_expr = "COUNT(*)";
		DPRINT("SnowflakeOptimizerExtension: Pushing down COUNT(*) to Snowflake\n");
	} else if (is_count) {
		// For COUNT(column), we need to get the column name
		if (bound_agg.children.size() == 1 &&
		    bound_agg.children[0]->expression_class == ExpressionClass::BOUND_COLUMN_REF) {
			auto &col_ref = bound_agg.children[0]->Cast<BoundColumnRefExpression>();
			string col_name = col_ref.alias.empty() ? col_ref.ToString() : col_ref.alias;
			aggregate_expr = "COUNT(" + col_name + ")";
			DPRINT("SnowflakeOptimizerExtension: Pushing down COUNT(%s) to Snowflake\n", col_name.c_str());
		} else {
			DPRINT("SnowflakeOptimizerExtension: COUNT argument is not a simple column reference\n");
			return false;
		}
	}

	// NOTE: We cannot push down COUNT aggregate to Snowflake in the current architecture.
	//
	// The problem is that the schema for the Snowflake scan is determined during bind
	// (before the optimizer runs). At bind time, the query is "SELECT * FROM table" which
	// returns the full table schema. If we change the query to "SELECT COUNT(*) FROM table"
	// in the optimizer, the schema no longer matches - Snowflake returns one INT64 column
	// but DuckDB expects the original table columns.
	//
	// To properly implement COUNT pushdown, we would need to:
	// 1. Detect COUNT during bind (not optimizer) and set up the schema accordingly
	// 2. Or re-bind the scan after modifying the query
	// 3. Or use a separate mechanism for aggregate-only queries
	//
	// For now, we detect the pattern but don't modify the query - DuckDB will compute
	// the COUNT locally (which still works correctly, just not as efficiently).
	(void)aggregate_expr;  // Unused for now
	snowflake_bind_data->factory->aggregate_pushdown.clear();  // Don't push down
	DPRINT("SnowflakeOptimizerExtension: COUNT pushdown detected but not implemented (schema mismatch issue)\n");
	return false;
}

} // namespace snowflake
} // namespace duckdb
