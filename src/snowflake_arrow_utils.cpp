#include "snowflake_debug.hpp"
#include "snowflake_arrow_utils.hpp"
#include "snowflake_query_builder.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

// Wrapper to handle ADBC ArrowArrayStream
// This class takes ownership of an ADBC stream and makes it compatible with
// DuckDB's ArrowArrayStreamWrapper
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

// This function is called by DuckDB's arrow_scan to produce an
// ArrowArrayStreamWrapper It's called once per scan to create the stream that
// will provide data chunks
unique_ptr<ArrowArrayStreamWrapper> SnowflakeProduceArrowScan(uintptr_t factory_ptr,
                                                              ArrowStreamParameters &parameters) {
	auto factory = reinterpret_cast<SnowflakeArrowStreamFactory *>(factory_ptr);
	DPRINT("SnowflakeProduceArrowScan: factory=%p, statement_initialized=%d\n", (void *)factory,
	       factory->statement_initialized);
	DPRINT("SnowflakeProduceArrowScan: pushdown enabled: filter=%d, projection=%d\n", factory->filter_pushdown_enabled,
	       factory->projection_pushdown_enabled);
	// Only log projected columns when projection pushdown is enabled
	if (factory->projection_pushdown_enabled) {
		DPRINT("SnowflakeProduceArrowScan: projected_columns.columns.size()=%llu\n",
		       parameters.projected_columns.columns.size());
		for (size_t i = 0; i < parameters.projected_columns.columns.size(); i++) {
			DPRINT("  projected_column[%llu] = %s\n", i, parameters.projected_columns.columns[i].c_str());
		}
	}

	// Apply pushdown if enabled (this modifies the query before execution)
	if (factory->filter_pushdown_enabled || factory->projection_pushdown_enabled) {
		// Extract projection columns from parameters
		vector<string> projection_cols;
		for (const auto &col : parameters.projected_columns.columns) {
			projection_cols.push_back(col);
		}

		// Call UpdatePushdownParameters to build modified query
		factory->UpdatePushdownParameters(projection_cols, parameters.filters);
	} else {
		// No pushdown - use original query as-is (like main branch behavior)
		DPRINT("Pushdown disabled, using original query: %s\n", factory->query.c_str());
		// Log what DuckDB is passing us even when pushdown is disabled for
		// debugging
		DPRINT("  DuckDB passed %llu projection columns (ignored since pushdown "
		       "disabled)\n",
		       parameters.projected_columns.columns.size());
		if (parameters.filters) {
			DPRINT("  DuckDB passed %llu filters (ignored since pushdown disabled)\n",
			       parameters.filters->filters.size());
		}
		// Don't call UpdatePushdownParameters - keep factory->modified_query as the
		// original query
	}

	// Initialize ADBC statement if not already done
	// We defer this to the produce function to avoid executing the query during
	// bind
	if (!factory->statement_initialized) {
		AdbcError error;
		std::memset(&error, 0, sizeof(error));

		// Create a new ADBC statement from the connection
		AdbcStatusCode status = AdbcStatementNew(factory->connection->GetConnection(), &factory->statement, &error);
		DPRINT("Statement created at %p for factory %p\n", (void *)&factory->statement, (void *)factory);
		if (status != ADBC_STATUS_OK) {
			throw IOException("Failed to create statement");
		}
		factory->statement_initialized = true;
	}

	// Always set the SQL query before execution to ensure we use the modified
	// query with pushdown This handles the case where the statement was
	// initialized during schema fetch but pushdown parameters are only available
	// now during execution
	{
		AdbcError set_error;
		std::memset(&set_error, 0, sizeof(set_error));
		AdbcStatusCode set_status =
		    AdbcStatementSetSqlQuery(&factory->statement, factory->modified_query.c_str(), &set_error);
		DPRINT("Setting query on statement: %s\n", factory->modified_query.c_str());
		if (set_status != ADBC_STATUS_OK) {
			std::string error_msg = "Failed to set query: ";
			if (set_error.message) {
				error_msg += set_error.message;
				if (set_error.release) {
					set_error.release(&set_error);
				}
			}
			throw IOException(error_msg);
		}
	}

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
		throw IOException(error_msg);
	}

	// Transfer ownership of the ADBC stream to our wrapper
	// This ensures zero-copy data transfer from Snowflake to DuckDB
	wrapper->InitializeFromADBC(&adbc_stream);
	wrapper->number_of_rows = rows_affected;

	return std::move(wrapper);
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

		// Set the query (use modified_query which includes pushdown)
		status = AdbcStatementSetSqlQuery(&factory->statement, factory->modified_query.c_str(), &error);
		if (status != ADBC_STATUS_OK) {
			std::string error_msg = "Failed to set query: ";
			if (error.message) {
				error_msg += error.message;
				if (error.release) {
					error.release(&error);
				}
			}
			throw IOException(error_msg);
		}
	}

	// Execute with schema only - this is a lightweight operation that just
	// returns the schema without actually executing the full query
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

void SnowflakeArrowStreamFactory::UpdatePushdownParameters(const vector<string> &projection,
                                                           TableFilterSet *filter_set) {
	DPRINT("UpdatePushdownParameters called: projection_size=%lu, "
	       "filter_count=%lu\n",
	       projection.size(), filter_set ? filter_set->filters.size() : 0);

	// Store the parameters
	projection_columns = projection;
	current_filters = filter_set;

	try {
		// Extract table name from base query (e.g., "SELECT * FROM
		// database.schema.table")
		string table_name;
		size_t from_pos = StringUtil::Upper(query).find(" FROM ");
		if (from_pos != string::npos) {
			table_name = query.substr(from_pos + 6); // Skip " FROM "
			StringUtil::Trim(table_name);
			// Remove trailing semicolon if present
			if (!table_name.empty() && table_name.back() == ';') {
				table_name.pop_back();
				StringUtil::Trim(table_name);
			}
		} else {
			throw InvalidInputException("Invalid base query format: missing FROM clause");
		}

		// Determine what to push down based on enabled flags
		vector<string> cols_to_project;
		TableFilterSet *filters_to_push = nullptr;

		// Only apply projection if projection pushdown is enabled
		if (projection_pushdown_enabled && !projection_columns.empty()) {
			cols_to_project = projection_columns;
		}

		// Only push filters if filter pushdown is enabled
		if (filter_pushdown_enabled) {
			filters_to_push = current_filters;
		}

		// Build the complete query using AST construction
		// Note: When filters are present, the filter column indices refer to
		// positions in the projection list (cols_to_project), NOT the original
		// schema (column_names). So we pass cols_to_project as the column mapping
		// for filters.
		vector<string> filter_column_names = cols_to_project.empty() ? column_names : cols_to_project;
		modified_query = snowflake::SnowflakeQueryBuilder::BuildQuery(table_name, cols_to_project, filters_to_push,
		                                                              filter_column_names);

		DPRINT("Pushdown applied:\n  Original: %s\n  Modified: %s\n", query.c_str(), modified_query.c_str());

	} catch (const NotImplementedException &e) {
		// Re-throw NotImplementedException - don't fall back in strict mode
		throw;
	} catch (const std::exception &e) {
		// Fallback for other exceptions (e.g., parsing errors)
		DPRINT("Pushdown failed: %s, using original query\n", e.what());
		modified_query = query;
	}
}

} // namespace duckdb
