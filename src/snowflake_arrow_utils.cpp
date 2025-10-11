#include "snowflake_debug.hpp"
#include "snowflake_arrow_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/common/types.hpp"

namespace duckdb {

// Wrapper to handle ADBC ArrowArrayStream
// This class takes ownership of an ADBC stream and makes it compatible with DuckDB's ArrowArrayStreamWrapper
class SnowflakeArrowArrayStreamWrapper : public ArrowArrayStreamWrapper {
public:
	SnowflakeArrowArrayStreamWrapper() : ArrowArrayStreamWrapper() {
	}

	void InitializeFromADBC(struct ArrowArrayStream *stream) {
		// Directly copy the ADBC stream structure to our base class member
		// This provides zero-copy access to the Arrow data from Snowflake
		arrow_array_stream = *stream;
		// Clear the source to prevent double-release
		// The wrapper now owns the stream and will handle cleanup
		std::memset(stream, 0, sizeof(*stream));
	}
};

// This function is called by DuckDB's arrow_scan to produce an ArrowArrayStreamWrapper
// It's called once per scan to create the stream that will provide data chunks
unique_ptr<ArrowArrayStreamWrapper> SnowflakeProduceArrowScan(uintptr_t factory_ptr,
                                                              ArrowStreamParameters &parameters) {
	auto factory = reinterpret_cast<SnowflakeArrowStreamFactory *>(factory_ptr);

	DPRINT("SnowflakeProduceArrowScan: factory=%p, statement_initialized=%d\n", (void *)factory,
	       factory->statement_initialized);

	// Extract projection columns from parameters
	std::vector<std::string> projection_columns;
	for (const auto &col_name : parameters.projected_columns.columns) {
		projection_columns.push_back(col_name);
	}

	// Log filter information for debugging
	if (parameters.filters && !parameters.filters->filters.empty()) {
		DPRINT("Found %zu filters in parameters\n", parameters.filters->filters.size());
		for (const auto &filter_entry : parameters.filters->filters) {
			DPRINT("Filter found for column %llu\n", filter_entry.first);
		}
	}

	// Update pushdown parameters from DuckDB's Arrow stream parameters
	factory->UpdatePushdownParameters(projection_columns, parameters.filters);

	// Initialize ADBC statement if not already done
	// We defer this to the produce function to avoid executing the query during bind
	if (!factory->statement_initialized) {
		AdbcError error;
		std::memset(&error, 0, sizeof(error));

		// Create a new ADBC statement from the connection
		AdbcStatusCode status = AdbcStatementNew(factory->connection->GetConnection(), &factory->statement, &error);
		DPRINT("Statement created at %p for factory %p\n", (void *)&factory->statement, (void *)factory);
		if (status != ADBC_STATUS_OK) {
			std::string error_msg = "Failed to create statement";
			if (error.message) {
				error_msg += ": " + std::string(error.message);
				if (error.release) {
					error.release(&error);
				}
			}
			throw IOException(error_msg);
		}
		factory->statement_initialized = true;
	}

	// Always update the query on the statement with the latest pushdown modifications
	// This ensures the modified query is used for execution
	AdbcError query_error;
	std::memset(&query_error, 0, sizeof(query_error));
	const std::string &query_to_use = factory->modified_query.empty() ? factory->query : factory->modified_query;

	AdbcStatusCode query_status = AdbcStatementSetSqlQuery(&factory->statement, query_to_use.c_str(), &query_error);
	if (query_status != ADBC_STATUS_OK) {
		std::string error_msg = "Failed to set modified query: ";
		if (query_error.message) {
			error_msg += query_error.message;
			if (query_error.release) {
				query_error.release(&query_error);
			}
		}
		throw IOException(error_msg);
	}
	DPRINT("Query set on statement: '%s'\n", query_to_use.c_str());

	// Execute the query and get the ArrowArrayStream
	// This is where the actual query execution happens
	auto wrapper = make_uniq<SnowflakeArrowArrayStreamWrapper>();
	struct ArrowArrayStream adbc_stream;
	int64_t rows_affected;
	AdbcError error;
	std::memset(&error, 0, sizeof(error));

	// ExecuteQuery returns an ArrowArrayStream that provides Arrow record batches
	AdbcStatusCode status = AdbcStatementExecuteQuery(&factory->statement, &adbc_stream, &rows_affected, &error);
	if (status != ADBC_STATUS_OK) {
		std::string error_msg = "Failed to execute query: ";
		if (error.message) {
			error_msg += error.message;
			if (error.release) {
				error.release(&error);
			}
		}
		// Clean up statement on error
		if (factory->statement_initialized) {
			AdbcError cleanup_error;
			std::memset(&cleanup_error, 0, sizeof(cleanup_error));
			AdbcStatementRelease(&factory->statement, &cleanup_error);
			factory->statement_initialized = false;
		}
		throw IOException(error_msg);
	}

	// Transfer ownership of the ADBC stream to our wrapper
	// This ensures zero-copy data transfer from Snowflake to DuckDB
	try {
		wrapper->InitializeFromADBC(&adbc_stream);
		wrapper->number_of_rows = rows_affected;
	} catch (const std::exception &e) {
		// Clean up stream on error
		if (adbc_stream.release) {
			adbc_stream.release(&adbc_stream);
		}
		throw IOException("Failed to initialize Arrow stream wrapper: " + std::string(e.what()));
	}

	return std::move(wrapper);
}

void SnowflakeArrowStreamFactory::SetColumnNames(const std::vector<std::string> &names) {
	column_names = names;
}

void SnowflakeArrowStreamFactory::SetFilterPushdownEnabled(bool enabled) {
	filter_pushdown_enabled = enabled;
}

void SnowflakeArrowStreamFactory::UpdatePushdownParameters(const std::vector<std::string> &projection,
                                                           TableFilterSet *filter_set) {
	// Thread-safe parameter update
	projection_columns = projection;
	current_filters = filter_set;

	// Only apply pushdown if enabled
	if (!filter_pushdown_enabled) {
		DPRINT("Pushdown disabled - using original query without optimization\n");
		modified_query = query;
		return;
	}

	try {
		// Build WHERE clause from filters
		std::string where_clause = "";
		if (current_filters && !current_filters->filters.empty()) {
			// Use projected column names for filter mapping since DuckDB filter indices
			// correspond to the projected columns, not the full table schema
			where_clause = snowflake::SnowflakeQueryBuilder::BuildWhereClause(current_filters, projection_columns);
		}

		// Build SELECT clause from projection
		std::string select_clause =
		    snowflake::SnowflakeQueryBuilder::BuildSelectClause(projection_columns, column_names);

		// Modify the original query with pushdown optimizations
		modified_query = snowflake::SnowflakeQueryBuilder::ModifyQuery(query, select_clause, where_clause);

		DPRINT("Pushdown applied - WHERE: '%s', SELECT: '%s'\n", where_clause.c_str(), select_clause.c_str());
	} catch (const std::exception &e) {
		DPRINT("Warning: Failed to apply pushdown, falling back to original query: %s\n", e.what());
		modified_query = query;
	}
}

// This function is called by DuckDB's arrow_scan during bind to get the schema
// It allows DuckDB to know the column types before actually executing the query
void SnowflakeGetArrowSchema(ArrowArrayStream *factory_ptr, ArrowSchema &schema) {
	auto factory = reinterpret_cast<SnowflakeArrowStreamFactory *>(factory_ptr);

	// Initialize statement if not already done
	if (!factory->statement_initialized) {
		AdbcError error;
		std::memset(&error, 0, sizeof(error));

		AdbcStatusCode status = AdbcStatementNew(factory->connection->GetConnection(), &factory->statement, &error);
		DPRINT("Statement created at %p for factory %p\n", (void *)&factory->statement, (void *)factory);
		if (status != ADBC_STATUS_OK) {
			throw IOException("Failed to create statement");
		}
		factory->statement_initialized = true;

		// For schema discovery, we use the original query since pushdown hasn't been applied yet
		// The modified query will be set later during data production
		status = AdbcStatementSetSqlQuery(&factory->statement, factory->query.c_str(), &error);
		if (status != ADBC_STATUS_OK) {
			std::string error_msg = "Failed to set query for schema: ";
			if (error.message) {
				error_msg += error.message;
				if (error.release) {
					error.release(&error);
				}
			}
			throw IOException(error_msg);
		}
		DPRINT("Original query set for schema discovery: '%s'\n", factory->query.c_str());
	}

	// Execute with schema only - this is a lightweight operation that just returns
	// the schema without actually executing the full query
	AdbcError schema_error;
	std::memset(&schema_error, 0, sizeof(schema_error));
	std::memset(&schema, 0, sizeof(schema));

	AdbcStatusCode schema_status = AdbcStatementExecuteSchema(&factory->statement, &schema, &schema_error);
	DPRINT("ExecuteSchema completed for statement %p\n", (void *)&factory->statement);
	if (schema_status != ADBC_STATUS_OK) {
		std::string error_msg = "Failed to get schema: ";
		if (schema_error.message) {
			error_msg += schema_error.message;
			if (schema_error.release) {
				schema_error.release(&schema_error);
			}
		}
		throw IOException(error_msg);
	}
}

} // namespace duckdb
