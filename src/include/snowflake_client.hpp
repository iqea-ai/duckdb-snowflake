#pragma once

#include "duckdb.hpp"
#include "snowflake_config.hpp"

#include "duckdb/common/adbc/adbc.h"
// Note: driver_manager functions are provided by DuckDB's build

namespace duckdb {
namespace snowflake {

struct SnowflakeColumn {
	string name;
	LogicalType type;
	bool is_nullable;
};

class SnowflakeClient {
public:
	SnowflakeClient();
	~SnowflakeClient();

	void Connect(const SnowflakeConfig &config);
	void Disconnect();
	bool IsConnected() const;
	bool TestConnection();

	AdbcConnection *GetConnection() {
		return &connection;
	}
	AdbcDatabase *GetDatabase() {
		return &database;
	}
	const SnowflakeConfig &GetConfig() const;

	vector<string> ListSchemas(ClientContext &context);
	vector<string> ListTables(ClientContext &context, const string &schema);
	vector<SnowflakeColumn> GetTableInfo(ClientContext &context, const string &schema, const string &table_name);
	idx_t GetTableRowCount(ClientContext &context, const string &schema, const string &table);

private:
	SnowflakeConfig config;
	AdbcDatabase database;
	AdbcConnection connection;
	bool connected = false;

	vector<vector<string>> ExecuteAndGetStrings(ClientContext &context, const string &query,
	                                            const vector<string> &expected_col_names);
	unique_ptr<DataChunk> ExecuteAndGetChunk(ClientContext &context, const string &query,
	                                         const vector<LogicalType> &expected_types,
	                                         const vector<string> &expected_names);
	void InitializeDatabase(const SnowflakeConfig &config);
	void InitializeConnection();
	void CheckError(const AdbcStatusCode status, const std::string &operation, AdbcError *error);
};

} // namespace snowflake
} // namespace duckdb
