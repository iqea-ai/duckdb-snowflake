#pragma once

#include "duckdb.hpp"
#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/common/adbc/adbc.h"
#include "duckdb/storage/table/scan_state.hpp"
#include "duckdb/common/limits.hpp"

#include <utility>
#include "snowflake_client_manager.hpp"

namespace duckdb {

// Factory structure to hold ADBC connection and query information
// This factory pattern allows us to integrate with DuckDB's arrow_scan table
// function which expects a factory that can produce ArrowArrayStreamWrapper
// instances
struct SnowflakeArrowStreamFactory {
	//! Special value indicating no limit
	static constexpr idx_t NO_LIMIT = NumericLimits<idx_t>::Maximum();

	// Snowflake connection managed by the client manager
	shared_ptr<snowflake::SnowflakeClient> connection;

	// SQL query to execute (original base query)
	std::string query;

	// Modified query after applying pushdown (if enabled)
	std::string modified_query;

	// ADBC statement handle - initialized lazily when first needed
	AdbcStatement statement;
	bool statement_initialized = false;

	// Pushdown configuration
	bool filter_pushdown_enabled = false;
	bool projection_pushdown_enabled = false;

	// Pushdown parameters (set by DuckDB via UpdatePushdownParameters)
	vector<string> projection_columns;
	TableFilterSet *current_filters = nullptr;
	vector<string> column_names; // Maps column indices to names for filter building

	// LIMIT/OFFSET pushdown parameters
	idx_t limit_value = NO_LIMIT;
	idx_t offset_value = 0;

	SnowflakeArrowStreamFactory(shared_ptr<snowflake::SnowflakeClient> conn, const std::string &query_str)
	    : connection(std::move(conn)), query(query_str), modified_query(query_str) {
		std::memset(&statement, 0, sizeof(statement));
	}

	~SnowflakeArrowStreamFactory() {
		// Clean up the ADBC statement if it was initialized
		if (statement_initialized) {
			AdbcError error;
			AdbcStatementRelease(&statement, &error);
		}
	}

	// Update pushdown parameters from DuckDB optimizer
	// This is called by DuckDB when it wants to push filters and projections to
	// the source
	void UpdatePushdownParameters(const vector<string> &projection, TableFilterSet *filter_set,
	                              idx_t limit = NO_LIMIT, idx_t offset = 0);
};

// Function to produce an ArrowArrayStreamWrapper from the factory
// This is called by DuckDB's arrow_scan when it needs to start scanning data
// Parameters:
//   factory_ptr: Pointer to our SnowflakeArrowStreamFactory cast to uintptr_t
//   parameters: Arrow stream parameters (projection columns and filters for
//   pushdown)
// Returns: An ArrowArrayStreamWrapper that provides Arrow data chunks
unique_ptr<ArrowArrayStreamWrapper> SnowflakeProduceArrowScan(uintptr_t factory_ptr, ArrowStreamParameters &parameters);

// Function to get the schema from the factory
// This is called by DuckDB's arrow_scan during bind to determine column types
// Parameters:
//   factory_ptr: Pointer to our SnowflakeArrowStreamFactory cast to
//   ArrowArrayStream* schema: Output parameter that will be filled with the
//   Arrow schema
void SnowflakeGetArrowSchema(ArrowArrayStream *factory_ptr, ArrowSchema &schema);

} // namespace duckdb
