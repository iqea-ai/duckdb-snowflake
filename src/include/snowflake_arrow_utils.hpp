#pragma once

#include "duckdb.hpp"
#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/common/adbc/adbc.h"

#include <utility>
#include "snowflake_client_manager.hpp"

namespace duckdb {

// Factory structure to hold ADBC connection and query information
// This factory pattern allows us to integrate with DuckDB's arrow_scan table function
// which expects a factory that can produce ArrowArrayStreamWrapper instances
struct SnowflakeArrowStreamFactory {
	// Snowflake connection managed by the client manager
	shared_ptr<snowflake::SnowflakeClient> connection;

	// SQL query to execute
	std::string query;

	// ADBC statement handle - initialized lazily when first needed
	AdbcStatement statement;
	bool statement_initialized = false;

	SnowflakeArrowStreamFactory(shared_ptr<snowflake::SnowflakeClient> conn, const std::string &query_str)
	    : connection(std::move(conn)), query(query_str) {
		std::memset(&statement, 0, sizeof(statement));
	}

	~SnowflakeArrowStreamFactory() {
		// Clean up the ADBC statement if it was initialized
		if (statement_initialized) {
			AdbcError error;
			AdbcStatementRelease(&statement, &error);
		}
	}
};

// Function to produce an ArrowArrayStreamWrapper from the factory
// This is called by DuckDB's arrow_scan when it needs to start scanning data
// Parameters:
//   factory_ptr: Pointer to our SnowflakeArrowStreamFactory cast to uintptr_t
//   parameters: Arrow stream parameters (projection, filters, etc.) - currently unused
// Returns: An ArrowArrayStreamWrapper that provides Arrow data chunks
unique_ptr<ArrowArrayStreamWrapper> SnowflakeProduceArrowScan(uintptr_t factory_ptr, ArrowStreamParameters &parameters);

// Function to get the schema from the factory
// This is called by DuckDB's arrow_scan during bind to determine column types
// Parameters:
//   factory_ptr: Pointer to our SnowflakeArrowStreamFactory cast to ArrowArrayStream*
//   schema: Output parameter that will be filled with the Arrow schema
void SnowflakeGetArrowSchema(ArrowArrayStream *factory_ptr, ArrowSchema &schema);

} // namespace duckdb
