# DuckDB Snowflake Extension - Developer Guide

This guide is for developers who want to build the DuckDB Snowflake extension from source, contribute to its development, or understand its internal architecture.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Build Instructions](#build-instructions)
- [Development Setup](#development-setup)
- [Testing](#testing)
- [Contributing](#contributing)

## Prerequisites

### System Requirements

- **CMake** >= 3.18
- **C++17** compatible compiler (GCC 7+, Clang 6+, MSVC 2019+)
- **vcpkg** (for dependency management)
- **Git** (with submodules)
- **DuckDB** with extension support (version 0.8.0 or later)
- **Arrow ADBC Snowflake driver** (shared library)
- **OpenSSL** for secure connections

### Platform Support

- **Linux**: x86_64, ARM64
- **macOS**: x86_64, ARM64 (Apple Silicon)
- **Windows**: x86_64 (with MinGW or MSVC)

### Required Dependencies

The extension depends on:

- DuckDB (as submodule)
- Apache Arrow ADBC (runtime dependency)
- OpenSSL (via vcpkg)

## Build Instructions

### Quick Build

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/your-repo/duckdb-snowflake.git
cd duckdb-snowflake

# Build release version (default)
make

```

### Build Targets

The Makefile provides several targets:

```bash
# Build release without ADBC (uses pre-built driver)
make release

# Build debug without ADBC (uses pre-built driver)
make debug

# Run tests
make test

# Clean build artifacts
make clean

```

### CMake Build (Alternative)

You can also build directly with CMake:

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja

# Build with ninja (parallel build)
ninja -j8

# Or with make
make -j8
```

### Debug Builds

Debug builds include additional logging for troubleshooting:

```bash
make debug

# Run with debug output
./build/debug/duckdb
```

Debug builds will print detailed information about:

- ADBC driver loading paths
- Connection establishment
- Type conversions
- Query execution flow

## Testing

### Running Tests

```bash
# Run all tests
make test

# Run specific test
./build/release/duckdb -c "LOAD snowflake; SELECT snowflake_version();"

# Run with debug output
SNOWFLAKE_DEBUG=1 ./build/debug/duckdb -c "LOAD snowflake;"
```

### Test Structure

```
test/
├── sql/                       # SQL test files
│   └── snowflake.test
└── README.md                  # Testing documentation
```

### Writing Tests

Create test files in `test/sql/`:

```sql
-- test/sql/my_test.test
statement ok
LOAD snowflake;

statement ok
CREATE SECRET test_snowflake (
    TYPE snowflake,
    ACCOUNT 'test_account',
    USER 'test_user',
    PASSWORD 'test_pass',
    DATABASE 'test_db',
    WAREHOUSE 'test_wh'
);

query I
SELECT snowflake_version();
----
Snowflake Extension v0.1.0
```

### Integration Testing

For integration tests with real Snowflake:

1. Set up test credentials
2. Create test database/schema
3. Run integration test suite

```bash
# Set test credentials
export SNOWFLAKE_TEST_ACCOUNT="your_test_account"
export SNOWFLAKE_TEST_USER="your_test_user"
export SNOWFLAKE_TEST_PASSWORD="your_test_password"
export SNOWFLAKE_TEST_DATABASE="your_test_database"
export SNOWFLAKE_TEST_WAREHOUSE="your_test_warehouse"

# Run integration tests
make test
```

## Contributing
This project follows similar guidelines a duckdb for contributions, please checkout https://github.com/duckdb/duckdb/blob/main/CONTRIBUTING.md

## Support

For development questions:

- Check existing issues on GitHub
- Review DuckDB extension development docs
- Ask questions in discussions

## License

This project is licensed under the same terms as DuckDB. See [LICENSE](LICENSE) for details.