#include "snowflake_settings.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/config.hpp"

#include <cstring>
#include <string>

namespace duckdb {
namespace snowflake {

//! Upper bound on both knobs. They exist to REDUCE the driver's read-ahead and the
//! driver's own defaults are 100 and 5, so nothing near this ceiling is a
//! legitimate tuning choice. Both are capped, for different reasons, measured
//! against the driver we ship (go/v1.11.0):
//!
//!   * result_queue_size becomes a Go channel capacity, so a large value takes the
//!     process down instead of erroring: 1e10 is OOM-killed (SIGKILL) and INT64_MAX
//!     panics the driver with "makechan: size out of range", which aborts DuckDB
//!     because releasing the now-broken connection throws while unwinding. Even 1e8
//!     silently reserves ~1.6 GB per result stream.
//!   * prefetch_concurrency does NOT crash (it is an errgroup limit, verified fine
//!     at INT64_MAX), but a huge value means unbounded parallel chunk downloads,
//!     which amplifies the very problem issue #66 is about.
static constexpr int64_t MAX_READ_AHEAD_VALUE = 10000;

// Bounds are enforced at SET time so a bad value is reported where the mistake is
// made. Below the range the driver returns ADBC_STATUS_INVALID_ARGUMENT ("must be
// > 0") from deep inside a scan; above it the driver does not error at all, it
// crashes the process. 0 is ours, not the driver's: it means "send nothing".
static void ValidateReadAheadValue(Value &parameter, const char *setting_name) {
	if (parameter.IsNull()) {
		throw InvalidInputException("%s must be between 0 and %lld (0 = use the driver default)", setting_name,
		                            (long long)MAX_READ_AHEAD_VALUE);
	}
	auto value = parameter.GetValue<int64_t>();
	if (value < 0 || value > MAX_READ_AHEAD_VALUE) {
		throw InvalidInputException("%s must be between 0 and %lld (0 = use the driver default), got %lld",
		                            setting_name, (long long)MAX_READ_AHEAD_VALUE, (long long)value);
	}
}

static void CheckResultQueueSize(ClientContext &, SetScope, Value &parameter) {
	ValidateReadAheadValue(parameter, "snowflake_result_queue_size");
}

static void CheckPrefetchConcurrency(ClientContext &, SetScope, Value &parameter) {
	ValidateReadAheadValue(parameter, "snowflake_prefetch_concurrency");
}

void RegisterSnowflakeSettings(DBConfig &config) {
	// AddExtensionOption defaults to SetScope::SESSION, which is what we want: a
	// session that materializes a large result wants a small queue, an
	// interactive session does not care, and neither belongs in the secret.
	config.AddExtensionOption(
	    "snowflake_result_queue_size",
	    "Maximum Arrow batches the ADBC driver buffers per Snowflake result stream. "
	    "0 (the default) leaves the driver's own default in place; max 10000. Set to 1 to throttle "
	    "the driver's read-ahead when the consumer is slow (CREATE TABLE AS, COPY TO, "
	    "inserts), which otherwise buffers the result outside memory_limit.",
	    LogicalType::BIGINT, Value::BIGINT(0), CheckResultQueueSize);

	config.AddExtensionOption(
	    "snowflake_prefetch_concurrency",
	    "How many Snowflake result chunks the ADBC driver downloads in parallel. "
	    "0 (the default) leaves the driver's own default in place; max 10000. Lowering this reduces "
	    "read-ahead further, but costs download throughput.",
	    LogicalType::BIGINT, Value::BIGINT(0), CheckPrefetchConcurrency);
}

string SnowflakeDriverSettings::ToLogString() const {
	string result = "result_queue_size=";
	result += result_queue_size > 0 ? std::to_string(result_queue_size) : "driver-default";
	result += " prefetch_concurrency=";
	result += prefetch_concurrency > 0 ? std::to_string(prefetch_concurrency) : "driver-default";
	return result;
}

SnowflakeDriverSettings GetDriverSettings(ClientContext &context) {
	SnowflakeDriverSettings settings;
	Value value;
	if (context.TryGetCurrentSetting("snowflake_result_queue_size", value) && !value.IsNull()) {
		settings.result_queue_size = value.GetValue<int64_t>();
	}
	if (context.TryGetCurrentSetting("snowflake_prefetch_concurrency", value) && !value.IsNull()) {
		settings.prefetch_concurrency = value.GetValue<int64_t>();
	}
	return settings;
}

// Always the string setter. AdbcStatementSetOption is part of the ADBC 1.0.0
// vtable and is therefore always populated; AdbcStatementSetOptionInt was added
// in 1.1.0 and our vendored driver manager dispatches it without a null check
// (third_party/adbc/driver_manager.cpp), so it would segfault against a 1.0.0
// driver. We ship our own driver manager because DuckDB dropped it in v1.5.
static void SetStatementOption(AdbcStatement *statement, const char *key, int64_t value) {
	AdbcError error;
	std::memset(&error, 0, sizeof(error));
	auto value_str = std::to_string(value);
	if (AdbcStatementSetOption(statement, key, value_str.c_str(), &error) != ADBC_STATUS_OK) {
		std::string msg = "Failed to set ADBC statement option '" + std::string(key) + "' to " + value_str + ": ";
		if (error.message) {
			msg += error.message;
			if (error.release) {
				error.release(&error);
			}
		}
		throw IOException(msg);
	}
}

void ApplyDriverSettings(const SnowflakeDriverSettings &settings, AdbcStatement *statement) {
	if (settings.result_queue_size > 0) {
		SetStatementOption(statement, "adbc.rpc.result_queue_size", settings.result_queue_size);
	}
	if (settings.prefetch_concurrency > 0) {
		SetStatementOption(statement, "adbc.snowflake.rpc.prefetch_concurrency", settings.prefetch_concurrency);
	}
}

} // namespace snowflake
} // namespace duckdb
