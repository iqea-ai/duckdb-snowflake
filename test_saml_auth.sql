-- Test SAML Authentication with Auth0
-- Run this in DuckDB CLI: duckdb -unsigned < test_saml_auth.sql
-- Or use: duckdb -unsigned and paste these commands

-- Step 1: Load the Snowflake extension
-- Install and load from community (latest version)
INSTALL snowflake FROM community;
LOAD snowflake;

-- Step 2: Create secret with EXT_BROWSER auth type (SAML)
CREATE SECRET my_auth0_saml (
    TYPE snowflake,
    ACCOUNT 'YFEQLOK-BD05926',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    AUTH_TYPE 'ext_browser'
);

-- Step 3: Connect (browser will open for SAML authentication)
-- This will redirect to Auth0, then back to Snowflake
ATTACH '' AS sf (TYPE snowflake, SECRET my_auth0_saml, READ_ONLY);

-- Step 4: Verify connection
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf';

-- Step 5: Test querying Snowflake data
SELECT COUNT(*) FROM sf.information_schema.tables;

-- Step 6: List schemas
SELECT schema_name FROM sf.information_schema.schemata LIMIT 10;

-- Step 7: Test a simple query on TPCH data (if available)
-- SELECT COUNT(*) FROM sf.tpch_sf10.customer;

-- Step 8: Cleanup (optional)
-- DETACH sf;
-- DROP SECRET my_auth0_saml;










