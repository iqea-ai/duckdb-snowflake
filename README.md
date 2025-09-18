# DuckDB Snowflake Extension

A powerful DuckDB extension that enables seamless querying of Snowflake databases using Arrow ADBC drivers. This extension provides efficient, columnar data transfer between DuckDB and Snowflake, making it ideal for analytics, ETL pipelines, and cross-database operations.

## Quick Start

### Installation

```sql
-- Install and load the extension
INSTALL snowflake FROM community;
LOAD snowflake;
```

### Basic Usage

```sql
-- 1. Create a Snowflake profile
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'your_account.snowflakecomputing.com',
    USER 'your_username',
    PASSWORD 'your_password',
    DATABASE 'your_database',
    WAREHOUSE 'your_warehouse'
);

-- 2.1 Query Snowflake data using pass through query
SELECT * FROM snowflake_scan(
    'SELECT * FROM customers WHERE state = ''CA''',
    'my_snowflake_secret'
);

-- 2.2 Query Snowflake data using local duckdb SQL syntax
ATTACH '' AS snow_db (TYPE snowflake, SECRET my_snowflake_secret, READ_ONLY);

SELECT * FROM snow_db.schema.customers WHERE state = 'CA';
```

## Documentation

### For End Users

If you want to **install and use** the extension, continue reading this README for:

- Installation instructions
- Configuration setup
- Function reference
- Usage examples
- Troubleshooting guide

## Overview

The DuckDB Snowflake Extension bridges the gap between DuckDB's analytical capabilities and Snowflake's cloud data warehouse, allowing you to query Snowflake data directly from DuckDB without complex data movement processes.

### Key Features

- **Direct Querying**: Execute SQL queries against Snowflake databases from within DuckDB
- **Arrow-Native Pipeline**: Leverages Apache Arrow for efficient, columnar data transfer
- **Secure Authentication**: Password-based authentication with secure credential storage
- **Secret Management**: Secure credential storage using DuckDB's secrets system
- **Storage Extension**: Attach Snowflake databases as read-only storage

## Installation

### Prerequisites

The DuckDB Snowflake extension requires the `libadbc_driver_snowflake.so` library to function properly. This library is **not included** in the extension package and must be obtained separately.

### Method 1: From DuckDB Community Extensions (Recommended)

```sql
-- Install and load the extension
INSTALL snowflake FROM community;
LOAD snowflake;
```

**Note:** You still need to download the ADBC driver separately (see [ADBC Driver Setup](#adbc-driver-setup) below).

### Method 2: Manual Installation

```bash
# Download the pre-built extension
wget https://github.com/your-org/duckdb-snowflake/releases/latest/download/snowflake.duckdb_extension

# Load in DuckDB
LOAD 'path/to/snowflake.duckdb_extension';
```

**Note:** You still need to download the ADBC driver separately (see [ADBC Driver Setup](#adbc-driver-setup) below).

## ADBC Driver Setup

The Snowflake extension requires the Apache Arrow ADBC Snowflake driver to communicate with Snowflake servers. Download and install the appropriate driver for your platform:

### Supported Platforms

- **Linux x86_64** (`linux_amd64`)
- **Linux ARM64** (`linux_arm64`) 
- **Linux x86_64 (musl)** (`linux_amd64_musl`)
- **macOS x86_64** (`osx_amd64`)
- **macOS ARM64** (`osx_arm64`)
- **Windows x86_64** (`windows_amd64`)
- **Windows x86_64 (MinGW)** (`windows_amd64_mingw`)

### Download and Installation Instructions

The ADBC Snowflake driver is distributed as Python wheel files. Download the appropriate wheel, extract the shared library, and place it in the correct DuckDB extension directory:

**For macOS (ARM64):**
```bash
# Download the Python wheel
curl -L -o adbc_driver_snowflake.whl https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-macosx_11_0_arm64.whl

# Extract the shared library
unzip adbc_driver_snowflake.whl "adbc_driver_snowflake/libadbc_driver_snowflake.so"
mv adbc_driver_snowflake/libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rm -rf adbc_driver_snowflake adbc_driver_snowflake.whl

# Place in DuckDB extensions directory
mkdir -p /Users/<username>/.duckdb/extensions/v1.4.0/osx_arm64
mv libadbc_driver_snowflake.so /Users/<username>/.duckdb/extensions/v1.4.0/osx_arm64/
```

**For macOS (x86_64):**
```bash
curl -L -o adbc_driver_snowflake.whl https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-macosx_10_15_x86_64.whl
unzip adbc_driver_snowflake.whl "adbc_driver_snowflake/libadbc_driver_snowflake.so"
mv adbc_driver_snowflake/libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rm -rf adbc_driver_snowflake adbc_driver_snowflake.whl

# Place in DuckDB extensions directory
mkdir -p /Users/<username>/.duckdb/extensions/v1.4.0/osx_amd64
mv libadbc_driver_snowflake.so /Users/<username>/.duckdb/extensions/v1.4.0/osx_amd64/
```

**For Linux (x86_64):**
```bash
curl -L -o adbc_driver_snowflake.whl https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-manylinux1_x86_64.manylinux2014_x86_64.manylinux_2_17_x86_64.manylinux_2_5_x86_64.whl
unzip adbc_driver_snowflake.whl "adbc_driver_snowflake/libadbc_driver_snowflake.so"
mv adbc_driver_snowflake/libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rm -rf adbc_driver_snowflake adbc_driver_snowflake.whl

# Place in DuckDB extensions directory
mkdir -p /home/<username>/.duckdb/extensions/v1.4.0/linux_amd64
mv libadbc_driver_snowflake.so /home/<username>/.duckdb/extensions/v1.4.0/linux_amd64/
```

**For Linux (ARM64):**
```bash
curl -L -o adbc_driver_snowflake.whl https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-manylinux2014_aarch64.manylinux_2_17_aarch64.whl
unzip adbc_driver_snowflake.whl "adbc_driver_snowflake/libadbc_driver_snowflake.so"
mv adbc_driver_snowflake/libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rm -rf adbc_driver_snowflake adbc_driver_snowflake.whl

# Place in DuckDB extensions directory
mkdir -p /home/<username>/.duckdb/extensions/v1.4.0/linux_amd64
mv libadbc_driver_snowflake.so /home/<username>/.duckdb/extensions/v1.4.0/linux_amd64/
```

**For Linux (x86_64 musl):**
```bash
# Note: Use the standard Linux x86_64 wheel for musl systems
curl -L -o adbc_driver_snowflake.whl https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-manylinux1_x86_64.manylinux2014_x86_64.manylinux_2_17_x86_64.manylinux_2_5_x86_64.whl
unzip adbc_driver_snowflake.whl "adbc_driver_snowflake/libadbc_driver_snowflake.so"
mv adbc_driver_snowflake/libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rm -rf adbc_driver_snowflake adbc_driver_snowflake.whl

# Place in DuckDB extensions directory
mkdir -p /home/<username>/.duckdb/extensions/v1.4.0/linux_amd64
mv libadbc_driver_snowflake.so /home/<username>/.duckdb/extensions/v1.4.0/linux_amd64/
```

**For Windows (x86_64):**
```cmd
# Download the wheel file (rename to zip for extraction)
wget https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-win_amd64.whl -O adbc_driver_snowflake.zip

# Extract the shared library
powershell Expand-Archive -Path adbc_driver_snowflake.zip -DestinationPath temp_extract
move temp_extract\adbc_driver_snowflake\libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rmdir /s temp_extract
del adbc_driver_snowflake.zip

# Place in DuckDB extensions directory
mkdir C:\Users\%USERNAME%\.duckdb\extensions\v1.4.0\windows_amd64
move libadbc_driver_snowflake.so C:\Users\%USERNAME%\.duckdb\extensions\v1.4.0\windows_amd64\
```

**For Windows (x86_64 MinGW):**
```cmd
# Note: Use the standard Windows x86_64 wheel for MinGW systems
wget https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-win_amd64.whl -O adbc_driver_snowflake.zip
powershell Expand-Archive -Path adbc_driver_snowflake.zip -DestinationPath temp_extract
move temp_extract\adbc_driver_snowflake\libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rmdir /s temp_extract
del adbc_driver_snowflake.zip

# Place in DuckDB extensions directory
mkdir C:\Users\%USERNAME%\.duckdb\extensions\v1.4.0\windows_amd64
move libadbc_driver_snowflake.so C:\Users\%USERNAME%\.duckdb\extensions\v1.4.0\windows_amd64\
```

### Alternative Driver Placement

The extension also searches for the driver in these locations (in order):
1. Extension directory: `./libadbc_driver_snowflake.so`
2. Extension subdirectory: `./adbc_drivers/libadbc_driver_snowflake.so`
3. Build directory: `./adbc_drivers/libadbc_driver_snowflake.so`
4. System paths: `/usr/local/lib/` or `/usr/lib/`

**Alternative approach (next to DuckDB binary):**
```bash
# Create the adbc_drivers directory next to your DuckDB binary
mkdir -p adbc_drivers
mv libadbc_driver_snowflake.so adbc_drivers/
```

### Verification

Test that the driver is found:
```sql
LOAD snowflake;
SELECT snowflake_version();
-- Should return: "Snowflake Extension v0.1.0"
```

### Troubleshooting

**If you get "ADBC driver not supported" error:**
- Verify the driver file is in the correct location
- Check file permissions (should be executable)
- Ensure you downloaded the correct architecture for your platform

**If you get "Driver not found" debug messages:**
- The extension will search multiple locations automatically
- Check the debug output to see which paths it's checking
- Place the driver in one of the searched locations

## Configuration

### Setting Up Snowflake Credentials

#### Using Secrets (Recommended)

Create a named profile to securely store your Snowflake credentials:

**Creating a Secret:**

```sql
-- Secret with optional parameters
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'adbniqz-ct69933',
    USER 'myusername',
    PASSWORD 'mypassword',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse',
    SCHEMA 'myschema'  -- Optional: default schema
);
```

**Listing Secrets:**

```sql
-- View all secrets
SELECT * FROM duckdb_secrets();

-- View only Snowflake secrets
SELECT * FROM duckdb_secrets() WHERE type = 'snowflake';
```

**Updating a Secret:**

```sql
-- Drop and recreate to update
DROP SECRET my_snowflake_secret;

CREATE SECRET ...
```

**Deleting a Secret:**

```sql
-- Remove a secret
DROP SECRET my_snowflake_secret;
```

## Functions Reference

### Scalar Functions

#### `snowflake_version()`

Returns the extension version information.

```sql
SELECT snowflake_version();
-- Returns: "Snowflake Extension v0.1.0"
```

### Table Functions

#### `snowflake_scan(query, profile)`

Executes SQL queries against Snowflake databases.

```sql
SELECT * FROM snowflake_scan(
    'SELECT * FROM customers WHERE state = ''CA''',
    'my_snowflake_secret'
);
```

### Storage Extension

#### `ATTACH` with Snowflake Storage Extension

Attaches a Snowflake database as a read-only storage extension.

```sql
-- Using secret
ATTACH '' AS snow_db (TYPE snowflake, SECRET my_snowflake_secret, READ_ONLY);

-- Using connection string
ATTACH 'account=myaccount;user=myuser;password=mypass;database=mydb;warehouse=mywh'
AS snow_db (TYPE snowflake, READ_ONLY);
```

## Usage Examples

### Basic Queries

```sql
-- Test connection
SELECT * FROM snowflake_scan('SELECT 1', 'my_snowflake_secret');

-- Query with attached database
ATTACH '' AS snow (TYPE snowflake, SECRET my_snowflake_secret, READ_ONLY);
SELECT * FROM snow.public.customers LIMIT 10;
```

### Complex Analytics

```sql
-- Use Snowflake's computational power for complex analytics
SELECT * FROM snowflake_scan(
    '
    WITH monthly_sales AS (
        SELECT
            DATE_TRUNC(''month'', order_date) as month,
            SUM(amount) as total_sales
        FROM orders
        GROUP BY 1
    )
    SELECT
        month,
        total_sales,
        LAG(total_sales) OVER (ORDER BY month) as prev_month_sales
    FROM monthly_sales
    ',
    'my_snowflake_secret'
);
```

### Cross-Database Analytics

```sql
-- Combine Snowflake data with local DuckDB tables
SELECT
    sf.product_id,
    sf.sales_amount,
    local.discount_rate,
    sf.sales_amount * (1 - local.discount_rate) as discounted_amount
FROM snowflake_scan(
    'SELECT product_id, SUM(amount) as sales_amount FROM sales GROUP BY product_id',
    'my_snowflake_secret'
) sf
JOIN local_discounts local ON sf.product_id = local.product_id;
```

### Data Export Workflows

```sql
-- Export large dataset efficiently
CREATE TABLE local_fact_sales AS
SELECT * FROM snowflake_scan(
    'SELECT * FROM fact_sales WHERE year >= 2020',
    'my_snowflake_secret'
);

-- Create Parquet files from Snowflake data
COPY (
    SELECT * FROM snowflake_scan(
        'SELECT * FROM large_table',
        'my_snowflake_secret'
    )
) TO 'output.parquet' (FORMAT PARQUET);
```


## Troubleshooting

### Common Errors

**Connection Errors:**

```
Failed to initialize connection: [Snowflake] 390100 (08004): Incorrect username or password was specified.
```

**Solution**: Verify your credentials and account information.

**Query Errors:**

```
Failed to get schema: [Snowflake] 090105 (22000): Cannot perform SELECT. This session does not have a current database.
```

**Solution**: Specify a database in the connection string or use fully qualified table names.

### Debugging Tips

```sql
-- Test connection with a simple query
SELECT * FROM snowflake_scan('SELECT 1', 'my_snowflake_secret');

-- Check profile configuration
SELECT * FROM duckdb_secrets() WHERE type = 'snowflake';

-- Verify warehouse status
SELECT * FROM snowflake_scan(
    'SHOW WAREHOUSES',
    'my_snowflake_secret'
);
```

## Limitations

- **Read-only access**: All Snowflake operations are read-only
- **Pushdown for Storage Attach**: Pushdown is not implemented for storage ATTACH, SELECT queries get all the data to duckdb before applying filters like WHERE clause
- **Large result sets**: Should be filtered at source for optimal performance, consider using snowflake_scan
- ****COUNT(\*)** like column alias operations are not supported until projection pushdown is implemented

## Security Best Practices

1. **Use Strong Passwords**: Create complex passwords for your Snowflake accounts
2. **Principle of Least Privilege**: Use Snowflake roles with minimal required permissions
3. **Regular Rotation**: Update passwords and credentials regularly
4. **Environment Separation**: Use different secrets for dev/test/prod environments
5. **Secure Storage**: Secrets are stored encrypted in DuckDB's internal storage

## Support

For issues or questions:

- Check the [GitHub repository](https://github.com/your-repo/duckdb-snowflake)
- Review Snowflake ADBC driver documentation
- Ensure you have the latest version of the extension

### For Developers

If you want to build the extension from source or contribute to development, see [BUILD.md](BUILD.md) for detailed build instructions and development guidelines.# Trigger new workflow run
# Test build with DuckDB 1.4.0
