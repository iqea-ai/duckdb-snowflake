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

#### Password Authentication
```sql
-- 1. Create a Snowflake profile with password authentication
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    USER 'YOUR_USERNAME',
    PASSWORD 'YOUR_PASSWORD',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE'
);

-- 2. Query Snowflake data
SELECT * FROM snowflake_scan(
    'SELECT * FROM customers WHERE state = ''CA''',
    'my_snowflake_secret'
);
```

#### OIDC Authentication (Recommended)
```sql
-- 1. Create a Snowflake profile with OIDC authentication
CREATE SECRET my_snowflake_oidc (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    ROLE 'YOUR_ROLE',
    OIDC_TOKEN 'YOUR_JWT_ACCESS_TOKEN_HERE'
);

-- 2. Attach and query using OIDC
ATTACH '' AS snow_db (TYPE snowflake, SECRET my_snowflake_oidc, ACCESS_MODE READ_ONLY);
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
- **Multiple Authentication Methods**: Password-based, OAuth, Key Pair, Workload Identity, and OIDC authentication
- **OIDC Support**: Full OpenID Connect authentication with PKCE security and DuckDB secrets integration
- **Secret Management**: Secure credential storage using DuckDB's secrets system (recommended for OIDC)
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

### Manual Installation

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

# 3. Move to DuckDB extensions directory
mkdir -p ~/.duckdb/extensions/v1.4.0/<PLATFORM>
mv adbc_driver_snowflake/libadbc_driver_snowflake.so ~/.duckdb/extensions/v1.4.0/<PLATFORM>/

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

# Install
mkdir -p ~/.duckdb/extensions/v1.4.0/linux_amd64
mv adbc_driver_snowflake/libadbc_driver_snowflake.so ~/.duckdb/extensions/v1.4.0/linux_amd64/

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

# Place in DuckDB extensions directory
mkdir C:\Users\%USERNAME%\.duckdb\extensions\v1.4.0\windows_amd64
move libadbc_driver_snowflake.so C:\Users\%USERNAME%\.duckdb\extensions\v1.4.0\windows_amd64\
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

**Creating a Secret with Password Authentication:**

```sql
-- Secret with password authentication
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    USER 'YOUR_USERNAME',
    PASSWORD 'YOUR_PASSWORD',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    SCHEMA 'YOUR_SCHEMA'  -- Optional: default schema
);
```

**Creating a Secret with OIDC Authentication:**

```sql
-- Secret with OIDC authentication (recommended for production)
CREATE SECRET my_snowflake_oidc (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    ROLE 'YOUR_ROLE',
    OIDC_TOKEN 'YOUR_JWT_ACCESS_TOKEN_HERE'
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

## OIDC Authentication

The DuckDB Snowflake extension supports OpenID Connect (OIDC) authentication with DuckDB secrets as the default and recommended method. OIDC provides enhanced security through JWT tokens and eliminates the need for password management.

### OIDC Authentication Methods

#### Pre-obtained Token (Recommended for Production)

Use this method when you already have a valid JWT access token from your OIDC provider:

```sql
-- Create secret with existing OIDC token
CREATE SECRET snowflake_oidc (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    ROLE 'YOUR_ROLE',
    OIDC_TOKEN 'YOUR_JWT_ACCESS_TOKEN_HERE'
);

-- Use immediately
ATTACH '' AS sf (TYPE snowflake, SECRET snowflake_oidc, ACCESS_MODE READ_ONLY);
SELECT * FROM sf.YOUR_SCHEMA.YOUR_TABLE LIMIT 10;
```

#### Interactive OIDC Flow

Use this method to trigger an interactive OIDC authentication flow:

```sql
-- Create secret with OIDC configuration
CREATE SECRET snowflake_oidc_interactive (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    ROLE 'YOUR_ROLE',
    OIDC_CLIENT_ID 'YOUR_OKTA_CLIENT_ID',
    OIDC_ISSUER_URL 'https://YOUR_OKTA_DOMAIN.okta.com/oauth2/YOUR_AUTH_SERVER_ID',
    OIDC_REDIRECT_URI 'http://localhost:8080/callback',
    OIDC_SCOPE 'openid profile email'
);

-- This will trigger interactive OIDC flow
ATTACH '' AS sf (TYPE snowflake, SECRET snowflake_oidc_interactive, ACCESS_MODE READ_ONLY);
```

### OIDC Configuration Parameters

**Required Parameters:**
- `ACCOUNT` - Your Snowflake account identifier
- `DATABASE` - Target database name

**Optional Parameters:**
- `WAREHOUSE` - Snowflake warehouse name
- `ROLE` - Snowflake role name
- `SCHEMA` - Default schema name

**OIDC Authentication Parameters:**
- `OIDC_TOKEN` - Pre-obtained JWT access token (recommended for production)
- `OIDC_CLIENT_ID` - OIDC client ID for interactive flow
- `OIDC_ISSUER_URL` - OIDC issuer URL (e.g., Okta authorization server)
- `OIDC_REDIRECT_URI` - OAuth redirect URI
- `OIDC_SCOPE` - OAuth scopes (default: "openid")

### OIDC Security Features

- **PKCE (Proof Key for Code Exchange)** - Prevents authorization code interception attacks
- **State Parameter Validation** - Protects against CSRF attacks
- **Encrypted Credential Storage** - OIDC tokens stored securely in DuckDB secrets
- **JWT Token Validation** - Automatic token validation and username extraction
- **Cross-Platform Browser Support** - Automatic browser launch for interactive flows

### OIDC Provider Support

The extension works with any OIDC-compliant provider including Okta, Auth0, Azure AD, Google Identity, and custom OIDC providers.

### OIDC Best Practices

**Use Descriptive Secret Names:**
```sql
CREATE SECRET snowflake_prod_analytics (...);
```

**Environment Separation:**
```sql
CREATE SECRET snowflake_dev (...);
CREATE SECRET snowflake_staging (...);
CREATE SECRET snowflake_prod (...);
```

**Token Rotation:**
```sql
DROP SECRET snowflake_prod;
CREATE SECRET snowflake_prod (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    OIDC_TOKEN 'NEW_ROTATED_TOKEN_HERE'
);
```

**Minimal Permissions:**
```sql
CREATE SECRET snowflake_readonly (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    ROLE 'READONLY_ROLE',
    OIDC_TOKEN 'READONLY_TOKEN_HERE'
);
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

### Password Authentication
1. **Use Strong Passwords**: Create complex passwords for your Snowflake accounts
2. **Principle of Least Privilege**: Use Snowflake roles with minimal required permissions
3. **Regular Rotation**: Update passwords and credentials regularly
4. **Environment Separation**: Use different secrets for dev/test/prod environments
5. **Secure Storage**: Secrets are stored encrypted in DuckDB's internal storage

### OIDC Authentication (Recommended)
1. **Use OIDC Tokens**: Prefer OIDC authentication over passwords for enhanced security
2. **Token Rotation**: Regularly rotate OIDC tokens for better security
3. **PKCE Security**: The extension automatically uses PKCE for secure OIDC flows
4. **State Validation**: CSRF protection is built into the OIDC implementation
5. **Encrypted Storage**: OIDC tokens are stored securely in DuckDB secrets
6. **Provider Integration**: Works with enterprise identity providers (Okta, Auth0, Azure AD)
7. **No Password Storage**: Eliminates password management and storage risks

## Support

For issues or questions:

- Check the [GitHub repository](https://github.com/your-repo/duckdb-snowflake)
- Review Snowflake ADBC driver documentation
- Ensure you have the latest version of the extension

### For Developers

If you want to build the extension from source or contribute to development, see [BUILD.md](BUILD.md) for detailed build instructions and development guidelines.# Trigger new workflow run
# Test build with DuckDB 1.4.0
