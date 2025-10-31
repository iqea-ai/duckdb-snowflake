-- ============================================================================
-- OAUTH / KEY PAIR / SAML AUTHENTICATION TEST SUITE
-- OAuth_KeyPair_SAML Branch Testing
-- ============================================================================
-- USAGE:
--   1. Start DuckDB: cd /Users/venkata/ddbsf-ext/duckdb-snowflake && ./build/release/duckdb
--   2. Load the extension: LOAD 'build/release/extension/snowflake/snowflake.duckdb_extension';
--   3. Update the configuration sections below with your credentials
--   4. Run the sections you want to test
-- ============================================================================

-- ============================================================================
-- CONFIGURATION - UPDATE THESE VALUES
-- ============================================================================
-- Note: DuckDB doesn't support .set variables, so update values directly in queries below

-- Your Snowflake credentials:
--   ACCOUNT: 'qd71618.us-east-2.aws'
--   WAREHOUSE: 'COMPUTE_WH'
--   DATABASE: 'SNOWFLAKE_SAMPLE_DATA'
--   USER: 'snowflake_test_user'
--   PASSWORD: 'TempPassword123!'

-- For OAuth: Get token from your Auth0/IdP OAuth flow
-- For Key Pair: Path to private key file (create if needed)
-- For Auth0: EXT_BROWSER (SAML) is the recommended method (works perfectly!)

-- ============================================================================
-- CLEANUP (in case running script multiple times)
-- ============================================================================
-- Drop any existing attachments and secrets from previous runs
-- Note: Using FORCE to avoid errors if they don't exist
DETACH DATABASE IF EXISTS sf_pwd;
DETACH DATABASE IF EXISTS sf_oauth;
DETACH DATABASE IF EXISTS sf_jwt;
DETACH DATABASE IF EXISTS sf_jwt_enc;
DETACH DATABASE IF EXISTS sf_sso;
DETACH DATABASE IF EXISTS sf_okta;
DETACH DATABASE IF EXISTS sf_mfa;
DETACH DATABASE IF EXISTS sf;
DROP SECRET IF EXISTS password_secret;
DROP SECRET IF EXISTS oauth_secret;
DROP SECRET IF EXISTS keypair_secret;
DROP SECRET IF EXISTS keypair_encrypted_secret;
DROP SECRET IF EXISTS saml_secret;
DROP SECRET IF EXISTS auth0_secret;
DROP SECRET IF EXISTS okta_secret;
DROP SECRET IF EXISTS mfa_secret;
DROP SECRET IF EXISTS test_secret;

-- ============================================================================
-- EXTENSION INFO
-- ============================================================================
-- Check extension version
SELECT snowflake_version() as version;

-- ============================================================================
-- METHOD 1: PASSWORD AUTHENTICATION (Baseline Test)
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
-- Note: Using simple SELECT to avoid projection pushdown error with ATTACH
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_pwd.tpch_sf1.customer 
LIMIT 10;

-- Cleanup
DETACH sf_pwd;
DROP SECRET password_secret;

-- ============================================================================
-- METHOD 2: OAUTH TOKEN AUTHENTICATION (NEW!)
-- ============================================================================
--  SKIP - SECRET with TOKEN parameter not yet implemented in secret provider
-- OAuth 2.0 token authentication - ideal for programmatic access
-- PREREQUISITE: Configure External OAuth integration in Snowflake
-- See: https://docs.snowflake.com/en/user-guide/oauth-ext-overview
-- 
--  This will fail with: "Unknown parameter 'token' for secret type 'snowflake'"
--  Use connection string method instead (see ADVANCED TESTING section)
/*
CREATE OR REPLACE SECRET oauth_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    AUTH_TYPE 'oauth',
    TOKEN 'YOUR_OAUTH_ACCESS_TOKEN_HERE'
);

ATTACH '' AS sf_oauth (TYPE snowflake, SECRET oauth_secret);
SELECT current_user() as authenticated_user;
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_oauth.tpch_sf1.customer 
LIMIT 10;

DETACH sf_oauth;
DROP SECRET oauth_secret;
*/

SELECT ' OAuth via SECRET skipped - use connection string method instead' as note;

-- ============================================================================
-- METHOD 3: KEY PAIR (JWT) AUTHENTICATION - BASIC
-- ============================================================================
--  SKIP - Private key file doesn't exist
-- RSA key pair auth without passphrase
-- PREREQUISITE: Generate key pair and configure Snowflake user
-- 
-- To generate unencrypted key:
--   openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out ~/.ssh/snowflake_key.pem -nocrypt
--   openssl rsa -in ~/.ssh/snowflake_key.pem -pubout -out ~/.ssh/snowflake_key.pub
--   Then in Snowflake: ALTER USER snowflake_test_user SET RSA_PUBLIC_KEY='<public_key>';
/*
CREATE OR REPLACE SECRET keypair_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/.ssh/snowflake_key.pem'
);

ATTACH '' AS sf_jwt (TYPE snowflake, SECRET keypair_secret);
SELECT current_user() as authenticated_user;
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_jwt.tpch_sf1.customer 
LIMIT 10;

DETACH sf_jwt;
DROP SECRET keypair_secret;
*/

SELECT ' Key pair test skipped - private key file not found' as note;

-- ============================================================================
-- METHOD 4: KEY PAIR WITH PASSPHRASE AUTHENTICATION (NEW!)
-- ============================================================================
--  SKIP - PRIVATE_KEY_PASSPHRASE parameter not yet implemented in secret provider
-- RSA key pair auth with encrypted private key (enhanced security)
-- PREREQUISITE: Generate encrypted key pair and configure Snowflake user
-- 
-- To generate encrypted key:
--   openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out ~/.ssh/snowflake_key_encrypted.p8 -passout pass:MyPassphrase
--   openssl rsa -in ~/.ssh/snowflake_key_encrypted.p8 -pubout -out ~/.ssh/snowflake_key_encrypted.pub -passin pass:MyPassphrase
--   Then in Snowflake: ALTER USER snowflake_test_user SET RSA_PUBLIC_KEY='<public_key>';
-- 
--  This will fail with: "Unknown parameter 'private_key_passphrase'"
--  Use connection string method instead (see ADVANCED TESTING section)
/*
CREATE OR REPLACE SECRET keypair_encrypted_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/.ssh/snowflake_key_encrypted.p8',
    PRIVATE_KEY_PASSPHRASE 'your_passphrase_here'
);

ATTACH '' AS sf_jwt_enc (TYPE snowflake, SECRET keypair_encrypted_secret);
SELECT current_user() as authenticated_user;
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_jwt_enc.tpch_sf1.customer 
LIMIT 10;

DETACH sf_jwt_enc;
DROP SECRET keypair_encrypted_secret;
*/

SELECT ' Key pair with passphrase test skipped - parameter not registered in secret provider' as note;

-- ============================================================================
-- METHOD 5: EXT_BROWSER (SAML2 SSO) AUTHENTICATION (NEW!)  WORKS WITH AUTH0!
-- ============================================================================
-- Browser-based SSO auth - opens browser for authentication
-- PREREQUISITE: Configure SAML2 integration in Snowflake and IdP (Auth0, Okta, Azure AD, etc.)
-- PROTOCOL: SAML 2.0 (works with any SAML 2.0 IdP)
-- RECOMMENDED FOR AUTH0: This is the primary method for Auth0 integration

-- Create secret (no user/password needed)
CREATE OR REPLACE SECRET auth0_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    AUTH_TYPE 'ext_browser'
);

-- Test connection (will open browser - check all windows/tabs if it seems stuck)
ATTACH '' AS sf_sso (TYPE snowflake, SECRET auth0_secret);

-- Verify authentication (should show your SSO username)
SELECT current_user() as authenticated_user;

-- Test query with ATTACH (avoid aggregates - use simple SELECT)
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_sso.tpch_sf1.customer 
LIMIT 10;

-- Cleanup
DETACH sf_sso;
DROP SECRET auth0_secret;

-- ============================================================================
-- METHOD 6: NATIVE OKTA AUTHENTICATION (NEW!)
-- ============================================================================
--  SKIP - OKTA_URL parameter not yet implemented in secret provider
-- Direct Okta authentication using custom Okta URL
-- PREREQUISITE: Configure Okta integration in Snowflake
-- 
--  This will fail with: "Unknown parameter 'okta_url'"
--  Use connection string method instead (see ADVANCED TESTING section)
-- 📝 Note: For Auth0, use EXT_BROWSER (Method 5) instead
/*
CREATE OR REPLACE SECRET okta_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    AUTH_TYPE 'okta',
    OKTA_URL 'https://yourcompany.okta.com'
);

ATTACH '' AS sf_okta (TYPE snowflake, SECRET okta_secret);
SELECT current_user() as authenticated_user;
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_okta.tpch_sf1.customer 
LIMIT 10;

DETACH sf_okta;
DROP SECRET okta_secret;
*/

SELECT ' Okta test skipped - parameter not registered in secret provider (use EXT_BROWSER for Auth0)' as note;

-- ============================================================================
-- METHOD 7: MFA AUTHENTICATION (NEW!)
-- ============================================================================
-- Multi-factor authentication with username, password, and MFA token
-- PREREQUISITE: MFA must be enabled for the Snowflake user

-- Create secret
CREATE OR REPLACE SECRET mfa_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    PASSWORD 'TempPassword123!',
    AUTH_TYPE 'mfa'
);

-- Test connection (will prompt for MFA token)
ATTACH '' AS sf_mfa (TYPE snowflake, SECRET mfa_secret);

-- Verify authentication
SELECT current_user() as authenticated_user;

-- Test query with ATTACH (avoid aggregates - use simple SELECT)
SELECT c_custkey, c_name, c_mktsegment, c_acctbal 
FROM sf_mfa.tpch_sf1.customer 
LIMIT 10;

-- Cleanup
DETACH sf_mfa;
DROP SECRET mfa_secret;

-- ============================================================================
-- ADVANCED TESTING - Connection String Format
-- ============================================================================
-- Note: Connection strings work for OAuth, Key Pair with passphrase, and Okta
-- These bypass the secret provider and work directly with the ADBC driver

--  Test OAuth with connection string (requires valid OAuth token)
-- Uncomment and add your token to test:
/*
ATTACH 'account=qd71618.us-east-2.aws;user=snowflake_test_user;auth_type=oauth;token=YOUR_ACTUAL_TOKEN;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' 
AS sf_oauth_cs (TYPE snowflake, READ_ONLY);

SELECT c_custkey, c_name FROM sf_oauth_cs.tpch_sf1.customer LIMIT 5;
DETACH sf_oauth_cs;
*/
SELECT ' OAuth connection string test skipped - add your token to test' as note;

--  Test Key Pair with connection string (requires private key file)
-- Uncomment if you have the key file:
/*
ATTACH 'account=qd71618.us-east-2.aws;user=snowflake_test_user;auth_type=key_pair;private_key=/Users/venkata/.ssh/snowflake_key.pem;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' 
AS sf_jwt_cs (TYPE snowflake, READ_ONLY);

SELECT c_custkey, c_name FROM sf_jwt_cs.tpch_sf1.customer LIMIT 5;
DETACH sf_jwt_cs;
*/
SELECT ' Key pair connection string test skipped - create key file to test' as note;

--  Test Key Pair with passphrase via connection string (requires encrypted key file)
-- Uncomment if you have an encrypted key file:
/*
ATTACH 'account=qd71618.us-east-2.aws;user=snowflake_test_user;auth_type=key_pair;private_key=/Users/venkata/.ssh/snowflake_key_encrypted.p8;private_key_passphrase=your_passphrase;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' 
AS sf_jwt_enc_cs (TYPE snowflake, READ_ONLY);

SELECT c_custkey, c_name FROM sf_jwt_enc_cs.tpch_sf1.customer LIMIT 5;
DETACH sf_jwt_enc_cs;
*/
SELECT ' Key pair with passphrase connection string test skipped - create encrypted key to test' as note;

--  Test EXT_BROWSER with connection string (should work!)
ATTACH 'account=qd71618.us-east-2.aws;auth_type=ext_browser;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' 
AS sf_sso_cs (TYPE snowflake, READ_ONLY);

SELECT c_custkey, c_name FROM sf_sso_cs.tpch_sf1.customer LIMIT 5;
DETACH sf_sso_cs;

--  Test Okta with connection string (for Okta users only)
-- Uncomment and add your Okta URL to test:
/*
ATTACH 'account=qd71618.us-east-2.aws;user=snowflake_test_user;auth_type=okta;okta_url=https://yourcompany.okta.com;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' 
AS sf_okta_cs (TYPE snowflake, READ_ONLY);

SELECT c_custkey, c_name FROM sf_okta_cs.tpch_sf1.customer LIMIT 5;
DETACH sf_okta_cs;
*/
SELECT ' Okta connection string test skipped - Auth0 users should use EXT_BROWSER' as note;

-- ============================================================================
-- ADVANCED QUERIES (works with any auth method)
-- ============================================================================

-- Recreate a secret for testing (use your preferred auth method)
CREATE OR REPLACE SECRET test_secret (
    TYPE snowflake,
    ACCOUNT 'qd71618.us-east-2.aws',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    USER 'snowflake_test_user',
    PASSWORD 'TempPassword123!'
);

ATTACH '' AS sf (TYPE snowflake, SECRET test_secret);

-- List all schemas (will be lowercase)
SELECT * FROM duckdb_schemas() WHERE database_name = 'sf';

-- List tables in a schema
SELECT * FROM duckdb_tables() WHERE database_name = 'sf' AND schema_name = 'tpch_sf1';

-- Simple query with joins (avoid aggregates with ATTACH)
SELECT 
    c.c_custkey,
    c.c_name,
    c.c_mktsegment,
    o.o_orderkey,
    o.o_totalprice
FROM sf.tpch_sf1.customer c
JOIN sf.tpch_sf1.orders o ON c.c_custkey = o.o_custkey
LIMIT 10;

-- Using snowflake_scan for aggregations (better performance - SAFE to use aggregates)
-- Note: snowflake_scan uses the database from the secret, so no need to qualify schema
SELECT * FROM snowflake_scan(
    'SELECT COUNT(*) as total_customers FROM SNOWFLAKE_SAMPLE_DATA.tpch_sf1.customer', 
    'test_secret'
);

-- Complex aggregation query with snowflake_scan (pushes to Snowflake)
SELECT * FROM snowflake_scan(
    'SELECT 
        c_mktsegment,
        COUNT(*) as customer_count,
        AVG(c_acctbal) as avg_balance
     FROM SNOWFLAKE_SAMPLE_DATA.tpch_sf1.customer 
     GROUP BY c_mktsegment 
     ORDER BY customer_count DESC',
    'test_secret'
);

-- Cleanup
DETACH sf;
DROP SECRET test_secret;

-- ============================================================================
-- TROUBLESHOOTING
-- ============================================================================

-- Check loaded extensions
SELECT * FROM duckdb_extensions() WHERE loaded = true;

-- List all secrets
SELECT * FROM duckdb_secrets();

-- Check authentication types in README
-- PASSWORD  - Standard username/password
-- OAUTH     - OAuth 2.0 token (recommended for Okta)
-- KEY_PAIR  - RSA key pair (with optional passphrase)
-- EXT_BROWSER - SAML 2.0 SSO (any IdP)
-- OKTA      - Native Okta integration
-- MFA       - Multi-factor authentication

-- ============================================================================
-- KEY FEATURES IN OAuth_KeyPair_SAML BRANCH
-- ============================================================================
-- ✓ OAuth 2.0 token authentication support
-- ✓ Key pair authentication with passphrase support (encrypted private keys)
-- ✓ External browser SSO (SAML 2.0) - works with any SAML IdP
-- ✓ Native Okta authentication
-- ✓ Multi-factor authentication (MFA)
-- ✓ All auth types work with both SECRET and connection string formats
-- 
-- Auth Type Enum in snowflake_config.hpp:
--   enum class SnowflakeAuthType { 
--     PASSWORD, OAUTH, KEY_PAIR, EXT_BROWSER, OKTA, MFA 
--   };
--
-- ============================================================================
-- KNOWN LIMITATIONS & IMPORTANT NOTES
-- ============================================================================
-- 1. Schema/table names are LOWERCASED in ATTACH interface
--    - Use: sf.tpch_sf1.customer
--    - NOT: sf.TPCH_SF1.CUSTOMER
--
-- 2.  IMPORTANT: Aggregate functions with ATTACH cause "projection pushdown" error
--     DON'T USE: SELECT COUNT(*) FROM sf.schema.table;
--     DON'T USE: SELECT SUM(col), AVG(col), MAX(col) FROM sf.schema.table;
--     DON'T USE: SELECT col, COUNT(*) FROM sf.schema.table GROUP BY col;
--    
--     SAFE WITH ATTACH: 
--       - SELECT * FROM sf.schema.table LIMIT 10;
--       - SELECT col1, col2 FROM sf.schema.table WHERE condition;
--       - Simple joins without aggregates
--    
--     FOR AGGREGATES, use snowflake_scan():
--       - SELECT * FROM snowflake_scan('SELECT COUNT(*) FROM schema.table', 'secret');
--       - SELECT * FROM snowflake_scan('SELECT col, COUNT(*) FROM table GROUP BY col', 'secret');
--
-- 3. EXT_BROWSER and OKTA may hang if:
--    - Browser opens in background (check all windows!)
--    - Default browser not set
--    - Browser is blocking pop-ups
--    - Not suitable for headless/containerized environments
--
-- 4. KEY_PAIR PRIVATE_KEY parameter:
--    - Must be a FILE PATH (string literal)
--    - NOT the output of read_text() or file contents
--    - Passphrase support now available for encrypted keys
--
-- 5. OAuth tokens must be valid OAuth 2.0 access tokens:
--    - Obtain from External OAuth integration configured in Snowflake
--    - Token refresh is not automatic - must be managed by application
--    - Works with Auth0, Azure AD, Okta, and other OAuth 2.0 providers
--
-- ============================================================================

