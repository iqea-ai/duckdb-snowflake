#pragma once

#include "snowflake_client.hpp"
#include "snowflake_config.hpp"
#include "duckdb/storage/object_cache.hpp"

#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace duckdb {
namespace snowflake {

class SnowflakeClientManager;

//! RAII lease for a pooled SnowflakeClient. While the lease is alive it has
//! exclusive use of one ADBC connection; on destruction the connection is
//! returned to the manager's idle pool for reuse (unless invalidated). ADBC
//! connections are not thread-safe, so at most one live lease may drive a given
//! connection at a time — which is precisely what the pool guarantees.
class ConnectionLease {
public:
	ConnectionLease() = default;
	ConnectionLease(SnowflakeClientManager &manager, SnowflakeConfig config, shared_ptr<SnowflakeClient> client)
	    : manager(&manager), config(std::move(config)), client(std::move(client)) {
	}
	~ConnectionLease();

	ConnectionLease(ConnectionLease &&other) noexcept
	    : manager(other.manager), config(std::move(other.config)), client(std::move(other.client)),
	      reusable(other.reusable) {
		other.manager = nullptr;
		other.client = nullptr;
	}
	ConnectionLease &operator=(ConnectionLease &&other) noexcept {
		if (this != &other) {
			Reset();
			manager = other.manager;
			config = std::move(other.config);
			client = std::move(other.client);
			reusable = other.reusable;
			other.manager = nullptr;
			other.client = nullptr;
		}
		return *this;
	}
	ConnectionLease(const ConnectionLease &) = delete;
	ConnectionLease &operator=(const ConnectionLease &) = delete;

	SnowflakeClient *operator->() const {
		return client.get();
	}
	SnowflakeClient &operator*() const {
		return *client;
	}
	SnowflakeClient *get() const {
		return client.get();
	}
	explicit operator bool() const {
		return client != nullptr;
	}

	//! Mark this connection as not reusable (e.g. after an auth/session error);
	//! it is dropped instead of returned to the idle pool when the lease ends.
	void Invalidate() {
		reusable = false;
	}

private:
	void Reset();

	SnowflakeClientManager *manager = nullptr;
	SnowflakeConfig config;
	shared_ptr<SnowflakeClient> client;
	bool reusable = true;
};

//! Process-wide pool of Snowflake ADBC connections, keyed by credentials.
//! Acquire() hands out a dedicated connection per concurrent scan / metadata
//! call; the connection is returned to a per-config idle list for reuse when its
//! lease is destroyed. There is intentionally NO cap on concurrently-active
//! leases: a single DuckDB query can legitimately hold several Arrow scan
//! streams open at once (e.g. UNION ALL with preserve_insertion_order=false), so
//! capping active connections would deadlock. Only the number of retained *idle*
//! connections is bounded.
class SnowflakeClientManager {
public:
	static SnowflakeClientManager &GetInstance();

	//! Acquire an exclusive connection for the given config, reusing a validated
	//! idle connection if available, otherwise creating and connecting a new one.
	//! The returned lease returns the connection to the pool on destruction.
	ConnectionLease Acquire(const SnowflakeConfig &config);

	//! Close and forget all idle connections for a config (e.g. on DETACH, or
	//! after an auth error). Active leases keep their own connection alive until
	//! released, at which point they are dropped rather than pooled if stale.
	void DrainIdle(const SnowflakeConfig &config);

	//! Close and forget every idle connection, for every config. Called when a
	//! DuckDB database closes (see SnowflakePoolCloseGuard), so pooled ADBC
	//! handles are released while the process is still fully alive (issue #69).
	void DrainAll();

private:
	friend class ConnectionLease;
	SnowflakeClientManager() = default;

	//! Return a connection to the idle pool (called by ~ConnectionLease).
	void Return(const SnowflakeConfig &config, shared_ptr<SnowflakeClient> client, bool reusable);

	//! Cap on retained idle connections per config. Active leases are unbounded.
	static constexpr size_t MAX_IDLE_PER_CONFIG = 8;

	std::unordered_map<SnowflakeConfig, std::vector<shared_ptr<SnowflakeClient>>, SnowflakeConfigHash> idle_connections;
	std::mutex pool_mutex;
};

//! Drains the connection pool when the DuckDB database that loaded the
//! extension closes. Registered as a non-evictable ObjectCache entry, which
//! ~DatabaseInstance destroys after its catalogs and client connections, so every
//! scan has already returned its lease (issue #69). Without this, the idle
//! connections a snowflake_query() leaves behind would only be released at
//! process exit, and on Windows that call into the Go driver never returns.
class SnowflakePoolCloseGuard : public ObjectCacheEntry {
public:
	~SnowflakePoolCloseGuard() override;

	static string ObjectType() {
		return "snowflake_connection_pool_guard";
	}
	string GetObjectType() override {
		return ObjectType();
	}
	//! Invalid index = never evicted; it must live exactly as long as the database.
	optional_idx GetEstimatedCacheMemory() const override {
		return optional_idx();
	}
};

} // namespace snowflake
} // namespace duckdb
