#include "snowflake_config.hpp"
#include "duckdb/common/exception.hpp"

#include <sstream>
#include <regex>

namespace duckdb {
namespace snowflake {

SnowflakeConfig SnowflakeConfig::ParseConnectionString(const std::string &connection_string) {
	SnowflakeConfig config;

	// Parse key=value pairs separated by semicolons
	std::regex param_regex("([^=;]+)=([^;]*)");
	auto params_begin = std::sregex_iterator(connection_string.begin(), connection_string.end(), param_regex);
	auto params_end = std::sregex_iterator();

	for (auto it = params_begin; it != params_end; ++it) {
		std::string key = (*it)[1].str();
		std::string value = (*it)[2].str();

		if (key == "account") {
			config.account = value;
		} else if (key == "username" || key == "user") {
			config.username = value;
		} else if (key == "password") {
			config.password = value;
		} else if (key == "warehouse") {
			config.warehouse = value;
		} else if (key == "database") {
			config.database = value;
		}
		// else if (key == "schema") {
		// 	config.schema = value;
		// }
		else if (key == "role") {
			config.role = value;
		} else if (key == "auth_type") {
			if (value == "password") {
				config.auth_type = SnowflakeAuthType::PASSWORD;
			} else if (value == "oauth") {
				config.auth_type = SnowflakeAuthType::OAUTH;
			} else if (value == "key_pair") {
				config.auth_type = SnowflakeAuthType::KEY_PAIR;
			} else if (value == "workload_identity") {
				config.auth_type = SnowflakeAuthType::WORKLOAD_IDENTITY;
			}
		} else if (key == "token") {
			config.oauth_token = value;
		} else if (key == "oidc_token") {
			config.oidc_token = value;
		} else if (key == "token_file_path") {
			config.token_file_path = value;
		} else if (key == "workload_identity_provider") {
			config.workload_identity_provider = value;
		} else if (key == "private_key") {
			config.private_key = value;
		} else if (key == "query_timeout") {
			config.query_timeout = std::stoi(value);
		} else if (key == "keep_alive") {
			config.keep_alive = (value == "true" || value == "1");
		} else if (key == "use_high_precision") {
			config.use_high_precision = (value == "true" || value == "1");
		}
	}

	// Validate required fields
	if (config.account.empty()) {
		throw InvalidInputException("Snowflake connection string missing required 'account' parameter");
	}

	return config;
}

std::string SnowflakeConfig::ToString() const {
	std::ostringstream oss;
	oss << "account=" << account << ";";
	oss << "user=" << username << ";";
	oss << "password=" << password << ";";
	oss << "database=" << database << ";";
	if (!warehouse.empty()) {
		oss << "warehouse=" << warehouse << ";";
	}
	if (!role.empty()) {
		oss << "role=" << role << ";";
	}
	if (auth_type == SnowflakeAuthType::OAUTH) {
		oss << "auth_type=oauth;";
		oss << "token=" << oauth_token << ";";
	} else if (auth_type == SnowflakeAuthType::KEY_PAIR) {
		oss << "auth_type=key_pair;";
		oss << "private_key=" << private_key << ";";
	} else if (auth_type == SnowflakeAuthType::WORKLOAD_IDENTITY) {
		oss << "auth_type=workload_identity;";
		if (!token_file_path.empty()) {
			oss << "token_file_path=" << token_file_path << ";";
		} else if (!oidc_token.empty()) {
			oss << "oidc_token=" << oidc_token << ";";
		}
		if (!workload_identity_provider.empty()) {
			oss << "workload_identity_provider=" << workload_identity_provider << ";";
		}
	}
	oss << "query_timeout=" << query_timeout << ";";
	oss << "keep_alive=" << (keep_alive ? "true" : "false") << ";";
	oss << "use_high_precision=" << (use_high_precision ? "true" : "false") << ";";
	return oss.str();
}

bool SnowflakeConfig::operator==(const SnowflakeConfig &other) const {
	return (account == other.account && username == other.username && password == other.password &&
	        warehouse == other.warehouse && database == other.database && role == other.role &&
	        auth_type == other.auth_type && oauth_token == other.oauth_token && private_key == other.private_key &&
	        query_timeout == other.query_timeout && keep_alive == other.keep_alive);
}

} // namespace snowflake
} // namespace duckdb
