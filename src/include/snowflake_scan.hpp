#include "duckdb.hpp"
#include "snowflake_arrow_utils.hpp"

namespace duckdb {
namespace snowflake {

// SnowflakeScanBindData inherits from ArrowScanFunctionData to leverage
// DuckDB's native Arrow integration This allows us to use all of DuckDB's Arrow
// scanning infrastructure without reimplementing it
struct SnowflakeScanBindData : public ArrowScanFunctionData {
	// The factory holds the ADBC connection and statement, keeping them alive
	// during the scan. Same object as ArrowScanFunctionData::factory, typed.
	shared_ptr<SnowflakeArrowStreamFactory> snowflake_factory;

	explicit SnowflakeScanBindData(const shared_ptr<SnowflakeArrowStreamFactory> &factory_p)
	    : ArrowScanFunctionData(factory_p), snowflake_factory(factory_p) {
	}
};

// static unique_ptr<FunctionData> SnowflakeScanBind(ClientContext &context,
// TableFunctionBindInput &input,
//                                                   vector<LogicalType>
//                                                   &return_types,
//                                                   vector<string> &names);

} // namespace snowflake

// Get the snowflake_scan table function (pushdown disabled - user controls
// query)
TableFunction GetSnowflakeScanFunction();

// Get the table scan function for ATTACH
// enable_pushdown: whether to enable filter and projection pushdown
TableFunction GetSnowflakeTableScanFunction(bool enable_pushdown);

} // namespace duckdb
