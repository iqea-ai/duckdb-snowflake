#include "snowflake_debug.hpp"
#include "snowflake_arrow_utils.hpp"
#include "duckdb/common/exception.hpp"

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

	// Initialize ADBC statement if not already done
	// We defer this to the produce function to avoid executing the query during bind
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

		// Set the SQL query on the statement
		status = AdbcStatementSetSqlQuery(&factory->statement, factory->query.c_str(), &error);
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

		// Set the query
		status = AdbcStatementSetSqlQuery(&factory->statement, factory->query.c_str(), &error);
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
