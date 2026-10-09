#pragma once

#include "duckdb.hpp"
#include "duckdb/common/arrow/arrow_wrapper.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/common/adbc/adbc.h"
#include "duckdb/storage/table/scan_state.hpp"

#include <utility>
#include "snowflake_client_manager.hpp"
#include "snowflake_settings.hpp"

namespace duckdb {

// Factory structure to hold ADBC connection and query information
// This factory pattern allows us to integrate with DuckDB's arrow_scan table
// function which expects a factory that can produce ArrowArrayStreamWrapper
// instances
struct SnowflakeArrowStreamFactory {
	// Connection leased exclusively for this scan, so no other scan drives it
	// concurrently (ADBC connections are not thread-safe). Returned on destruction.
	snowflake::ConnectionLease lease;

	// SQL query to execute (original base query)
	std::string query;

	// Modified query after applying pushdown (if enabled)
	std::string modified_query;

	// ADBC statement handle - initialized lazily when first needed
	AdbcStatement statement;
	bool statement_initialized = false;

	// Pre-executed Arrow stream from bind time (used by snowflake_query to avoid
	// double-executing the query: once for schema in bind, once for data in scan)
	struct ArrowArrayStream cached_stream;
	bool has_cached_stream = false;

	// Pushdown configuration
	bool filter_pushdown_enabled = false;
	bool projection_pushdown_enabled = false;

	// True for the snowflake_query() passthrough SELECT path: the base query is
	// arbitrary user SQL, so projection is applied by wrapping it as a subquery
	// (SELECT <cols> FROM (<query>)) rather than by parsing a table name out of it.
	bool wrap_as_subquery = false;

	// ADBC driver read-ahead knobs, snapshotted from the session at bind time.
	// These CANNOT be read where the statement is created: SnowflakeProduceArrowScan
	// is handed to DuckDB's arrow scanner as a plain function pointer (see
	// SnowflakeScanBindData), so DuckDB fixes its signature and passes no
	// ClientContext — there is nothing there to call TryGetCurrentSetting on.
	// Bind has the context, so the values are captured there and carried here.
	snowflake::SnowflakeDriverSettings driver_settings;

	// Pushdown parameters (set by DuckDB via UpdatePushdownParameters)
	vector<string> projection_columns;
	TableFilterSet *current_filters = nullptr;
	vector<string> column_names; // Maps column indices to names for filter building

	SnowflakeArrowStreamFactory(snowflake::ConnectionLease lease_p, const std::string &query_str)
	    : lease(std::move(lease_p)), query(query_str), modified_query(query_str) {
		std::memset(&statement, 0, sizeof(statement));
		std::memset(&cached_stream, 0, sizeof(cached_stream));
	}

	~SnowflakeArrowStreamFactory() {
		// Release ADBC resources that live on the leased connection BEFORE the
		// lease (declared first, destroyed last) returns that connection to the
		// pool — the statement/stream are backed by this connection.
		if (statement_initialized) {
			AdbcError error;
			AdbcStatementRelease(&statement, &error);
		}
		if (has_cached_stream && cached_stream.release) {
			cached_stream.release(&cached_stream);
		}
	}

	// Update pushdown parameters from DuckDB optimizer
	// This is called by DuckDB when it wants to push filters and projections to
	// the source
	void UpdatePushdownParameters(const vector<string> &projection, TableFilterSet *filter_set);

	// Apply a projection to the snowflake_query() passthrough path by wrapping the
	// user query as a subquery: SELECT <quoted projection cols> FROM (<query>).
	// DuckDB's Arrow scanner reads projected columns positionally (child[k] == the
	// k-th requested column), so the produced stream MUST expose exactly these
	// columns in this order — see issue #32 (wrong-column reads / SIGSEGV when the
	// produced stream returned all columns regardless of the projection).
	void UpdatePassthroughProjection(const vector<string> &projection);
};

// Function to produce an ArrowArrayStreamWrapper from the factory
// This is called by DuckDB's arrow_scan when it needs to start scanning data
// Parameters:
//   factory_ptr: Pointer to our SnowflakeArrowStreamFactory cast to uintptr_t
//   parameters: Arrow stream parameters (projection columns and filters for
//   pushdown)
// Returns: An ArrowArrayStreamWrapper that provides Arrow data chunks
unique_ptr<ArrowArrayStreamWrapper> SnowflakeProduceArrowScan(uintptr_t factory_ptr, ArrowStreamParameters &parameters);

// Fetch the Arrow schema by executing the query with a 1-row limit (data-path
// schema) instead of AdbcStatementExecuteSchema. Required so geoarrow.wkb column
// tags (which the driver applies by peeking the first data batch) reach DuckDB's
// bind, and so Snowflake TIMESTAMP units are reported correctly (issue #44:
// ExecuteSchema metadata mis-reports TIMESTAMP_NTZ units). Uses a temporary
// statement/stream released before returning, so it does not disturb the
// factory's own statement used later during scan production.
void SnowflakeGetArrowSchemaViaQuery(SnowflakeArrowStreamFactory *factory, ArrowSchema &schema);

// Execute the query and cache the resulting Arrow stream in the factory.
// Also populates schema from the stream's get_schema callback.
// Used by snowflake_query bind to avoid double-execution: the cached stream is
// returned by SnowflakeProduceArrowScan instead of re-executing the query.
void SnowflakeExecuteAndCacheStream(SnowflakeArrowStreamFactory *factory, ArrowSchema &schema);

// Execute factory->query on the leased connection (discarding its rows) and return
// Snowflake's query id for it, read via LAST_QUERY_ID() on the same session — the
// lease guarantees no other statement runs in between. Used by snowflake_query bind
// to re-target row-returning metadata statements (SHOW/DESC/LIST/EXPLAIN/CALL) at
// TABLE(RESULT_SCAN('<id>')), which unlike the raw statement can be wrapped as a
// projected subquery (issue #48).
std::string SnowflakeExecuteAndGetQueryId(SnowflakeArrowStreamFactory *factory);

} // namespace duckdb
