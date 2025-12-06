#include "snowflake_optimizer_extension.hpp"
#include "snowflake_scan.hpp"
#include "snowflake_debug.hpp"
#include "duckdb/planner/operator/logical_limit.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
#include "duckdb/planner/bound_result_modifier.hpp"

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
	OptimizePlan(*plan);
}

void SnowflakeOptimizerExtension::OptimizePlan(LogicalOperator &op) {
	// Check if this is a LIMIT operator with a Snowflake scan as its child
	if (op.type == LogicalOperatorType::LOGICAL_LIMIT) {
		auto &limit_op = op.Cast<LogicalLimit>();

		// Check if we have exactly one child and it's a LogicalGet (table scan)
		if (op.children.size() == 1 && op.children[0]->type == LogicalOperatorType::LOGICAL_GET) {
			auto &get_op = *op.children[0];

			if (IsSnowflakeScan(get_op)) {
				// Extract LIMIT value if it's a constant
				idx_t limit_value = SnowflakeArrowStreamFactory::NO_LIMIT;
				idx_t offset_value = 0;

				if (limit_op.limit_val.Type() == LimitNodeType::CONSTANT_VALUE) {
					limit_value = limit_op.limit_val.GetConstantValue();
					DPRINT("SnowflakeOptimizerExtension: Found LIMIT %lu\n", limit_value);
				}

				if (limit_op.offset_val.Type() == LimitNodeType::CONSTANT_VALUE) {
					offset_value = limit_op.offset_val.GetConstantValue();
					DPRINT("SnowflakeOptimizerExtension: Found OFFSET %lu\n", offset_value);
				}

				// Only push down if we have a constant limit
				if (limit_value != SnowflakeArrowStreamFactory::NO_LIMIT) {
					PushLimitToSnowflakeScan(get_op, limit_value, offset_value);
				}
			}
		}
	}

	// Recursively process children
	for (auto &child : op.children) {
		OptimizePlan(*child);
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

} // namespace snowflake
} // namespace duckdb
