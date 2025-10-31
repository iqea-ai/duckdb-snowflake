#include "snowflake_client_manager.hpp"

namespace duckdb {
namespace snowflake {

SnowflakeClientManager &SnowflakeClientManager::GetInstance() {
  static SnowflakeClientManager instance;
  return instance;
}

shared_ptr<SnowflakeClient>
SnowflakeClientManager::GetConnection(const SnowflakeConfig &config) {
  std::lock_guard<std::mutex> lock(connection_mutex);

  auto it = connections.find(config);
  if (it != connections.end() && it->second->IsConnected()) {
    return it->second;
  }

  // Create new connection
  auto connection = make_shared_ptr<SnowflakeClient>();
  connection->Connect(config);

  connections[config] = connection;
  return connection;
}

void SnowflakeClientManager::ReleaseConnection(const SnowflakeConfig &config) {
  std::lock_guard<std::mutex> lock(connection_mutex);
  connections.erase(config);
}

} // namespace snowflake
} // namespace duckdb
