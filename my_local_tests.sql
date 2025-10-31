-- ============================================================================
-- LOCAL TEST FILE - NOT TRACKED BY GIT
-- This file is for manual testing of all Snowflake authentication methods
-- ============================================================================
-- USAGE:
--   1. Update the configuration sections below with your credentials
--   2. Load the extension: LOAD 'build/release/extension/snowflake/snowflake.duckdb_extension';
--   3. Run the sections you want to test
-- ============================================================================

-- ============================================================================
-- CONFIGURATION - UPDATE THESE VALUES
-- ============================================================================

-- Your Snowflake account (account locator format: qd71618.us-east-2.aws)
-- Find it at: https://app.snowflake.com/ (in the URL: https://<account>.snowflakecomputing.com)
.set snowflake_account 'qd71618.us-east-2.aws'

-- Warehouse and database
.set warehouse 'COMPUTE_WH'
.set database 'SNOWFLAKE_SAMPLE_DATA'

-- Password authentication credentials
.set username 'snowflake_test_user'
.set password 'TempPassword123!'

-- Key pair authentication (path to your private key file)
.set private_key_path '/Users/venkata/.ssh/snowflake_key.pem'
.set private_key_passphrase ''  -- Leave empty if no passphrase

-- ============================================================================
-- METHOD 1: PASSWORD AUTHENTICATION
-- ============================================================================
-- Simple username/password auth - works immediately, no browser needed

-- Create secret
CREATE OR REPLACE SECRET password_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    PASSWORD 'TempPassword123!'
);

-- Test connection
ATTACH '' AS sf_pwd (TYPE snowflake, SECRET password_secret);

-- Verify authentication
SELECT current_user() as authenticated_user;

-- Test query (remember: lowercase schema/table names!)
SELECT COUNT(*) FROM sf_pwd.tpch_sf1.customer;

-- Cleanup
DETACH sf_pwd;
DROP SECRET password_secret;

-- ============================================================================
-- METHOD 2: KEY PAIR (JWT) AUTHENTICATION
-- ============================================================================
-- RSA key pair auth - for automation/headless environments
-- PREREQUISITE: Run scripts/generate_snowflake_keypair.sh and configure user

-- Create secret (PRIVATE_KEY expects a file path, not file contents)
CREATE OR REPLACE SECRET keypair_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/.ssh/snowflake_key.pem'
    -- PRIVATE_KEY_PASSPHRASE 'your_passphrase'  -- Uncomment if key is encrypted
);

-- Test connection
ATTACH '' AS sf_jwt (TYPE snowflake, SECRET keypair_secret);

-- Verify authentication
SELECT current_user() as authenticated_user;

-- Test query
SELECT COUNT(*) FROM sf_jwt.tpch_sf1.customer;

-- Test with snowflake_scan (pushes aggregation to Snowflake)
SELECT COUNT(*) 
FROM snowflake_scan('keypair_secret', 'tpch_sf1.customer');

-- Cleanup
DETACH sf_jwt;
DROP SECRET keypair_secret;

-- ============================================================================
-- METHOD 3: EXT_BROWSER (SAML2 SSO) AUTHENTICATION
-- ============================================================================
-- Browser-based SSO auth - opens browser for authentication
-- PREREQUISITE: Configure SAML2 integration in Snowflake and IdP (Auth0/Okta)

-- Create secret (no user/password needed)
CREATE OR REPLACE SECRET saml_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    AUTH_TYPE 'ext_browser'
);

-- Test connection (will open browser - check all windows/tabs if it seems stuck)
ATTACH '' AS sf_sso (TYPE snowflake, SECRET saml_secret);

-- Verify authentication (should show your SSO username)
SELECT current_user() as authenticated_user;

-- Test query
SELECT COUNT(*) FROM sf_sso.tpch_sf1.customer;

-- Cleanup
DETACH sf_sso;
DROP SECRET saml_secret;

-- ============================================================================
-- ADVANCED QUERIES
-- ============================================================================

-- List all schemas (will be lowercase)
SELECT * FROM duckdb_schemas() WHERE database_name = 'sf_pwd';

-- List tables in a schema
SELECT * FROM duckdb_tables() WHERE database_name = 'sf_pwd' AND schema_name = 'tpch_sf1';

-- Complex query with joins (remember: use lowercase!)
SELECT 
    c.c_name,
    c.c_mktsegment,
    COUNT(o.o_orderkey) as order_count,
    SUM(o.o_totalprice) as total_spent
FROM sf_pwd.tpch_sf1.customer c
JOIN sf_pwd.tpch_sf1.orders o ON c.c_custkey = o.o_custkey
GROUP BY c.c_name, c.c_mktsegment
ORDER BY total_spent DESC
LIMIT 10;

-- Using snowflake_scan for aggregations (better performance)
SELECT COUNT(*) as total_customers
FROM snowflake_scan('password_secret', 'tpch_sf1.customer');

-- ============================================================================
-- TROUBLESHOOTING
-- ============================================================================

-- Check loaded extensions
SELECT * FROM duckdb_extensions() WHERE loaded = true;

-- List all secrets
SELECT * FROM duckdb_secrets();

-- Drop all secrets (cleanup)
-- DROP SECRET IF EXISTS password_secret;
-- DROP SECRET IF EXISTS keypair_secret;
-- DROP SECRET IF EXISTS saml_secret;

-- ============================================================================
-- KNOWN LIMITATIONS
-- ============================================================================
-- 1. Schema/table names are LOWERCASED in ATTACH interface
--    - Use: sf.tpch_sf1.customer
--    - NOT: sf.TPCH_SF1.CUSTOMER
--
-- 2. Aggregate functions with ATTACH may fail with "projection pushdown" error
--    - Solution: Use snowflake_scan() instead, which pushes to Snowflake
--    - Example: SELECT COUNT(*) FROM snowflake_scan('secret', 'schema.table');
--
-- 3. EXT_BROWSER may hang if:
--    - Browser opens in background (check all windows!)
--    - Default browser not set
--    - Browser is blocking pop-ups
--
-- 4. KEY_PAIR PRIVATE_KEY parameter:
--    - Must be a FILE PATH (string literal)
--    - NOT the output of read_text() or file contents
--
-- ============================================================================


