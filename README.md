# DuckDB Snowflake Extension with Predicate Pushdown

This repository contains a DuckDB extension for connecting to Snowflake with advanced predicate pushdown capabilities.

## Features

- **Predicate Pushdown**: Automatically pushes WHERE clauses and SELECT projections to Snowflake
- **Comprehensive Testing**: 20+ SQL logic tests that verify pushdown functionality
- **Production Ready**: Robust error handling and comprehensive test coverage
- **GitHub Actions CI/CD**: Automated testing on every push and pull request

## Quick Start

### Prerequisites

- DuckDB (built from source)
- Snowflake account with sample data access
- CMake and Ninja build tools

### Building the Extension

```bash
# Build DuckDB
cd duckdb
make debug

# Build the Snowflake extension
cd build/debug
ninja
```

### Running Tests

```bash
# Set environment variables
export SNOWFLAKE_ACCOUNT='your_account'
export SNOWFLAKE_USERNAME='your_username'
export SNOWFLAKE_PASSWORD='your_password'
export SNOWFLAKE_DATABASE='SNOWFLAKE_SAMPLE_DATA'

# Run comprehensive pushdown tests
./test/unittest "/path/to/test/sql/snowflake_pushdown_proof.test"

# Run all Snowflake tests
./test/unittest "[sql]" --list-tests | grep snowflake
```

## Test Suite

The repository includes comprehensive SQL logic tests:

- **`snowflake_pushdown_proof.test`**: 20 tests verifying pushdown functionality
- **`snowflake_basic_connectivity.test`**: Basic connectivity and setup tests
- **`snowflake_error_handling.test`**: Error handling and edge cases

### Test Coverage

- ✅ Basic equality filters (`WHERE C_CUSTKEY = 1`)
- ✅ IN clause filters (`WHERE C_CUSTKEY IN (1,2,3)`)
- ✅ Range filters (`WHERE C_CUSTKEY > 1000 AND C_CUSTKEY < 2000`)
- ✅ BETWEEN filters (`WHERE C_CUSTKEY BETWEEN 1000 AND 2000`)
- ✅ String filters (`WHERE C_NAME = 'Customer#000000001'`)
- ✅ LIKE filters (`WHERE C_NAME LIKE 'Customer#00000000%'`)
- ✅ IS NULL/IS NOT NULL filters
- ✅ Projection pushdown (SELECT specific columns)
- ✅ Complex multi-column projections
- ✅ Query history verification (when permissions allow)

## GitHub Actions Integration

The repository is configured with GitHub Actions to automatically run tests on:

- Every push to `main` or `develop` branches
- Every pull request
- Manual workflow dispatch

### Required Secrets

Add these secrets to your GitHub repository settings:

- `SNOWFLAKE_ACCOUNT`: Your Snowflake account identifier
- `SNOWFLAKE_USERNAME`: Your Snowflake username
- `SNOWFLAKE_PASSWORD`: Your Snowflake password
- `SNOWFLAKE_DATABASE`: Database name (e.g., `SNOWFLAKE_SAMPLE_DATA`)

### Viewing Test Results

1. Go to your repository on GitHub
2. Click on the "Actions" tab
3. Select the latest workflow run
4. View test results and logs

## Pushdown Verification

The tests verify pushdown is working by:

1. **Debug Output Analysis**: Checking that queries are modified with WHERE clauses and SELECT projections
2. **Query History Verification**: Attempting to query Snowflake's query history (requires permissions)
3. **Result Verification**: Ensuring correct results are returned with pushdown enabled

Example debug output showing pushdown:
```
[DEBUG] Query pushdown applied - original: 'SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF1.CUSTOMER', 
modified: 'SELECT "C_CUSTKEY" FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF1.CUSTOMER WHERE "C_CUSTKEY" = 1'
```

## Function Reference

- `snowflake_scan(query, secret_name)`: Main table function for querying Snowflake
- `snowflake_version()`: Returns extension version information

## Environment Variables

- `SNOWFLAKE_DISABLE_PUSHDOWN`: Set to 'true' to disable pushdown (for testing)

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.