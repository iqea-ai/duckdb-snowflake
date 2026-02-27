#include "storage/snowflake_catalog_set.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {
namespace snowflake {
optional_ptr<CatalogEntry> SnowflakeCatalogSet::GetEntry(ClientContext &context, const string &name) {
	TryLoadEntries(context);

	lock_guard<mutex> lock(entry_lock);
	// Case-insensitive lookup to match DuckDB convention
	for (const auto &entry : entries) {
		if (StringUtil::CIEquals(entry.first, name)) {
			return entry.second.get();
		}
	}

	return nullptr;
}

void SnowflakeCatalogSet::Scan(ClientContext &context, const std::function<void(CatalogEntry &)> &callback) {
	TryLoadEntries(context);

	lock_guard<mutex> lock(entry_lock);
	for (const auto &entry : entries) {
		callback(*entry.second);
	}
}

void SnowflakeCatalogSet::TryLoadEntries(ClientContext &context) {
	lock_guard<mutex> lock(load_lock);
	if (is_loaded) {
		return;
	}
	LoadEntries(context);
	is_loaded = true;
}
} // namespace snowflake
} // namespace duckdb
