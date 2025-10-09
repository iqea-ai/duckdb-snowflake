# Predicate Pushdown Overview

## What is Predicate Pushdown?

Predicate pushdown is a query optimization technique that moves filter conditions from the client (DuckDB) to the data source (Snowflake) to reduce data transfer and improve performance.

## How It Works

### Without Pushdown
```sql
-- DuckDB retrieves ALL data from Snowflake, then filters locally
SELECT * FROM snowflake_scan('SELECT * FROM large_table', 'secret') 
WHERE column_name = 'value';
-- Result: 1,000,000 rows transferred, 1 row returned
```

### With Pushdown
```sql
-- DuckDB pushes the filter to Snowflake, retrieves only filtered data
SELECT * FROM snowflake_scan('SELECT * FROM large_table', 'secret') 
WHERE column_name = 'value';
-- Result: 1 row transferred, 1 row returned
```

## Performance Impact

- **Data Transfer**: 99.9%+ reduction in data transfer
- **Query Speed**: Significantly faster for filtered queries
- **Network Usage**: Minimal network bandwidth usage
- **Memory Usage**: Reduced memory consumption in DuckDB

## Supported Operations

- **Equality Filters**: `WHERE column = 'value'`
- **Range Filters**: `WHERE column > 100`
- **IN Clauses**: `WHERE column IN ('a', 'b', 'c')`
- **BETWEEN**: `WHERE column BETWEEN 1 AND 100`
- **Column Projections**: Only selected columns are retrieved

## Controlling Pushdown

```bash
# Disable pushdown (for debugging or testing)
export SNOWFLAKE_DISABLE_PUSHDOWN=true

# Enable pushdown (default)
export SNOWFLAKE_DISABLE_PUSHDOWN=false
```

## Technical Implementation

The pushdown system:
1. **Analyzes** DuckDB filter expressions
2. **Translates** them to Snowflake SQL syntax
3. **Modifies** the original query with WHERE clauses
4. **Executes** the optimized query on Snowflake
5. **Returns** only the filtered results

## Test Results Summary

### Unit Tests ✅ PASSED
```
Starting Snowflake Predicate Pushdown Unit Tests...
==================================================
Testing BuildWhereClause...
Single filter WHERE clause: WHERE "id" = 42
Multiple filters WHERE clause: WHERE "id" = 42 AND "age" > 0
Empty filters WHERE clause: 
BuildWhereClause tests passed!

Testing BuildSelectClause...
Select clause result: "id", "name", "age"
Single column select: "id"
BuildSelectClause tests passed!

Testing ModifyQuery...
WHERE only modification: SELECT * FROM my_table WHERE "id" > 100
SELECT only modification: SELECT "id", "name" FROM my_table
Both WHERE and SELECT modification: SELECT "id", "name" FROM my_table WHERE "id" > 100
ModifyQuery tests passed!

==================================================
All unit tests passed successfully!
==================================================
```

### Extension Loading Tests ✅ PASSED
```sql
-- Extension version check
SELECT snowflake_version();
-- Result: Snowflake Extension v0.1.0

-- Function availability check
SELECT COUNT(*) FROM duckdb_functions() WHERE function_name = 'snowflake_scan';
-- Result: 1 (function is available)

-- Extension status check
SELECT loaded, installed FROM duckdb_extensions() WHERE extension_name = 'snowflake';
-- Result: true, true (extension loaded and installed)
```

### Test Suite Coverage

The extension includes comprehensive test suites:

1. **snowflake_basic_connectivity.test** - Basic connectivity and setup tests
2. **snowflake_data_types.test** - Data type handling and conversion tests
3. **snowflake_error_handling.test** - Error handling and edge cases
4. **snowflake_performance.test** - Performance and optimization tests
5. **snowflake_pushdown_proof.test** - Predicate pushdown verification tests
6. **snowflake_read_operations.test** - Read operation functionality tests

### Build Status ✅ SUCCESSFUL

- **Extension Build**: Successfully compiled with DuckDB v1.4.0-dev4076
- **ADBC Driver**: Successfully integrated (libadbc_driver_snowflake.so)
- **Dependencies**: All required dependencies resolved
- **Warnings**: Minor unused variable warnings (non-critical)

## Current Limitations

- **Read-only access**: All Snowflake operations are read-only
- **Pushdown for Storage Attach**: Pushdown is not implemented for storage ATTACH, SELECT queries get all the data to duckdb before applying filters like WHERE clause
- **Large result sets**: Should be filtered at source for optimal performance, consider using snowflake_scan
- **COUNT(\*)** like column alias operations are not supported until projection pushdown is implemented

## Best Practices

- Use specific column names in SELECT clauses
- Apply filters early in your queries
- Avoid complex expressions that can't be pushed down
- Test with `SNOWFLAKE_DISABLE_PUSHDOWN=true` to verify performance gains
- Use `snowflake_scan` for optimal pushdown performance
- For large datasets, always include WHERE clauses to leverage pushdown

## Debugging and Verification

### Query History Verification
The pushdown system provides debug output showing the exact queries sent to Snowflake:
```
Query set on statement: 'SELECT "C_CUSTKEY" FROM ... WHERE "C_CUSTKEY" = 1'
```

### Performance Comparison
Test pushdown effectiveness by comparing with and without pushdown:
```bash
# With pushdown (default)
SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM large_table', 'secret') WHERE id = 1;

# Without pushdown (for comparison)
export SNOWFLAKE_DISABLE_PUSHDOWN=true
SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM large_table', 'secret') WHERE id = 1;
```

## Future Enhancements

- Projection pushdown for column selection optimization
- Storage ATTACH pushdown support
- Complex expression pushdown
- Aggregation pushdown
- Join pushdown optimization
