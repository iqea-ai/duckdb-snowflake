# Setting up DuckDB Snowflake Extension with GitHub CI

This guide explains how to set up this DuckDB Snowflake extension repository with automatic test running on GitHub.

## Prerequisites

1. **Private GitHub Repository**: You need access to the private repository
2. **Snowflake Credentials**: You need Snowflake account credentials for testing

## Setup Steps

### 1. Add Snowflake Secrets to GitHub

Go to your GitHub repository → Settings → Secrets and variables → Actions, and add these secrets:

```
SNOWFLAKE_ACCOUNT=wdbniqg-ct63033
SNOWFLAKE_USERNAME=ptandra
SNOWFLAKE_PASSWORD=LM7qsuw6XGbuBQK
SNOWFLAKE_DATABASE=SNOWFLAKE_SAMPLE_DATA
```

### 2. Push to Your Repository

```bash
# Add your private repository as remote
git remote add test1 https://github.com/sathvikkurap/test1.git

# Push to your repository
git push test1 main
```

### 3. Verify Tests Run Automatically

1. Go to your GitHub repository
2. Click on the "Actions" tab
3. You should see the "Snowflake SQL Logic Tests" workflow
4. Click on it to see the test results

## Test Files Included

- `test/sql/snowflake_pushdown_proof.test` - Comprehensive pushdown verification (20 tests)
- `test/sql/snowflake_basic_connectivity.test` - Basic connectivity tests
- `test/sql/snowflake_performance.test` - Performance comparison tests
- `test/sql/snowflake_data_types.test` - Data type handling tests
- `test/sql/snowflake_error_handling.test` - Error handling tests
- `test/sql/snowflake_read_operations.test` - Read operation tests

## What the Tests Verify

### Pushdown Proof Tests
- ✅ Basic equality filter pushdown
- ✅ IN filter pushdown  
- ✅ Range filter pushdown
- ✅ Projection pushdown
- ✅ BETWEEN filter pushdown
- ✅ Complex filter combinations
- ✅ String and LIKE filter pushdown
- ✅ IS NULL/IS NOT NULL filters
- ✅ Query history verification (if permissions allow)

### Debug Output Verification
The tests verify pushdown is working by checking debug output like:
```
[DEBUG] Query pushdown applied - original: 'SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF1.CUSTOMER', 
modified: 'SELECT "C_CUSTKEY" FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF1.CUSTOMER WHERE "C_CUSTKEY" = 1'
```

## Local Testing

To run tests locally:

```bash
# Set environment variables
export SNOWFLAKE_ACCOUNT='wdbniqg-ct63033'
export SNOWFLAKE_USERNAME='ptandra'
export SNOWFLAKE_PASSWORD='LM7qsuw6XGbuBQK'
export SNOWFLAKE_DATABASE='SNOWFLAKE_SAMPLE_DATA'

# Build the extension
cd duckdb && make debug

# Run specific tests
./build/debug/test/unittest "/path/to/test/sql/snowflake_pushdown_proof.test"
./build/debug/test/unittest "/path/to/test/sql/snowflake_basic_connectivity.test"

# Run all Snowflake tests
./build/debug/test/unittest "[sql]" | grep snowflake
```

## GitHub Actions Workflow

The repository includes `.github/workflows/snowflake-sql-tests.yml` which:

1. **Builds DuckDB** with the Snowflake extension
2. **Runs SQL Logic Tests** automatically on every push/PR
3. **Uses GitHub Secrets** for Snowflake credentials
4. **Reports Results** in the GitHub Actions interface

## Troubleshooting

### Tests Fail with Permission Errors
- Ensure Snowflake user has access to `SNOWFLAKE_SAMPLE_DATA` database
- Check that the user can query `INFORMATION_SCHEMA` tables

### Build Failures
- Ensure all dependencies are installed (cmake, ninja-build, etc.)
- Check that the ADBC driver is available

### Secret Issues
- Verify all 4 Snowflake secrets are set in GitHub repository settings
- Ensure secret names match exactly: `SNOWFLAKE_ACCOUNT`, `SNOWFLAKE_USERNAME`, `SNOWFLAKE_PASSWORD`, `SNOWFLAKE_DATABASE`

## Production Readiness

This setup provides:
- ✅ **Automated Testing**: Tests run on every code change
- ✅ **Comprehensive Coverage**: 20+ test scenarios
- ✅ **Real Snowflake Verification**: Tests against actual Snowflake instance
- ✅ **Debug Output Verification**: Confirms pushdown is working
- ✅ **Error Handling**: Graceful handling of permission issues
- ✅ **CI/CD Integration**: Ready for production deployment
