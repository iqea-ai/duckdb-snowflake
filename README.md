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

## Configuration

### Authentication Methods

The DuckDB Snowflake extension supports multiple authentication methods to connect to Snowflake. Choose the method that best fits your security requirements and infrastructure.

#### Authentication Protocol Overview

| Method | Protocol | Browser Required | Use Case |
|--------|----------|------------------|----------|
| **Password** | Username/Password | No | Development, simple setups |
| **OAuth** | OAuth 2.0 | No | Programmatic access, APIs, automation, **recommended for Okta** |
| **Key Pair** | RSA Key Pair | No | Production, headless servers, highest security |
| **EXT_BROWSER** | SAML 2.0 | Yes | Interactive SSO, any SAML 2.0 IdP (Okta, AD FS) |
| **OKTA** | Native Okta API | Yes | Okta-specific native integration |
| **MFA** | Password + MFA | No | Enhanced password security |

**Important Notes:**
- **For Okta users**: OAuth is recommended over EXT_BROWSER/SAML for better security, flexibility, and programmatic access
- **EXT_BROWSER uses SAML 2.0**: Works with any SAML 2.0-compliant identity provider including Okta, AD FS, Azure AD
- **OAuth uses OAuth 2.0**: Ideal for headless environments, automation, and modern applications
- **Browser-based methods** (EXT_BROWSER, OKTA) require interactive browser access and won't work in containerized/headless environments

#### 1. Password Authentication (Default)

Standard username and password authentication.

```sql
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    PASSWORD 'mypassword',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;password=mypassword;database=mydb;warehouse=mywh
```

#### 2. OAuth Token Authentication

Use **OAuth 2.0** tokens for authentication. **Recommended for Okta environments** and ideal for programmatic access.

**Protocol**: OAuth 2.0

**Advantages**:
- No browser required (works in headless/containerized environments)
- Supports programmatic authentication for APIs and automation
- Better security with configurable authorization flows
- Token-based access control with granular permissions
- Recommended by Snowflake for Okta integration

**How to obtain tokens**:
- Configure External OAuth integration in Snowflake with your IdP (Okta, Azure AD, etc.)
- Use your IdP's OAuth flow to obtain access tokens
- See [Snowflake External OAuth documentation](https://docs.snowflake.com/en/user-guide/oauth-ext-overview)

```sql
CREATE SECRET my_oauth_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'oauth',
    TOKEN 'your_oauth_access_token',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=oauth;token=your_oauth_access_token;database=mydb;warehouse=mywh
```

**Note**: The token must be a valid OAuth 2.0 access token obtained from your configured External OAuth provider.

#### 3. Key Pair Authentication

Use RSA key pairs for secure, password-less authentication.

**Basic Key Pair (Unencrypted Private Key):**

```sql
CREATE SECRET my_keypair_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/path/to/rsa_key.p8',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=key_pair;private_key=/path/to/rsa_key.p8;database=mydb;warehouse=mywh
```

**Key Pair with Encrypted Private Key:**

For enhanced security, use a passphrase-protected private key (PKCS#8 format).

```sql
CREATE SECRET my_secure_keypair_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/path/to/encrypted_rsa_key.p8',
    PRIVATE_KEY_PASSPHRASE 'your_passphrase',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=key_pair;private_key=/path/to/encrypted_rsa_key.p8;private_key_passphrase=your_passphrase;database=mydb;warehouse=mywh
```

**Generating Key Pairs:**

```bash
# Generate private key with passphrase
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out rsa_key.p8 -passout pass:your_passphrase

# Generate public key
openssl rsa -in rsa_key.p8 -pubout -out rsa_key.pub -passin pass:your_passphrase

# Assign public key to Snowflake user
# In Snowflake, run:
# ALTER USER myusername SET RSA_PUBLIC_KEY='<public_key_content>';
```

#### 4. External Browser SSO Authentication

Authenticate using your web browser with **SAML 2.0** identity providers (Okta, AD FS, Azure AD, etc.). This opens a browser window for SSO login.

**Protocol**: SAML 2.0 (Security Assertion Markup Language)

**How it works**:
1. Client initiates authentication
2. Browser window opens to your IdP login page
3. User authenticates with IdP (e.g., Okta)
4. IdP sends SAML assertion to Snowflake
5. Snowflake validates assertion and creates session

```sql
CREATE SECRET my_sso_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'ext_browser',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=ext_browser;database=mydb;warehouse=mywh
```

**Note:** External browser authentication is also supported with the alias `externalbrowser`:
```
account=myaccount;user=myusername;auth_type=externalbrowser;database=mydb;warehouse=mywh
```

**Limitations**:
- Requires interactive browser access (not suitable for headless/containerized environments)
- User must be present to authenticate
- For Okta environments, consider using OAuth instead for programmatic access

#### 5. Native Okta Authentication

Direct authentication with Okta using a custom Okta URL.

```sql
CREATE SECRET my_okta_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'okta',
    OKTA_URL 'https://yourcompany.okta.com',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=okta;okta_url=https://yourcompany.okta.com;database=mydb;warehouse=mywh
```

#### 6. Multi-Factor Authentication (MFA)

Authenticate using username, password, and MFA token.

```sql
CREATE SECRET my_mfa_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    PASSWORD 'mypassword',
    AUTH_TYPE 'mfa',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;password=mypassword;auth_type=mfa;database=mydb;warehouse=mywh
```

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


## Support

For issues or questions:

- Check the [GitHub repository](https://github.com/iqea-ai/duckdb-snowflake). Raise any feature requests/issues under issues
- Review Snowflake ADBC driver documentation (https://arrow.apache.org/adbc/main/driver/snowflake.html)
- Ensure you have the latest version of the extension

