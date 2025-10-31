#include "storage/snowflake_catalog_set.hpp"

namespace duckdb {
namespace snowflake {
optional_ptr<CatalogEntry> SnowflakeCatalogSet::GetEntry(ClientContext &context,
                                                         const string &name) {
  TryLoadEntries(context);

  lock_guard<mutex> lock(entry_lock);
  auto entry_it = entries.find(name);
  if (entry_it != entries.end()) {
    return entry_it->second.get();
  }

  return nullptr;
}

void SnowflakeCatalogSet::Scan(
    ClientContext &context,
    const std::function<void(CatalogEntry &)> &callback) {
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
