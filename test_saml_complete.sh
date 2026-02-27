#!/bin/bash

# Complete SAML Authentication Test Script
# Tests Auth0 SAML authentication with DuckDB Snowflake extension

set -e

echo "=========================================="
echo "Testing SAML Authentication with Auth0"
echo "=========================================="
echo ""

# Check if DuckDB is available
if ! command -v duckdb &> /dev/null; then
    echo "ERROR: DuckDB not found in PATH"
    echo "Please install DuckDB or add it to your PATH"
    exit 1
fi

echo "✓ DuckDB found: $(duckdb --version 2>&1 | head -1)"
echo ""

# Set extension path (adjust if needed)
EXTENSION_PATH="../artifact_test/snowflake-v1.4.0-extension-osx_arm64/snowflake.duckdb_extension"

if [ ! -f "$EXTENSION_PATH" ]; then
    echo "ERROR: Extension not found at $EXTENSION_PATH"
    echo "Please update EXTENSION_PATH in this script"
    exit 1
fi

echo "✓ Extension found: $EXTENSION_PATH"
echo ""

# Create test SQL file
cat > /tmp/test_saml.sql << 'EOF'
-- Load extension
LOAD '/Users/venkata/ddbsf-ext/artifact_test/snowflake-v1.4.0-extension-osx_arm64/snowflake.duckdb_extension';

-- Disable progress bars
PRAGMA disable_progress_bar;
PRAGMA disable_print_progress_bar;

-- Create secret with EXT_BROWSER auth type (SAML)
CREATE SECRET my_auth0_saml (
    TYPE snowflake,
    ACCOUNT 'YFEQLOK-BD05926',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    AUTH_TYPE 'ext_browser'
);

-- Connect (browser will open for SAML authentication)
ATTACH '' AS sf (TYPE snowflake, SECRET my_auth0_saml, READ_ONLY);

-- Verify connection
SELECT 'Connection successful!' as status;
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf';

-- Test querying Snowflake data
SELECT COUNT(*) as table_count FROM sf.information_schema.tables;

-- List schemas
SELECT schema_name FROM sf.information_schema.schemata LIMIT 10;
EOF

echo "Starting SAML authentication test..."
echo "A browser window will open for authentication."
echo ""

# Run DuckDB with the test SQL
duckdb -unsigned < /tmp/test_saml.sql

echo ""
echo "=========================================="
echo "Test completed!"
echo "=========================================="










