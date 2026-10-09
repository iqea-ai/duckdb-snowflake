#include "snowflake_debug.hpp"
#include "snowflake_arrow_utils.hpp"
#include "snowflake_query_builder.hpp"
#include "snowflake_client_manager.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

// Helper function to detect authentication/session-related errors
// Only checks for specific Snowflake error codes to avoid false positives
static bool IsAuthenticationError(const std::string &error_msg) {
	// Error 390111: Session no longer exists
	// Error 390114: Authentication token has expired
	// Error 390144: JWT token is invalid
	// Note: We intentionally exclude 250001 (connection failed) as it's too general
	// and can occur for network issues, not just auth failures
	return error_msg.find("390111") != std::string::npos || // Session no longer exists
	       error_msg.find("390114") != std::string::npos || // Token expired
	       error_msg.find("390144") != std::string::npos;   // Invalid JWT
}

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

	// If the query was pre-executed at bind time (snowflake_query path), return
	// the cached stream directly without re-executing.
	if (factory->has_cached_stream) {
		auto wrapper = make_uniq<SnowflakeArrowArrayStreamWrapper>();
		wrapper->InitializeFromADBC(&factory->cached_stream);
		factory->has_cached_stream = false;
		return std::move(wrapper);
	}
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

		// Build the modified query. The snowflake_query() passthrough path wraps the
		// user SQL as a subquery (projection only); the ATTACH path parses the table
		// name out of the base query and rebuilds it.
		if (factory->wrap_as_subquery) {
			factory->UpdatePassthroughProjection(projection_cols);
		} else {
			factory->UpdatePushdownParameters(projection_cols, parameters.filters);
		}
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
		AdbcStatusCode status = AdbcStatementNew(factory->lease->GetConnection(), &factory->statement, &error);
		DPRINT("Statement created at %p for factory %p\n", (void *)&factory->statement, (void *)factory);
		if (status != ADBC_STATUS_OK) {
			throw IOException("Failed to create statement");
		}
		factory->statement_initialized = true;

		// This is the statement that streams the result set, so it is the one that
		// matters for memory: without a bound here the driver reads the whole result
		// ahead of a slow consumer (issue #66).
		snowflake::ApplyDriverSettings(factory->driver_settings, &factory->statement);
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

		// Check if this is an authentication error (token expired, etc.)
		if (IsAuthenticationError(error_msg)) {
			// Invalidate the cached connection so next attempt gets a fresh one
			auto &client_manager = snowflake::SnowflakeClientManager::GetInstance();
			// Don't return this connection to the pool, and drop any idle ones —
			// the auth token has likely expired for all sessions on this config.
			factory->lease.Invalidate();
			client_manager.DrainIdle(factory->lease->GetConfig());
			DPRINT("Authentication error detected, connection invalidated for retry\n");

			// Provide a helpful error message
			throw IOException(error_msg +
			                  "\n\nThe authentication token may have expired. "
			                  "Please retry your query - a fresh connection will be established automatically.");
		}

		throw IOException(error_msg);
	}

	// Transfer ownership of the ADBC stream to our wrapper
	// This ensures zero-copy data transfer from Snowflake to DuckDB
	wrapper->InitializeFromADBC(&adbc_stream);
	wrapper->number_of_rows = rows_affected;

	return std::move(wrapper);
}

void SnowflakeGetArrowSchemaViaQuery(SnowflakeArrowStreamFactory *factory, ArrowSchema &schema) {
	// Fetch the bind schema by executing the query with a 1-row limit and reading
	// the schema off the executed stream, instead of AdbcStatementExecuteSchema.
	// Two reasons the data-path schema is required:
	//   * GEOGRAPHY/GEOMETRY: the driver tags columns geoarrow.wkb by peeking the
	//     first data batch, so the tag is absent from the ExecuteSchema metadata.
	//     A 1-row result lets the driver peek (a 0-row result cannot be peeked).
	//   * Timestamps: ExecuteSchema mis-reports Snowflake TIMESTAMP units (issue #44).
	// Newlines guard against a trailing line comment swallowing the closing paren.
	std::string probe_query = "SELECT * FROM (\n" + factory->modified_query + "\n) AS __sf_schema_probe LIMIT 1";

	AdbcError error;
	std::memset(&error, 0, sizeof(error));

	// Use a temporary statement so we don't initialize/consume factory->statement,
	// which the scan production path creates and uses on its own.
	AdbcStatement probe_stmt;
	std::memset(&probe_stmt, 0, sizeof(probe_stmt));
	AdbcStatusCode status = AdbcStatementNew(factory->lease->GetConnection(), &probe_stmt, &error);
	if (status != ADBC_STATUS_OK) {
		throw IOException("Failed to create statement for schema probe");
	}
	// LIMIT 1, so this cannot buffer anything meaningful; applied for consistency so
	// every statement on the lease runs under the same driver configuration. Released
	// on failure like every other throw below: the connection returns to the pool, so
	// a leaked statement handle would outlive this scan.
	try {
		snowflake::ApplyDriverSettings(factory->driver_settings, &probe_stmt);
	} catch (...) {
		AdbcStatementRelease(&probe_stmt, nullptr);
		throw;
	}

	status = AdbcStatementSetSqlQuery(&probe_stmt, probe_query.c_str(), &error);
	if (status != ADBC_STATUS_OK) {
		std::string msg = "Failed to set schema-probe query: ";
		if (error.message) {
			msg += error.message;
			if (error.release) {
				error.release(&error);
			}
		}
		AdbcStatementRelease(&probe_stmt, nullptr);
		throw IOException(msg);
	}

	ArrowArrayStream probe_stream;
	std::memset(&probe_stream, 0, sizeof(probe_stream));
	int64_t rows_affected = 0;
	AdbcError exec_error;
	std::memset(&exec_error, 0, sizeof(exec_error));
	status = AdbcStatementExecuteQuery(&probe_stmt, &probe_stream, &rows_affected, &exec_error);
	if (status != ADBC_STATUS_OK) {
		std::string msg = "Failed to execute schema-probe query: ";
		if (exec_error.message) {
			msg += exec_error.message;
			if (exec_error.release) {
				exec_error.release(&exec_error);
			}
		}
		AdbcStatementRelease(&probe_stmt, nullptr);
		if (IsAuthenticationError(msg)) {
			auto &client_manager = snowflake::SnowflakeClientManager::GetInstance();
			// Don't return this connection to the pool, and drop any idle ones —
			// the auth token has likely expired for all sessions on this config.
			factory->lease.Invalidate();
			client_manager.DrainIdle(factory->lease->GetConfig());
			throw IOException(msg + "\n\nThe authentication token may have expired. "
			                        "Please retry your query - a fresh connection will be established automatically.");
		}
		throw IOException(msg);
	}

	std::memset(&schema, 0, sizeof(schema));
	int rc = probe_stream.get_schema ? probe_stream.get_schema(&probe_stream, &schema) : -1;

	// Release the probe stream and statement; the caller only needs the schema.
	if (probe_stream.release) {
		probe_stream.release(&probe_stream);
	}
	AdbcStatementRelease(&probe_stmt, nullptr);

	if (rc != 0) {
		throw IOException("Failed to get schema from schema-probe stream");
	}
}

void SnowflakeExecuteAndCacheStream(SnowflakeArrowStreamFactory *factory, ArrowSchema &schema) {
	AdbcError error;
	std::memset(&error, 0, sizeof(error));
	AdbcStatusCode status = AdbcStatementNew(factory->lease->GetConnection(), &factory->statement, &error);
	if (status != ADBC_STATUS_OK) {
		throw IOException("Failed to create statement for snowflake_query");
	}
	factory->statement_initialized = true;

	// Safe to throw past this point: the factory owns the statement now and releases
	// it in its destructor.
	snowflake::ApplyDriverSettings(factory->driver_settings, &factory->statement);

	status = AdbcStatementSetSqlQuery(&factory->statement, factory->modified_query.c_str(), &error);
	if (status != ADBC_STATUS_OK) {
		std::string msg = "Failed to set query: ";
		if (error.message) {
			msg += error.message;
			if (error.release) {
				error.release(&error);
			}
		}
		throw IOException(msg);
	}

	int64_t rows_affected;
	AdbcError exec_error;
	std::memset(&exec_error, 0, sizeof(exec_error));
	status = AdbcStatementExecuteQuery(&factory->statement, &factory->cached_stream, &rows_affected, &exec_error);
	if (status != ADBC_STATUS_OK) {
		std::string msg = "Failed to execute snowflake_query: ";
		if (exec_error.message) {
			msg += exec_error.message;
			if (exec_error.release) {
				exec_error.release(&exec_error);
			}
		}
		if (IsAuthenticationError(msg)) {
			auto &client_manager = snowflake::SnowflakeClientManager::GetInstance();
			// Don't return this connection to the pool, and drop any idle ones —
			// the auth token has likely expired for all sessions on this config.
			factory->lease.Invalidate();
			client_manager.DrainIdle(factory->lease->GetConfig());
			DPRINT("Authentication error detected, connection invalidated for retry\n");
			throw IOException(msg + "\n\nThe authentication token may have expired. "
			                        "Please retry your query - a fresh connection will be established automatically.");
		}
		throw IOException(msg);
	}
	factory->has_cached_stream = true;

	std::memset(&schema, 0, sizeof(schema));
	if (factory->cached_stream.get_schema) {
		int rc = factory->cached_stream.get_schema(&factory->cached_stream, &schema);
		if (rc != 0) {
			throw IOException("Failed to get schema from snowflake_query stream");
		}
	}
}

// Execute one statement on the factory's leased connection with a temporary
// statement handle, optionally keeping the result stream. Shares the
// auth-error invalidation behavior of the other execution paths.
// When out_stream is requested, out_statement must be too: an ADBC statement
// must outlive its active stream (the factory keeps its own statement alive for
// the same reason), so the caller releases the stream first, then the statement.
static void ExecuteOnLease(SnowflakeArrowStreamFactory *factory, const std::string &sql, ArrowArrayStream *out_stream,
                           AdbcStatement *out_statement, const char *what) {
	AdbcError error;
	std::memset(&error, 0, sizeof(error));
	AdbcStatement stmt;
	std::memset(&stmt, 0, sizeof(stmt));
	if (AdbcStatementNew(factory->lease->GetConnection(), &stmt, &error) != ADBC_STATUS_OK) {
		throw IOException(std::string("Failed to create statement for ") + what);
	}
	try {
		snowflake::ApplyDriverSettings(factory->driver_settings, &stmt);
	} catch (...) {
		AdbcStatementRelease(&stmt, nullptr);
		throw;
	}
	if (AdbcStatementSetSqlQuery(&stmt, sql.c_str(), &error) != ADBC_STATUS_OK) {
		std::string msg = std::string("Failed to set query for ") + what + ": ";
		if (error.message) {
			msg += error.message;
			if (error.release) {
				error.release(&error);
			}
		}
		AdbcStatementRelease(&stmt, nullptr);
		throw IOException(msg);
	}
	ArrowArrayStream stream;
	std::memset(&stream, 0, sizeof(stream));
	int64_t rows_affected = 0;
	AdbcError exec_error;
	std::memset(&exec_error, 0, sizeof(exec_error));
	if (AdbcStatementExecuteQuery(&stmt, &stream, &rows_affected, &exec_error) != ADBC_STATUS_OK) {
		std::string msg = std::string("Failed to execute ") + what + ": ";
		if (exec_error.message) {
			msg += exec_error.message;
			if (exec_error.release) {
				exec_error.release(&exec_error);
			}
		}
		AdbcStatementRelease(&stmt, nullptr);
		if (IsAuthenticationError(msg)) {
			auto &client_manager = snowflake::SnowflakeClientManager::GetInstance();
			// Don't return this connection to the pool, and drop any idle ones —
			// the auth token has likely expired for all sessions on this config.
			factory->lease.Invalidate();
			client_manager.DrainIdle(factory->lease->GetConfig());
			throw IOException(msg + "\n\nThe authentication token may have expired. "
			                        "Please retry your query - a fresh connection will be established automatically.");
		}
		throw IOException(msg);
	}
	if (out_stream) {
		*out_stream = stream;
		*out_statement = stmt;
		return;
	}
	if (stream.release) {
		// Rows are not needed; the statement has run server-side, which is all
		// RESULT_SCAN requires.
		stream.release(&stream);
	}
	AdbcStatementRelease(&stmt, nullptr);
}

// Read row 0 of column 0 from a single-batch utf8 stream (e.g. SELECT
// LAST_QUERY_ID()). Handles both 32-bit ("u") and 64-bit ("U") offset layouts.
static std::string ReadFirstRowUtf8(ArrowArrayStream &stream, const char *what) {
	ArrowSchema schema;
	std::memset(&schema, 0, sizeof(schema));
	if (!stream.get_schema || stream.get_schema(&stream, &schema) != 0) {
		throw IOException(std::string("Failed to read schema from ") + what);
	}
	std::string format;
	if (schema.n_children == 1 && schema.children[0] && schema.children[0]->format) {
		format = schema.children[0]->format;
	}
	if (schema.release) {
		schema.release(&schema);
	}
	if (format != "u" && format != "U") {
		throw IOException(std::string(what) + " returned unexpected Arrow type '" + format + "', expected utf8");
	}

	ArrowArray batch;
	std::memset(&batch, 0, sizeof(batch));
	if (!stream.get_next || stream.get_next(&stream, &batch) != 0 || !batch.release) {
		throw IOException(std::string("Failed to read result batch from ") + what);
	}
	std::string value;
	auto *col = batch.children && batch.n_children == 1 ? batch.children[0] : nullptr;
	if (!col || col->length < 1 || col->n_buffers < 3) {
		batch.release(&batch);
		throw IOException(std::string(what) + " returned no rows");
	}
	auto row = col->offset;
	auto data = reinterpret_cast<const char *>(col->buffers[2]);
	if (format == "u") {
		auto offsets = reinterpret_cast<const int32_t *>(col->buffers[1]);
		value.assign(data + offsets[row], offsets[row + 1] - offsets[row]);
	} else {
		auto offsets = reinterpret_cast<const int64_t *>(col->buffers[1]);
		value.assign(data + offsets[row], offsets[row + 1] - offsets[row]);
	}
	batch.release(&batch);
	return value;
}

std::string SnowflakeExecuteAndGetQueryId(SnowflakeArrowStreamFactory *factory) {
	// LAST_QUERY_ID() below is only correct if both statements run on the same
	// exclusively-leased session with nothing in between. Capture the connection
	// identity so a refactor that swaps or re-acquires the lease mid-flight fails
	// loudly here instead of splicing a foreign query id into RESULT_SCAN.
	auto *leased_connection = factory->lease->GetConnection();

	// Run the statement itself; RESULT_SCAN only needs it executed server-side.
	ExecuteOnLease(factory, factory->query, nullptr, nullptr, "metadata statement");

	if (factory->lease->GetConnection() != leased_connection) {
		throw InternalException("snowflake_query metadata path: connection changed between the statement and "
		                        "LAST_QUERY_ID(); the query id would belong to a different session");
	}

	// Same session (exclusive lease), so LAST_QUERY_ID() is the statement above.
	// The statement handle stays open until the stream is drained and released.
	ArrowArrayStream id_stream;
	std::memset(&id_stream, 0, sizeof(id_stream));
	AdbcStatement id_stmt;
	std::memset(&id_stmt, 0, sizeof(id_stmt));
	ExecuteOnLease(factory, "SELECT LAST_QUERY_ID()", &id_stream, &id_stmt, "LAST_QUERY_ID()");
	std::string query_id;
	try {
		query_id = ReadFirstRowUtf8(id_stream, "LAST_QUERY_ID()");
	} catch (...) {
		if (id_stream.release) {
			id_stream.release(&id_stream);
		}
		AdbcStatementRelease(&id_stmt, nullptr);
		throw;
	}
	if (id_stream.release) {
		id_stream.release(&id_stream);
	}
	AdbcStatementRelease(&id_stmt, nullptr);

	// The id feeds a quoted SQL literal; Snowflake ids are UUID-shaped, so anything
	// else means we mis-read the result and must not splice it into a query.
	for (auto c : query_id) {
		if (!isalnum(static_cast<unsigned char>(c)) && c != '-') {
			throw IOException("Unexpected query id from LAST_QUERY_ID(): " + query_id);
		}
	}
	if (query_id.empty()) {
		throw IOException("LAST_QUERY_ID() returned an empty id");
	}
	return query_id;
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

void SnowflakeArrowStreamFactory::UpdatePassthroughProjection(const vector<string> &projection) {
	projection_columns = projection;

	// No projection requested (e.g. SELECT *): run the user query unchanged.
	if (projection.empty()) {
		modified_query = query;
		return;
	}

	// Select exactly the requested columns, in the requested order, from the user
	// query wrapped as a derived table. The names come from the result schema, so
	// quoting the exact name round-trips for both folded-uppercase and
	// case-sensitive columns.
	string cols;
	for (size_t i = 0; i < projection.size(); i++) {
		if (i > 0) {
			cols += ", ";
		}
		cols += snowflake::QuoteSnowflakeIdentifier(projection[i]);
	}
	// The newlines matter: a trailing line comment in the user's query would
	// otherwise swallow the closing paren and alias.
	modified_query = "SELECT " + cols + " FROM (\n" + query + "\n) AS __sf_passthrough";
	DPRINT("Passthrough projection applied:\n  Original: %s\n  Modified: %s\n", query.c_str(), modified_query.c_str());
}

} // namespace duckdb
