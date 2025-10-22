#pragma once
#include <string>
#include <functional>
#include <cstdint>

namespace duckdb {
namespace snowflake {

enum class SnowflakeAuthType {
	PASSWORD,          // Username and password authentication
	OAUTH,             // OAuth token authentication (pre-obtained token)
	KEY_PAIR,          // Key pair/JWT authentication (not supported)
	WORKLOAD_IDENTITY, // Workload identity federation (not supported)
	EXTERNAL_OAUTH     // External OAuth flow (Auth0, Okta, etc.) - requires token acquisition
};

struct SnowflakeConfig {
	std::string account;
	std::string warehouse;
	std::string database;
	std::string role;
	SnowflakeAuthType auth_type = SnowflakeAuthType::PASSWORD;
	std::string username;
	std::string password;
	std::string oauth_token;
	std::string private_key;
	std::string oidc_token;
	std::string token_file_path;
	std::string workload_identity_provider;

	// OIDC configuration
	std::string oidc_client_id;
	std::string oidc_issuer_url;
	std::string oidc_redirect_uri;
	std::string oidc_scope = "openid";

	int32_t query_timeout = 300; // seconds
	bool keep_alive = true;
	bool use_high_precision = true; // When false, DECIMAL(p,0) converts to INT64

	static SnowflakeConfig ParseConnectionString(const std::string &connection_string);

	// Generate connection string from config
	std::string ToString() const;

	bool operator==(const SnowflakeConfig &other) const;
};

// Custom hash function for SnowflakeConfig to use it as a map key
struct SnowflakeConfigHash {
private:
	// Golden ratio constant used in hash combining (same as boost::hash_combine)
	// This is (phi - 1) * 2^32 where phi is the golden ratio
	static constexpr std::size_t GOLDEN_RATIO_HASH = 0x9e3779b9;

	// Helper function to combine hash values (similar to boost::hash_combine)
	template <typename T>
	void HashCombine(std::size_t &seed, const T &value) const {
		seed ^= std::hash<T> {}(value) + GOLDEN_RATIO_HASH + (seed << 6) + (seed >> 2);
	}

public:
	std::size_t operator()(const SnowflakeConfig &config) const {
		std::size_t seed = 0;

		HashCombine(seed, config.account);
		HashCombine(seed, config.warehouse);
		HashCombine(seed, config.database);
		HashCombine(seed, config.role);
		HashCombine(seed, static_cast<int>(config.auth_type));
		HashCombine(seed, config.username);
		HashCombine(seed, config.password);
		HashCombine(seed, config.oauth_token);
		HashCombine(seed, config.private_key);
		HashCombine(seed, config.oidc_token);
		HashCombine(seed, config.token_file_path);
		HashCombine(seed, config.workload_identity_provider);
		HashCombine(seed, config.oidc_client_id);
		HashCombine(seed, config.oidc_issuer_url);
		HashCombine(seed, config.oidc_redirect_uri);
		HashCombine(seed, config.oidc_scope);

		return seed;
	}
};

} // namespace snowflake
} // namespace duckdb
