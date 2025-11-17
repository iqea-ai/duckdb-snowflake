# DuckDB Snowflake Extension

A powerful DuckDB extension that enables seamless querying of Snowflake databases using Arrow ADBC drivers. This extension provides efficient, columnar data transfer between DuckDB and Snowflake, making it ideal for analytics, ETL pipelines, and cross-database operations.

## Quick Start

> **Important:** Advanced authentication (OAuth, key pair, Okta, EXT_BROWSER) and filter/predicate pushdown require the Snowflake extension build for DuckDB **v1.4.1** or newer. Install the v1.4.1 artifacts from the `OAuth_KeyPair_SAML` GitHub Actions run before continuing.

### Get the Latest Extension Build (v1.4.1)

Install DuckDB 1.4.1 (or newer) and then install the Snowflake extension directly from the community repository:

```sql
INSTALL snowflake FROM community;
LOAD snowflake;
``` 

Confirm your DuckDB version meets the requirement:

```sql
PRAGMA version;
```

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
    ACCOUNT 'your_account_identifier',
    USER 'your_username',
    PASSWORD 'your_password',
    DATABASE 'your_database',
    WAREHOUSE 'your_warehouse'
);

-- 2.1 Query Snowflake data using pass through query
SELECT * FROM snowflake_query(
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

- **Direct Querying**: Execute SQL queries against Snowflake from within DuckDB
- **Arrow-Native Pipeline**: Leverages Apache Arrow for efficient, columnar data transfer
- **Multiple Authentication Methods**: Support for password, OAuth, key-pair (with passphrase support), external browser SSO, Okta, and MFA authentication
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

The Snowflake extension requires the Apache Arrow ADBC Snowflake driver to communicate with Snowflake servers.

### Quick Install (Recommended)

Use the automated installer script to download and install the correct ADBC driver for your platform:

```bash
# Using curl
curl -sSL https://raw.githubusercontent.com/iqea-ai/duckdb-snowflake/main/scripts/install-adbc-driver.sh | sh

# Or using wget
wget -qO- https://raw.githubusercontent.com/iqea-ai/duckdb-snowflake/main/scripts/install-adbc-driver.sh | sh
```

The installer will:
- Detect your DuckDB version and platform automatically
- Download the correct ADBC driver version
- Install it to `~/.duckdb/extensions/<version>/<platform>/libadbc_driver_snowflake.so`
- Verify the installation

> **Tip:** When the script prompts for a version, specify `v1.4.1` to ensure advanced authentication and pushdown features are available.

### Manual Installation (advanced)

If you prefer to install manually, download and install the appropriate driver for your platform:

#### Supported Platforms

| Platform | DuckDB Directory | Wheel File Suffix |
|----------|------------------|-----------------|
| **Linux x86_64** | `linux_amd64` | `manylinux1_x86_64.manylinux2014_x86_64...` |
| **Linux ARM64** | `linux_arm64` | `manylinux2014_aarch64.manylinux_2_17_aarch64` |
| **macOS x86_64** | `osx_amd64` | `macosx_10_15_x86_64` |
| **macOS ARM64** | `osx_arm64` | `macosx_11_0_arm64` |
| **Windows x86_64** | `windows_amd64` | `win_amd64` |

#### Generic Installation Steps - non windows

Replace `<PLATFORM>` with your platform directory from the table above:

```bash
# 1. Download the appropriate wheel for your platform from:
#    https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/
#    See the table above for the correct wheel file suffix

# 2. Extract the driver library
unzip adbc_driver_snowflake-*.whl "adbc_driver_snowflake/*"

# 3. Move to DuckDB extensions directory (DuckDB v1.4.1)
mkdir -p ~/.duckdb/extensions/v1.4.1/<PLATFORM>
mv adbc_driver_snowflake/libadbc_driver_snowflake.so ~/.duckdb/extensions/v1.4.1/<PLATFORM>/

# 4. Clean up
rm -rf adbc_driver_snowflake adbc_driver_snowflake-*.whl
```

#### Example: Linux x86_64 Installation

```bash
# Download
curl -L -o adbc_driver_snowflake.whl \
  https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-manylinux1_x86_64.manylinux2014_x86_64.manylinux_2_17_x86_64.manylinux_2_5_x86_64.whl

# Extract
unzip adbc_driver_snowflake.whl "adbc_driver_snowflake/*"

# Install (DuckDB v1.4.1)
mkdir -p ~/.duckdb/extensions/v1.4.1/linux_amd64
mv adbc_driver_snowflake/libadbc_driver_snowflake.so ~/.duckdb/extensions/v1.4.1/linux_amd64/

# Clean up
rm -rf adbc_driver_snowflake adbc_driver_snowflake.whl
```

#### Available Wheel Files

All wheels are available from [Apache Arrow ADBC Release 1.8.0](https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/):

- **Linux x86_64**: `adbc_driver_snowflake-1.8.0-py3-none-manylinux1_x86_64.manylinux2014_x86_64.manylinux_2_17_x86_64.manylinux_2_5_x86_64.whl`
- **Linux ARM64**: `adbc_driver_snowflake-1.8.0-py3-none-manylinux2014_aarch64.manylinux_2_17_aarch64.whl`
- **macOS x86_64**: `adbc_driver_snowflake-1.8.0-py3-none-macosx_10_15_x86_64.whl`
- **macOS ARM64**: `adbc_driver_snowflake-1.8.0-py3-none-macosx_11_0_arm64.whl`

#### Installation Steps - Windows
``` shell
# Download
wget https://github.com/apache/arrow-adbc/releases/download/apache-arrow-adbc-20/adbc_driver_snowflake-1.8.0-py3-none-win_amd64.whl -O adbc_driver_snowflake.zip
powershell Expand-Archive -Path adbc_driver_snowflake.zip -DestinationPath temp_extract
move temp_extract\adbc_driver_snowflake\libadbc_driver_snowflake.so libadbc_driver_snowflake.so
rmdir /s temp_extract
del adbc_driver_snowflake.zip

# Place in DuckDB extensions directory (DuckDB v1.4.1)
mkdir C:\Users\%USERNAME%\.duckdb\extensions\v1.4.1\windows_amd64
move libadbc_driver_snowflake.so C:\Users\%USERNAME%\.duckdb\extensions\v1.4.1\windows_amd64\
```

### Verification

Test that the driver is found:
```sql
LOAD snowflake;
SELECT snowflake_version();
-- Should return: "Snowflake Extension v0.1.0"
```

## Configuration

### Authentication Methods

The DuckDB Snowflake extension supports multiple authentication methods:

- **Password**: Standard username/password (simple setup, development) - Tested
- **OAuth 2.0**: Token-based authentication (recommended for Okta/Auth0, headless environments) - Known Issues
- **Key Pair**: RSA key-based authentication (production, highest security, recommended) - Tested
- **External Browser (SAML 2.0)**: SSO with any SAML provider (Okta, Auth0, AD FS, Azure AD) - Tested
- **Okta**: Native Okta integration (Okta IdP only) - Implemented
- **MFA**: Multi-factor authentication (interactive sessions only, not for programmatic use)

Note on account values:
- Use your Snowflake account identifier (e.g., `myaccount` or `xy12345.us-east-1`) for `ACCOUNT` in all secrets and connection strings (Password, Key Pair, OAuth, MFA, EXT_BROWSER, OKTA).
- Use the full Snowflake URL (`https://<account>.snowflakecomputing.com`) only in IdP configuration such as OAuth/SAML audience values.

**Quick Example (Password Auth):**
```sql
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount',
    USER 'myusername',
    PASSWORD 'mypassword',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**For detailed setup instructions, configuration examples, and IdP integration guides, see [Authentication Documentation](docs/AUTHENTICATION.md)**

**For step-by-step setup of OAuth, Key Pair, Okta, and MFA authentication, see [Authentication Setup Guide](docs/AUTHENTICATION_SETUP.md)**

### Setting Up Snowflake Credentials

#### Using Secrets (Recommended)

Create a named profile to securely store your Snowflake credentials:

**Creating a Secret:**

```sql
-- Secret with optional parameters (password authentication example)
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

#### `snowflake_query(query, profile)`

Executes SQL queries against Snowflake databases.

```sql
SELECT * FROM snowflake_query(
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
SELECT * FROM snowflake_query('SELECT 1', 'my_snowflake_secret');

-- Query with attached database
ATTACH '' AS snow (TYPE snowflake, SECRET my_snowflake_secret, READ_ONLY);
SELECT * FROM snow.public.customers LIMIT 10;
```

### Complex Analytics

```sql
-- Use Snowflake's computational power for complex analytics
SELECT * FROM snowflake_query(
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
FROM snowflake_query(
    'SELECT product_id, SUM(amount) as sales_amount FROM sales GROUP BY product_id',
    'my_snowflake_secret'
) sf
JOIN local_discounts local ON sf.product_id = local.product_id;
```

### Data Export Workflows

```sql
-- Export large dataset efficiently
CREATE TABLE local_fact_sales AS
SELECT * FROM snowflake_query(
    'SELECT * FROM fact_sales WHERE year >= 2020',
    'my_snowflake_secret'
);

-- Create Parquet files from Snowflake data
COPY (
    SELECT * FROM snowflake_query(
        'SELECT * FROM large_table',
        'my_snowflake_secret'
    )
) TO 'output.parquet' (FORMAT PARQUET);
```


## Filter & Projection Pushdown

The extension can optimize queries by pushing filters and column selections to Snowflake. **Pushdown is disabled by default** and must be explicitly enabled.

> **Requires DuckDB 1.4.1 build of the Snowflake extension.** Earlier versions of the extension do not include the pushdown planner improvements referenced below.

### Enabling Pushdown

Add `enable_pushdown true` to the ATTACH statement:

```sql
-- Pushdown DISABLED (default)
ATTACH '' AS snow (TYPE snowflake, SECRET my_secret, READ_ONLY);

-- Pushdown ENABLED
ATTACH '' AS snow (TYPE snowflake, SECRET my_secret, READ_ONLY, enable_pushdown true);
```

### How Pushdown Works (when enabled)

```sql
ATTACH '' AS snow (TYPE snowflake, SECRET my_secret, READ_ONLY, enable_pushdown true);

-- Simple filter and projection
SELECT id, name FROM snow.schema.customers WHERE age > 25;
-- Snowflake executes: SELECT "id", "name" FROM ... WHERE "age" > 25

-- Complex filters with IN and OR
SELECT * FROM snow.schema.orders
WHERE status IN ('PENDING', 'PROCESSING')
   OR (order_date >= '2024-01-01' AND order_date < '2024-02-01');
-- All filters pushed to Snowflake

-- Join queries with filter pushdown
SELECT c.id, c.name, n.country_name
FROM snow.schema.customers c
JOIN snow.schema.nations n ON c.nation_id = n.id
WHERE n.country_name = 'USA' AND c.id <= 1000;
-- Static filters pushed to both tables
```

**Supported Pushdown (current):**
- **Comparison filters**: `=`, `!=`, `<`, `>`, `<=`, `>=`, `IS NULL`, `IS NOT NULL`
- **Logical operators**: `AND`, `OR`
- **IN clauses**: `col IN (value1, value2, ...)` (converted to multiple OR conditions)

**Not yet implemented:** join-filter pushdown and projection pushdown. These optimizations are on the roadmap but disabled in the current build.

**Important: DuckDB's Optimizer Controls Pushdown**

When pushdown is enabled, DuckDB's query optimizer decides which filters to push down based on performance estimates. Not all filters in your query will necessarily be pushed to Snowflake:

```sql
-- With pushdown enabled
ATTACH '' AS snow (TYPE snowflake, SECRET my_secret, READ_ONLY, enable_pushdown true);

-- Query with multiple filters
SELECT * FROM snow.schema.customer
WHERE C_CUSTKEY > 100000 AND C_PHONE IS NOT NULL;

-- DuckDB may push down: WHERE C_CUSTKEY > 100000
-- DuckDB may apply locally: C_PHONE IS NOT NULL (cheap to evaluate after filtering)
```

This is **optimal behavior**. DuckDB keeps certain filters local when:
- The filter is very cheap to evaluate (e.g., `IS NOT NULL`)
- A prior filter already reduces the dataset significantly
- Local evaluation is faster than remote execution

The extension supports all standard comparison and null-check filters. DuckDB's optimizer will use them when it determines pushdown improves performance.

### snowflake_query (Pushdown Disabled)
```sql
-- User-provided SQL is executed as-is, no modification
SELECT * FROM snowflake_query('SELECT * FROM customers WHERE age > 25', 'my_secret');
```

Use `snowflake_query()` when you need full control over the SQL sent to Snowflake.

## Limitations

- **Read-only access**: All Snowflake operations are read-only
- **Function calls in filters**: Expressions like `WHERE UPPER(name) = 'FOO'` not pushed down
- **LIMIT pushdown**: Not supported for `ATTACH` - LIMIT is applied after fetching data from Snowflake

### Working with LIMIT

When using `ATTACH`, LIMIT clauses are applied locally by DuckDB after fetching all data:

```sql
-- LIMIT applied locally (fetches all rows, then limits)
SELECT * FROM snow.schema.customer LIMIT 100;
```

For efficient row sampling, use `snowflake_query()` with Snowflake's native sampling:

```sql
-- Option 1: LIMIT pushed to Snowflake
SELECT * FROM snowflake_query(
    'SELECT * FROM customer LIMIT 100',
    'my_secret'
);

-- Option 2: Snowflake SAMPLE clause (recommended for large tables)
SELECT * FROM snowflake_query(
    'SELECT * FROM customer SAMPLE (1000 ROWS)',
    'my_secret'
);

-- Option 3: Percentage-based sampling
SELECT * FROM snowflake_query(
    'SELECT * FROM customer SAMPLE (1)',  -- 1% of rows
    'my_secret'
);
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

### Debugging Tips

```sql
-- Test connection with a simple query
SELECT * FROM snowflake_query('SELECT 1', 'my_snowflake_secret');

-- Check profile configuration
SELECT * FROM duckdb_secrets() WHERE type = 'snowflake';

-- Verify warehouse status
SELECT * FROM snowflake_query(
    'SHOW WAREHOUSES',
    'my_snowflake_secret'
);
```

## Support

For issues or questions:

- Check the [GitHub repository](https://github.com/iqea-ai/duckdb-snowflake). Raise any feature requests/issues under issues
- Review Snowflake ADBC driver documentation (https://arrow.apache.org/adbc/main/driver/snowflake.html)
- Ensure you have the latest version of the extension

### For Developers

If you want to build the extension from source or contribute to development, see [BUILD.md](BUILD.md) for detailed build instructions and development guidelines.
