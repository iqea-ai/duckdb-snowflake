#include "snowflake_debug.hpp"
#include "snowflake_client.hpp"
#include "snowflake_types.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/function/table/arrow/arrow_duck_schema.hpp"
#include "duckdb/common/string_util.hpp"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define access _access
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace duckdb {
namespace snowflake {

// Helper function to check if a file exists
static bool FileExists(const std::string &path) {
#ifdef _WIN32
	return (access(path.c_str(), 0) == 0);
#else
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
#endif
}

// Get the directory where the current extension is located
static std::string GetExtensionDirectory() {
#ifdef _WIN32
	HMODULE hModule = NULL;
	// Get handle to the module containing this function
	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                      reinterpret_cast<LPCTSTR>(&GetExtensionDirectory), &hModule)) {
		char path[MAX_PATH];
		if (GetModuleFileNameA(hModule, path, sizeof(path))) {
			std::string full_path(path);
			// Find the last directory separator
			size_t last_sep = full_path.find_last_of("/\\");
			std::string dir = (last_sep != std::string::npos) ? full_path.substr(0, last_sep) : ".";

			DPRINT("GetExtensionDirectory: module path = %s\n", path);
			DPRINT("GetExtensionDirectory: parent_path = %s\n", dir.c_str());

			return dir;
		}
	}
	// Fallback to current directory
	return ".";
#else
	Dl_info info;
	// Use a function from this library to get its path
	if (dladdr(reinterpret_cast<void *>(&GetExtensionDirectory), &info)) {
		std::string path(info.dli_fname);
		// Find the last directory separator
		size_t last_sep = path.find_last_of("/\\");
		std::string dir = (last_sep != std::string::npos) ? path.substr(0, last_sep) : ".";

		DPRINT("GetExtensionDirectory: dli_fname = %s\n", info.dli_fname);
		DPRINT("GetExtensionDirectory: parent_path = %s\n", dir.c_str());

		return dir;
	}
	// Fallback to current directory
	return ".";
#endif
}

SnowflakeClient::SnowflakeClient() {
	std::memset(&database, 0, sizeof(database));
	std::memset(&connection, 0, sizeof(connection));
}

SnowflakeClient::~SnowflakeClient() {
	Disconnect();
}

void SnowflakeClient::Connect(const SnowflakeConfig &config) {
	if (connected) {
		Disconnect();
	}

	this->config = config;
	InitializeDatabase(config);
	InitializeConnection();
	connected = true;
}

void SnowflakeClient::Disconnect() {
	if (!connected) {
		return;
	}

	AdbcError error;
	std::memset(&error, 0, sizeof(error));
	AdbcStatusCode status;

	status = AdbcConnectionRelease(&connection, &error);
	CheckError(status, "Failed to release ADBC connection", &error);

	status = AdbcDatabaseRelease(&database, &error);
	CheckError(status, "Failed to release ADBC database", &error);

	connected = false;
}

bool SnowflakeClient::IsConnected() const {
	return connected;
}

const SnowflakeConfig &SnowflakeClient::GetConfig() const {
	return config;
}

void SnowflakeClient::InitializeDatabase(const SnowflakeConfig &config) {
	AdbcError error;
	std::memset(&error, 0, sizeof(error));

	AdbcStatusCode status = AdbcDatabaseNew(&database, &error);
	CheckError(status, "Failed to create ADBC database", &error);

	// Use ADBC driver manager to load the Snowflake driver dynamically
	// Try multiple locations for the driver
	std::vector<std::string> search_paths;

	// 1. Try the extension directory
	std::string extension_dir = GetExtensionDirectory();
	search_paths.push_back(extension_dir + "/" + SNOWFLAKE_ADBC_LIB);

	// 2. Try adbc_drivers subdirectory relative to extension
	search_paths.push_back(extension_dir + "/adbc_drivers/" + SNOWFLAKE_ADBC_LIB);

	// 3. Try the build directory structure
	search_paths.push_back(extension_dir + "/../../../adbc_drivers/" + SNOWFLAKE_ADBC_LIB);

	// 4. Try system paths
	search_paths.push_back(std::string("/usr/local/lib/") + SNOWFLAKE_ADBC_LIB);
	search_paths.push_back(std::string("/usr/lib/") + SNOWFLAKE_ADBC_LIB);

	// 5. Try just the filename - let the system search for it
	search_paths.emplace_back(SNOWFLAKE_ADBC_LIB);

	// Find the first existing driver
	std::string driver_path;
	for (const auto &path : search_paths) {
		DPRINT("Checking for driver at: %s\n", path.c_str());
		if (FileExists(path)) {
			driver_path = path;
			DPRINT("Found driver at: %s\n", driver_path.c_str());
			break;
		}
	}

	if (driver_path.empty()) {
		// Use the filename and hope it's in the library path
		driver_path = SNOWFLAKE_ADBC_LIB;
		DPRINT("Driver not found in search paths, using: %s\n", driver_path.c_str());
	}

	DPRINT("Snowflake ADBC Driver Loading:\n");
	DPRINT("Extension directory: %s\n", extension_dir.c_str());
	DPRINT("Final driver path: %s\n", driver_path.c_str());

	status = AdbcDatabaseSetOption(&database, "driver", driver_path.c_str(), &error);
	CheckError(status, "Failed to set Snowflake driver path", &error);

	// Set connection parameters
	status = AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.account", config.account.c_str(), &error);
	CheckError(status, "Failed to set account", &error);

	if (!config.username.empty()) {
		status = AdbcDatabaseSetOption(&database, "username", config.username.c_str(), &error);
		CheckError(status, "Failed to set username", &error);
	}

	// Set authentication based on type
	switch (config.auth_type) {
	case SnowflakeAuthType::PASSWORD:
		if (!config.password.empty()) {
			status = AdbcDatabaseSetOption(&database, "password", config.password.c_str(), &error);
			CheckError(status, "Failed to set password", &error);
		}
		break;
	case SnowflakeAuthType::OAUTH:
		if (!config.oauth_token.empty()) {
			status =
			    AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.auth_token", config.oauth_token.c_str(), &error);
			CheckError(status, "Failed to set OAuth token", &error);
		}
		break;
	case SnowflakeAuthType::KEY_PAIR:
		if (!config.private_key.empty()) {
			status =
			    AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.private_key", config.private_key.c_str(), &error);
			CheckError(status, "Failed to set private key", &error);
		}
		break;
	}

	// Set optional parameters
	if (!config.warehouse.empty()) {
		status = AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.warehouse", config.warehouse.c_str(), &error);
		CheckError(status, "Failed to set warehouse", &error);
	}

	if (!config.database.empty()) {
		status = AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.database", config.database.c_str(), &error);
		CheckError(status, "Failed to set database", &error);
	}

	if (!config.role.empty()) {
		status = AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.role", config.role.c_str(), &error);
		CheckError(status, "Failed to set role", &error);
	}

	// Set query timeout
	std::string timeout_str = std::to_string(config.query_timeout);
	status = AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.client_session_keep_alive",
	                               config.keep_alive ? "true" : "false", &error);
	CheckError(status, "Failed to set keep alive", &error);

	// Set high precision mode (when false, DECIMAL(p,0) converts to INT64)
	status = AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.client_option.use_high_precision",
	                               config.use_high_precision ? "true" : "false", &error);
	CheckError(status, "Failed to set high precision mode", &error);

	// Initialize the database
	status = AdbcDatabaseInit(&database, &error);
	CheckError(status, "Failed to initialize database", &error);
}

void SnowflakeClient::InitializeConnection() {
	AdbcError error;
	std::memset(&error, 0, sizeof(error));
	AdbcStatusCode status = AdbcConnectionNew(&connection, &error);
	CheckError(status, "Failed to create connection", &error);

	status = AdbcConnectionInit(&connection, &database, &error);
	CheckError(status, "Failed to initialize connection", &error);
}

bool SnowflakeClient::TestConnection() {
	if (!IsConnected()) {
		return false;
	}

	// Test the connection with a simple query
	AdbcStatement statement;
	AdbcError error_obj;
	std::memset(&error_obj, 0, sizeof(error_obj));
	std::memset(&statement, 0, sizeof(statement));

	AdbcStatusCode status = AdbcStatementNew(&connection, &statement, &error_obj);
	if (status != ADBC_STATUS_OK) {
		if (error_obj.release) {
			error_obj.release(&error_obj);
		}
		return false;
	}

	// Execute simple test query
	status = AdbcStatementSetSqlQuery(&statement, "SELECT 1", &error_obj);
	if (status != ADBC_STATUS_OK) {
		AdbcStatementRelease(&statement, &error_obj);
		if (error_obj.release) {
			error_obj.release(&error_obj);
		}
		return false;
	}

	ArrowArrayStream stream;
	std::memset(&stream, 0, sizeof(stream));
	status = AdbcStatementExecuteQuery(&statement, &stream, nullptr, &error_obj);
	bool success = (status == ADBC_STATUS_OK);

	// Clean up
	if (stream.release) {
		stream.release(&stream);
	}
	AdbcStatementRelease(&statement, &error_obj);
	if (error_obj.release) {
		error_obj.release(&error_obj);
	}

	return success;
}

void SnowflakeClient::CheckError(const AdbcStatusCode status, const std::string &operation, AdbcError *error) {
	if (status == ADBC_STATUS_OK) {
		return;
	}

	std::string error_message = operation + ": " + ((error && error->message) ? error->message : "Unknown ADBC error.");

	// Print to console for debugging
	DPRINT("Error] %s\n", error_message.c_str());

	// Add more context based on common error patterns
	if (error && error->message) {
		std::string msg(error->message);
		if (msg.find("authentication") != std::string::npos || msg.find("Authentication") != std::string::npos) {
			DPRINT("username and password\n");
		} else if (msg.find("account") != std::string::npos || msg.find("Account") != std::string::npos) {
			DPRINT("account identifier format (e.g., 'account-name.region')\n");
		} else if (msg.find("warehouse") != std::string::npos || msg.find("Warehouse") != std::string::npos) {
			DPRINT("warehouse name and permissions\n");
		} else if (msg.find("database") != std::string::npos || msg.find("Database") != std::string::npos) {
			DPRINT("database name and permissions\n");
		} else if (msg.find("network") != std::string::npos || msg.find("Network") != std::string::npos ||
		           msg.find("connection") != std::string::npos || msg.find("Connection") != std::string::npos) {
			DPRINT("network connectivity and firewall settings\n");
		}
	}

	// Only release if the error has a release function and hasn't been released already
	if (error && error->release && error->message) {
		error->release(error);
	}

	throw IOException(error_message);
}

vector<string> SnowflakeClient::ListSchemas(ClientContext &context) {
	const string schema_query = "SELECT schema_name FROM " + config.database + ".INFORMATION_SCHEMA.SCHEMATA";
	auto result = ExecuteAndGetStrings(context, schema_query, {"schema_name"});
	auto schemas = result[0];

	for (auto &schema : schemas) {
		schema = StringUtil::Lower(schema);
	}

	return schemas;
}

vector<string> SnowflakeClient::ListTables(ClientContext &context, const string &schema = "") {
	DPRINT("ListTables called for schema: %s in database: %s\n", schema.c_str(), config.database.c_str());
	const string upper_schema = StringUtil::Upper(schema);
	const string table_name_query = "SELECT table_name FROM " + config.database + ".information_schema.tables" +
	                                (!schema.empty() ? " WHERE table_schema = '" + upper_schema + "'" : "");
	DPRINT("Table query: %s\n", table_name_query.c_str());

	auto result = ExecuteAndGetStrings(context, table_name_query, {"table_name"});
	auto table_names = result[0];

	for (auto &table_name : table_names) {
		table_name = StringUtil::Lower(table_name);
	}

	DPRINT("ListTables returning %zu tables\n", table_names.size());
	for (const auto &table_name : table_names) {
		DPRINT("Found table: %s\n", table_name.c_str());
	}
	return table_names;
}

vector<SnowflakeColumn> SnowflakeClient::GetTableInfo(ClientContext &context, const string &schema,
                                                      const string &table_name) {
	const string upper_schema = StringUtil::Upper(schema);
	const string upper_table = StringUtil::Upper(table_name);

	const string table_info_query = "SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE FROM " + config.database +
	                                ".information_schema.columns WHERE table_schema = '" + upper_schema +
	                                "' AND table_name = '" + upper_table + "' ORDER BY ORDINAL_POSITION";

	DPRINT("GetTableInfo query: %s\n", table_info_query.c_str());
	const vector<string> expected_names = {"COLUMN_NAME", "DATA_TYPE", "IS_NULLABLE"};

	auto result = ExecuteAndGetStrings(context, table_info_query, expected_names);

	if (result.empty() || result[0].empty()) {
		throw CatalogException("Cannot retrieve column information for table '%s.%s'. "
		                       "The table may have been dropped or you may lack permissions.",
		                       schema, table_name);
	}

	vector<SnowflakeColumn> col_data;

	for (idx_t row_idx = 0; row_idx < result[0].size(); row_idx++) {
		string column_name = StringUtil::Lower(result[0][row_idx]);
		string data_type = result[1][row_idx];
		string nullable = result[2][row_idx];

		bool is_nullable = (nullable == "YES");
		LogicalType duckdb_type = SnowflakeTypeToLogicalType(data_type);

		SnowflakeColumn new_col = {column_name, duckdb_type, is_nullable};
		col_data.emplace_back(new_col);
	}

	return col_data;
}

vector<vector<string>> SnowflakeClient::ExecuteAndGetStrings(ClientContext &context, const string &query,
                                                             const vector<string> &expected_col_names) {
	if (!connected) {
		throw IOException("Connection must be created before ListTables is called");
	}

	AdbcStatement statement;
	std::memset(&statement, 0, sizeof(statement));
	AdbcError error;
	std::memset(&error, 0, sizeof(error));
	AdbcStatusCode status;

	DPRINT("ExecuteAndGetStrings: Query='%s'\n", query.c_str());
	DPRINT("About to create statement...\n");
	status = AdbcStatementNew(GetConnection(), &statement, &error);
	CheckError(status, "Failed to create AdbcStatement", &error);
	DPRINT("Statement created successfully\n");

	status = AdbcStatementSetSqlQuery(&statement, query.c_str(), &error);
	CheckError(status, "Failed to set AdbcStatement with SQL query: " + query, &error);

	ArrowArrayStream stream = {};
	int64_t rows_affected = -1;

	DPRINT("Executing statement\n");
	status = AdbcStatementExecuteQuery(&statement, &stream, &rows_affected, &error);
	CheckError(status, "Failed to execute AdbcStatement with SQL query: " + query, &error);

	ArrowSchema schema = {};
	int schema_result = stream.get_schema(&stream, &schema);
	if (schema_result != 0 || schema.release == nullptr) {
		throw IOException("Failed to get Arrow schema from stream");
	}

	ArrowSchemaWrapper schema_wrapper;
	schema_wrapper.arrow_schema = schema;

	if (!expected_col_names.empty()) {
		if (schema.n_children != static_cast<int64_t>(expected_col_names.size())) {
			throw IOException("Expected " + to_string(expected_col_names.size()) + " columns but got " +
			                  to_string(schema.n_children));
		}

		for (idx_t col_idx = 0; col_idx < expected_col_names.size(); col_idx++) {
			if (schema.children[col_idx]->name &&
			    !StringUtil::CIEquals(schema.children[col_idx]->name, expected_col_names[col_idx])) {
				throw IOException("Expected column '" + expected_col_names[col_idx] + "' but got '" +
				                  schema.children[col_idx]->name + "'");
			}
		}
	}

	vector<vector<string>> results(schema.n_children);

	while (true) {
		ArrowArray arrow_array;
		int return_code = stream.get_next(&stream, &arrow_array);

		if (return_code != 0) {
			throw IOException("ArrowArrayStream returned error code: " + std::to_string(return_code));
		}

		if (arrow_array.release == nullptr) {
			break;
		}

		ArrowArrayWrapper array_wrapper;
		array_wrapper.arrow_array = arrow_array;

		for (idx_t col_idx = 0; col_idx < arrow_array.n_children; col_idx++) {
			ArrowArray *column = arrow_array.children[col_idx];
			if (column && column->buffers && column->n_buffers >= 3) {
				// For string columns: buffer[0] is validity, buffer[1] is offsets, buffer[2] is data
				const int32_t *offsets = (const int32_t *)column->buffers[1];
				const char *data = (const char *)column->buffers[2];
				const uint8_t *validity = nullptr;

				if (column->buffers[0]) {
					validity = (const uint8_t *)column->buffers[0];
				}

				for (int64_t row_idx = 0; row_idx < column->length; row_idx++) {
					if (validity && column->null_count > 0) {
						size_t byte_idx = row_idx / 8;
						size_t bit_idx = row_idx % 8;
						bool is_valid = (validity[byte_idx] >> bit_idx) & 1;

						if (!is_valid) {
							// TODO possibly push NULL instead of ""?
							results[col_idx].push_back("");
							continue;
						}
					}

					int32_t start = offsets[row_idx];
					int32_t end = offsets[row_idx + 1];
					std::string value(data + start, end - start);
					results[col_idx].push_back(value);
				}
			}
		}
	}

	if (stream.release) {
		stream.release(&stream);
	}

	DPRINT("Releasing statement at %p\n", (void *)&statement);
	CheckError(AdbcStatementRelease(&statement, &error), "Failed to release AdbcStatement", &error);

	return results;
}

unique_ptr<DataChunk> SnowflakeClient::ExecuteAndGetChunk(ClientContext &context, const string &query,
                                                          const vector<LogicalType> &expected_types,
                                                          const vector<string> &expected_names) {
	DPRINT("ExecuteAndGetChunk called with query: %s\n", query.c_str());
	if (!connected) {
		DPRINT("ExecuteAndGetChunk: Not connected!\n");
		throw IOException("Connection must be created before ExecuteAndGetChunk is called");
	}
	DPRINT("ExecuteAndGetChunk: Connection is active\n");

	AdbcStatement statement;
	AdbcError error;
	AdbcStatusCode status;

	DPRINT("Creating ADBC statement...\n");
	status = AdbcStatementNew(GetConnection(), &statement, &error);
	CheckError(status, "Failed to create AdbcStatement", &error);
	DPRINT("ADBC statement created successfully\n");

	DPRINT("Setting SQL query on statement...\n");
	status = AdbcStatementSetSqlQuery(&statement, query.c_str(), &error);
	CheckError(status, "Failed to set AdbcStatement with SQL query: " + query, &error);
	DPRINT("SQL query set successfully\n");

	ArrowArrayStream stream = {};
	int64_t rows_affected = -1;

	DPRINT("Executing SQL query...\n");
	status = AdbcStatementExecuteQuery(&statement, &stream, &rows_affected, &error);
	CheckError(status, "Failed to execute AdbcStatement with SQL query: " + query, &error);
	DPRINT("SQL query executed successfully, rows_affected: %lld\n", rows_affected);

	DPRINT("Getting Arrow schema...\n");
	ArrowSchema schema = {};
	int schema_result = stream.get_schema(&stream, &schema);
	DPRINT("Arrow schema obtained, result: %d\n", schema_result);

	if (schema.release == nullptr) {
		DPRINT("Arrow schema is NULL!\n");
		throw IOException("Failed to get Arrow schema from stream");
	}

	if (static_cast<size_t>(schema.n_children) != expected_types.size()) {
		throw IOException("Schema has " + std::to_string(schema.n_children) + " columns but expected " +
		                  std::to_string(expected_types.size()));
	}

	ArrowSchemaWrapper schema_wrapper;
	schema_wrapper.arrow_schema = schema;

	// Use the new DuckDB API to populate the arrow table schema
	ArrowTableType arrow_table;
	vector<string> actual_names;
	vector<LogicalType> actual_types;
	ArrowTableFunction::PopulateArrowTableType(DBConfig::GetConfig(context), arrow_table,
	                                           schema_wrapper, actual_names, actual_types);

	if (actual_types.size() != expected_types.size()) {
		throw IOException("Schema mismatch: expected " + to_string(expected_types.size()) + " columns but got " +
		                  to_string(actual_types.size()));
	}

	for (size_t name_idx = 0; name_idx < expected_names.size(); name_idx++) {
		if (!StringUtil::CIEquals(expected_names[name_idx], actual_names[name_idx])) {
			throw IOException("Expected column '" + expected_names[name_idx] + "' at position " + to_string(name_idx) +
			                  " but got '" + actual_names[name_idx] + "'");
		}
	}

	vector<unique_ptr<DataChunk>> collected_chunks;
	int batch_count = 0;

	while (true) {
		ArrowArray arrow_array;
		DPRINT("Getting next Arrow batch %d...\n", batch_count);
		int return_code = stream.get_next(&stream, &arrow_array);

		if (return_code != 0) {
			DPRINT("ArrowArrayStream returned error code: %d\n", return_code);
			throw IOException("ArrowArrayStream returned error code: " + std::to_string(return_code));
		}

		if (arrow_array.release == nullptr) {
			DPRINT("No more Arrow batches\n");
			break;
		}

		if (arrow_array.null_count == arrow_array.length) {
			DPRINT("Arrow array is all nulls!\n");
		}

		for (int64_t i = 0; i < arrow_array.n_children; i++) {
			if (arrow_array.children[i]) {
				DPRINT("Child %lld: length=%lld, null_count=%lld\n", i, arrow_array.children[i]->length,
				       arrow_array.children[i]->null_count);
			}
		}

		DPRINT("Got Arrow batch %d with %lld rows\n", batch_count, arrow_array.length);
		batch_count++;

		auto temp_chunk = make_uniq<DataChunk>();
		temp_chunk->Initialize(Allocator::DefaultAllocator(), actual_types);

		auto array_wrapper = make_uniq<ArrowArrayWrapper>();
		array_wrapper->arrow_array = arrow_array;

		DPRINT("Arrow array details: n_buffers=%lld, n_children=%lld\n", arrow_array.n_buffers, arrow_array.n_children);

		DPRINT("Creating ArrowScanLocalState...\n");
		ArrowScanLocalState local_state(std::move(array_wrapper), context);
		DPRINT("ArrowScanLocalState initialized\n");
		DPRINT("Arrow table has %zu columns\n", arrow_table.GetColumns().size());

		for (size_t i = 0; i < actual_types.size(); i++) {
			DPRINT("Column %zu type: %s\n", i, actual_types[i].ToString().c_str());
		}

		try {
			ArrowTableFunction::ArrowToDuckDB(local_state, arrow_table.GetColumns(), *temp_chunk, batch_count - 1);
			DPRINT("ArrowToDuckDB completed, chunk size: %llu\n", temp_chunk->size());
		} catch (const std::exception &e) {
			DPRINT("ArrowToDuckDB failed: %s\n", e.what());
			throw;
		}

		collected_chunks.emplace_back(std::move(temp_chunk));
	}

	// optimization for small result sets
	if (collected_chunks.size() == 1) {
		DPRINT("Only one chunk read, skipping chunk consolidation\n");
		return std::move(collected_chunks[0]);
	}

	auto result_chunk = make_uniq<DataChunk>();
	result_chunk->Initialize(Allocator::DefaultAllocator(), actual_types);

	DPRINT("Collected %zu chunks, combining them...\n", collected_chunks.size());
	for (const auto &chunk : collected_chunks) {
		result_chunk->Append(*chunk);
	}
	DPRINT("Final result chunk has %llu rows\n", result_chunk->size());

	if (stream.release) {
		stream.release(&stream);
	}

	CheckError(AdbcStatementRelease(&statement, &error), "Failed to release AdbcStatement", &error);

	DPRINT("ExecuteAndGetChunk completed successfully\n");
	return result_chunk;
}
} // namespace snowflake
} // namespace duckdb
