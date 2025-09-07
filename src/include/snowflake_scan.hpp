#include "duckdb.hpp"
#include "snowflake_arrow_utils.hpp"

namespace duckdb {
namespace snowflake {

// SnowflakeScanBindData inherits from ArrowScanFunctionData to leverage DuckDB's native Arrow integration
// This allows us to use all of DuckDB's Arrow scanning infrastructure without reimplementing it
struct SnowflakeScanBindData : public ArrowScanFunctionData {
	// The factory holds the ADBC connection and statement, keeping them alive during the scan
	unique_ptr<SnowflakeArrowStreamFactory> factory;

	explicit SnowflakeScanBindData(unique_ptr<SnowflakeArrowStreamFactory> factory_p)
	    : ArrowScanFunctionData(SnowflakeProduceArrowScan, reinterpret_cast<uintptr_t>(factory_p.get())),
	      factory(std::move(factory_p)) {
		// ArrowScanFunctionData constructor takes:
		// 1. A function pointer to produce ArrowArrayStreamWrapper instances
		// 2. A pointer to the factory that will be passed to that function
	}
};

static unique_ptr<FunctionData> SnowflakeScanBind(ClientContext &context, TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types, vector<string> &names);

} // namespace snowflake

TableFunction GetSnowflakeScanFunction();

} // namespace duckdb
