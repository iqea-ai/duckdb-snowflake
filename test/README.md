# Testing the Snowflake Extension

This directory contains all tests for the DuckDB Snowflake extension. Tests are written as [SQLLogicTests](https://duckdb.org/dev/sqllogictest/intro.html), which use SQL statements to verify functionality.

## Prerequisites

Before running tests, you need:

1. **A Snowflake account** with access to TPC-H sample data
2. **Environment variables** set with your Snowflake credentials:

```bash
export SNOWFLAKE_ACCOUNT="YOUR_ACCOUNT_ID"          # e.g., "FLWILYJ-FL43292"
export SNOWFLAKE_USERNAME="your_username"
export SNOWFLAKE_PASSWORD="your_password"
export SNOWFLAKE_DATABASE="SNOWFLAKE_SAMPLE_DATA"   # Or your database
export SNOWFLAKE_ADBC_DRIVER_PATH="/path/to/libadbc_driver_snowflake.so"
```

3. **ADBC Snowflake driver** installed (see main README for installation)

## Running Tests

### Run All Tests
```bash
# From the repository root
make test           # Release mode
make test_debug     # Debug mode with verbose output
```

### Run Specific Test Files
```bash
# From the repository root
./build/release/test/unittest "test/sql/snowflake_basic_connectivity.test"
./build/release/test/unittest "test/sql/pushdown/hybrid_queries.test"
```

### Run Tests by Pattern
```bash
# Run all pushdown tests
./build/release/test/unittest "test/sql/pushdown/*.test"

# Run specific test name
./build/release/test/unittest "*/snowflake_*"
```

## Test Categories

### Core Functionality (`test/sql/`)
- **`snowflake_basic_connectivity.test`** - Connection, secrets, basic queries
- **`snowflake_all_auth_methods.test`** - Password, key pair, external browser auth
- **`snowflake_read_operations.test`** - SELECT, WHERE, JOIN, subqueries
- **`snowflake_performance.test`** - Large datasets, aggregations, window functions
- **`snowflake_error_handling.test`** - Error messages, invalid operations

### Pushdown Tests (`test/sql/pushdown/`)
- **`pushdown_disabled.test`** - Tests with pushdown OFF (default behavior)
- **`filter_pushdown.test`** - WHERE clause pushdown (=, <, >, IN, BETWEEN)
- **`pushdown_behavior.test`** - Compares results WITH and WITHOUT pushdown
- **`hybrid_queries.test`** - Snowflake + local DuckDB table JOINs
- **`query_builder.test`** - SQL generation for pushdown queries

## Test Coverage

### Authentication Methods
All three authentication methods are tested:
- Password authentication
- Key pair (RSA) authentication
- External browser (SSO/Okta)

### Query Features
- Basic SELECT, WHERE, ORDER BY, LIMIT
- JOINs (INNER, LEFT, multiple tables)
- Aggregations (COUNT, SUM, AVG, GROUP BY)
- Window functions (ROW_NUMBER, RANK, LAG, LEAD)
- Subqueries (correlated, EXISTS, IN)
- UNION, INTERSECT, EXCEPT
- BETWEEN and range filters
- Complex expressions (CASE, string/numeric functions)

### Pushdown Verification
Tests verify:
1. Results match with and without pushdown
2. Filters are pushed to Snowflake (via EXPLAIN)
3. Projections are pushed to Snowflake
4. Performance improvements with pushdown enabled

### Hybrid Queries
Tests combine Snowflake and local DuckDB data:
- `snowflake_query()` results JOINed with local tables
- ATTACH tables JOINed with local tables
- UNION of Snowflake and local data
- Multi-way JOINs (Snowflake + multiple local tables)

## Understanding Test Output

### Test Results
```
Running test/sql/snowflake_basic_connectivity.test
✓ Test 1: Extension Loading
✓ Test 2: Create Secret
✓ Test 3: Query Execution
All tests passed: 3/3
```

### Profiling Output
Some tests generate profiling data in JSON files:
- `hybrid_queries_profile.json` - Timing and metrics for hybrid queries
- Look for `PRAGMA enable_profiling` in test files

View profiling data:
```bash
# Pretty print JSON profiling output
cat hybrid_queries_profile.json | jq '.'

# Extract query timing
cat hybrid_queries_profile.json | jq '.timings'
```

### Debug Output
When tests run, you may see debug messages:
```
[DEBUG] GetCredentials: auth_type_str = 'key_pair'
[DEBUG] auth_type_str is empty, using default PASSWORD auth
```
These are informational and help diagnose credential loading.

## Writing New Tests

Tests use SQLLogicTest format. Example:

```sql
# Test name/description
statement ok
CREATE SECRET test (TYPE snowflake, ...);

# Query with expected result count
query I
SELECT COUNT(*) FROM snowflake_table WHERE id = 1;
----
1

# Verify error is thrown
statement error
SELECT * FROM nonexistent_table;
```

Key directives:
- `require snowflake` - Skip if extension not available
- `require-env VAR` - Skip if environment variable not set
- `statement ok` - Statement should succeed
- `statement error` - Statement should fail
- `query I/IT/III` - Query with expected results (I=integer, T=text)

## Troubleshooting

### Tests Skipped
If tests are skipped, check:
1. Environment variables are set (especially for `require-env` tests)
2. Snowflake credentials are valid
3. ADBC driver is installed and path is correct

### Connection Failures
```
IO Error: Failed to initialize connection
```
- Verify SNOWFLAKE_ACCOUNT format (use short form: `ACCOUNT-ID`, not `ACCOUNT.snowflakecomputing.com`)
- Check username/password are correct
- Ensure warehouse exists and is accessible

### Test Failures
If specific tests fail:
1. Check your Snowflake database has TPC-H sample data (`TPCH_SF1` schema)
2. Verify your user has SELECT permissions
3. Run individual test with `unittest` for detailed output

## CI/CD Testing

Tests run automatically on GitHub Actions:
- Main workflow: `.github/workflows/MainDistributionPipeline.yml`
- Snowflake-specific tests: `.github/workflows/test-with-snowflake.yml`

GitHub secrets required:
- `SNOWFLAKE_ACCOUNT`
- `SNOWFLAKE_USERNAME`
- `SNOWFLAKE_PASSWORD`
- `SNOWFLAKE_DATABASE`