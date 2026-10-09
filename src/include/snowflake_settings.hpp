#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/adbc/adbc.h"

namespace duckdb {

class ClientContext;
class DBConfig;

namespace snowflake {

//! Read-ahead knobs for the ADBC Snowflake driver, snapshotted from the session
//! settings at bind time.
//!
//! The driver buffers decoded Arrow records ahead of the consumer in memory it
//! allocates itself. Our handoff to DuckDB is zero-copy (see
//! SnowflakeArrowArrayStreamWrapper::InitializeFromADBC), so none of that memory
//! passes through DuckDB's buffer manager and `memory_limit` neither sees nor
//! bounds it. When the consumer is slower than the network — CREATE TABLE AS,
//! COPY TO a compressed file, an insert — the driver can pull the entire result
//! into memory while DuckDB has written a fraction of it (issue #66).
//!
//! A value of 0 means "leave the driver's own default alone", so a session that
//! never touches these settings behaves exactly as it did before they existed.
struct SnowflakeDriverSettings {
	//! adbc.rpc.result_queue_size: max Arrow batches buffered per result stream.
	int64_t result_queue_size = 0;
	//! adbc.snowflake.rpc.prefetch_concurrency: result chunks downloaded in parallel.
	int64_t prefetch_concurrency = 0;

	bool AnySet() const {
		return result_queue_size > 0 || prefetch_concurrency > 0;
	}

	//! Render for the DEBUG log. Renders an unset knob as "driver-default" rather
	//! than 0, since 0 is a value the driver itself would reject — logging it as a
	//! number reads as "we set it to 0", which is the opposite of what happened.
	string ToLogString() const;
};

//! Register the SET-able extension options. Called once from LoadInternal.
void RegisterSnowflakeSettings(DBConfig &config);

//! Read the current session values. Requires a ClientContext, so this is callable
//! only at bind time — see SnowflakeArrowStreamFactory::driver_settings for why
//! the values must be carried to the statement rather than read where they are used.
SnowflakeDriverSettings GetDriverSettings(ClientContext &context);

//! Apply the settings to a freshly created ADBC statement. Values of 0 are skipped.
//! Throws IOException if the driver rejects a value.
void ApplyDriverSettings(const SnowflakeDriverSettings &settings, AdbcStatement *statement);

} // namespace snowflake
} // namespace duckdb
