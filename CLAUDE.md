# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a DuckDB extension that enables querying Snowflake databases via Apache Arrow ADBC drivers. The extension provides:
- `snowflake_scan()` table function for direct Snowflake queries
- Storage extension for `ATTACH` with TYPE snowflake
- Secret management for Snowflake credentials

## Build Commands

```bash
# Build release (downloads ADBC driver automatically)
make release

# Build debug (includes DEBUG_SNOWFLAKE logging)
make debug

# Run all tests
make test

# Run specific test file
./build/debug/test/unittest "test/sql/snowflake_*.test"

# Clean build
make clean
```

## Testing with Real Snowflake

Tests require environment variables:
```bash
export SNOWFLAKE_ACCOUNT="your_account"
export SNOWFLAKE_USERNAME="your_username"
export SNOWFLAKE_PASSWORD="your_password"
export SNOWFLAKE_DATABASE="your_database"
make test-snowflake
```

## Architecture

### Core Components

- **snowflake_extension.cpp** - Extension entry point, registers functions and storage extension
- **snowflake_scan.cpp** - Table function implementation for `snowflake_scan(query, secret_name)`
- **snowflake_client.cpp** - ADBC driver wrapper, manages connections and query execution
- **snowflake_client_manager.cpp** - Connection pooling and lifecycle management
- **snowflake_secrets.cpp** - DuckDB secret type registration for Snowflake credentials
- **snowflake_arrow_utils.cpp** - Arrow to DuckDB type conversion utilities
- **snowflake_types.cpp** - Snowflake to DuckDB type mappings

### Storage Extension (src/storage/)

Enables `ATTACH '' AS db (TYPE snowflake, SECRET secret_name)`:
- **snowflake_storage.cpp** - Storage extension registration
- **snowflake_catalog.cpp** - Catalog implementation
- **snowflake_schema_entry.cpp** / **snowflake_schema_set.cpp** - Schema enumeration
- **snowflake_table_entry.cpp** / **snowflake_table_set.cpp** - Table enumeration

### Data Flow

1. User creates secret with Snowflake credentials
2. `snowflake_scan()` or ATTACH triggers ADBC driver loading
3. Driver is loaded dynamically from `~/.duckdb/extensions/{version}/{platform}/libadbc_driver_snowflake.so`
4. Queries execute via ADBC, returning Arrow arrays
5. Arrow data converted to DuckDB vectors

## Key Files

- `CMakeLists.txt` - Build configuration, links OpenSSL, handles ADBC driver copying
- `extension_config.cmake` - DuckDB extension loader config
- `Makefile` - Build targets wrapping extension-ci-tools
- `vcpkg.json` - Dependency manifest (OpenSSL)

## Dependencies

- DuckDB (git submodule at `./duckdb/`)
- OpenSSL (via vcpkg)
- Apache Arrow ADBC Snowflake driver (runtime, downloaded to `adbc_drivers/`)
- extension-ci-tools (git submodule)

## Debug Builds

Debug builds define `DEBUG_SNOWFLAKE` which enables verbose logging in `snowflake_debug.hpp` for:
- ADBC driver loading paths
- Connection establishment
- Type conversions
- Query execution flow

## Test Files

Tests use DuckDB's SQLLogicTest format in `test/sql/`:
- `snowflake_basic_connectivity.test`
- `snowflake_data_types.test`
- `snowflake_read_operations.test`
- `snowflake_error_handling.test`
- `snowflake_performance.test`
