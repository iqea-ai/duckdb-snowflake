#include "snowflake_functions.hpp"
#include "snowflake_secrets.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/common/exception.hpp"
#include <string>

namespace duckdb {

void SnowflakeStoreCredentialsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);

	auto &profile_name = args.data[0];
	auto &username = args.data[1];
	auto &password = args.data[2];
	auto &account = args.data[3];
	auto &warehouse = args.data[4];
	auto &database = args.data[5];
	auto &schema = args.data[6];

	if (profile_name.GetValue(0).IsNull()) {
		result.SetValue(0, Value("Error: Profile name cannot be null"));
		return;
	}

	std::string profile = profile_name.GetValue(0).GetValue<std::string>();
	std::string user = username.GetValue(0).GetValue<std::string>();
	std::string pass = password.GetValue(0).GetValue<std::string>();
	std::string acc = account.GetValue(0).GetValue<std::string>();
	std::string wh = warehouse.GetValue(0).GetValue<std::string>();
	std::string db = database.GetValue(0).GetValue<std::string>();
	std::string sch = schema.GetValue(0).GetValue<std::string>();

	try {
		// Store credentials using the new secrets manager
		SnowflakeSecretsHelper::StoreCredentials(state.GetContext(), profile, user, pass, acc, wh, db, sch);

		std::string result_msg = "Credentials stored successfully for profile: " + profile + " (username: " + user +
		                         ", account: " + acc + ", warehouse: " + wh + ", database: " + db + ", schema: " + sch +
		                         ")";
		result.SetValue(0, Value(result_msg));
	} catch (const std::exception &e) {
		result.SetValue(0, Value("Error storing credentials: " + std::string(e.what())));
	}
}

void SnowflakeListProfilesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);

	try {
		auto profiles = SnowflakeSecretsHelper::ListProfiles(state.GetContext());

		std::string profiles_str = "Available profiles: ";
		if (profiles.empty()) {
			profiles_str = "No profiles stored";
		} else {
			for (const auto &profile : profiles) {
				profiles_str += profile + ", ";
			}
			profiles_str = profiles_str.substr(0, profiles_str.length() - 2); // Remove last ", "
		}

		result.SetValue(0, Value(profiles_str));
	} catch (const std::exception &e) {
		result.SetValue(0, Value("Error listing profiles: " + std::string(e.what())));
	}
}

void SnowflakeValidateCredentialsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	result.SetVectorType(VectorType::CONSTANT_VECTOR);

	auto &profile_name = args.data[0];

	if (profile_name.GetValue(0).IsNull()) {
		result.SetValue(0, Value::BOOLEAN(false));
		return;
	}

	std::string profile = profile_name.GetValue(0).GetValue<std::string>();

	try {
		// Validate credentials using our new validation method
		bool is_valid = SnowflakeSecretsHelper::ValidateCredentials(state.GetContext(), profile, 10);
		result.SetValue(0, Value::BOOLEAN(is_valid));
	} catch (const std::exception &e) {
		// If validation fails, return false
		result.SetValue(0, Value::BOOLEAN(false));
	}
}

ScalarFunction GetSnowflakeStoreCredentialsFunction() {
	return ScalarFunction("snowflake_store_credentials",
	                      {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR,
	                       LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR},
	                      LogicalType::VARCHAR, SnowflakeStoreCredentialsFunction);
}

ScalarFunction GetSnowflakeListProfilesFunction() {
	return ScalarFunction("snowflake_list_profiles", {}, LogicalType::VARCHAR, SnowflakeListProfilesFunction);
}

ScalarFunction GetSnowflakeValidateCredentialsFunction() {
	return ScalarFunction("snowflake_validate_credentials", {LogicalType::VARCHAR}, LogicalType::BOOLEAN,
	                      SnowflakeValidateCredentialsFunction);
}

} // namespace duckdb
