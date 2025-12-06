#pragma once

#include "duckdb.hpp"
#include "duckdb/optimizer/optimizer_extension.hpp"
#include "duckdb/planner/logical_operator.hpp"

namespace duckdb {
namespace snowflake {

//! SnowflakeOptimizerExtension: Optimizer extension for pushing down LIMIT/OFFSET and aggregates to Snowflake
//!
//! This extension runs before DuckDB's optimizers and detects patterns like:
//!   LogicalLimit -> LogicalGet (Snowflake table)
//!   LogicalAggregate (COUNT) -> LogicalGet (Snowflake table)
//!
//! When such a pattern is found, it extracts the values and stores them
//! in the Snowflake scan's bind data so they can be pushed down to Snowflake.
class SnowflakeOptimizerExtension {
public:
	//! Get the OptimizerExtension to register with DuckDB
	static OptimizerExtension GetOptimizerExtension();

private:
	//! Pre-optimize function that runs before DuckDB's optimizers
	static void PreOptimize(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan);

	//! Recursively traverse the plan and push down LIMIT to Snowflake scans
	static void OptimizePlan(unique_ptr<LogicalOperator> &op);

	//! Check if a LogicalGet is a Snowflake table scan
	static bool IsSnowflakeScan(LogicalOperator &op);

	//! Find a Snowflake scan in the subtree (may be nested under projections, etc.)
	static LogicalOperator *FindSnowflakeScan(LogicalOperator &op);

	//! Push LIMIT/OFFSET down to a Snowflake scan
	static void PushLimitToSnowflakeScan(LogicalOperator &get_op, idx_t limit_value, idx_t offset_value);

	//! Try to push down COUNT aggregate to Snowflake
	//! Returns true if pushdown was successful and plan was modified
	static bool TryPushdownAggregate(unique_ptr<LogicalOperator> &op);
};

} // namespace snowflake
} // namespace duckdb
