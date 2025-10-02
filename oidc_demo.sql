-- =============================================================================
-- DuckDB Snowflake Extension - OIDC Authentication Demo
-- =============================================================================
-- This file demonstrates the complete OIDC authentication functionality
-- using DuckDB secrets as the default and recommended method.
-- =============================================================================

-- Step 1: Load the Snowflake extension
-- =============================================================================
LOAD 'snowflake';

-- Step 2: Create OIDC Secret (PREFERRED METHOD)
-- =============================================================================
-- This is the recommended way to configure OIDC authentication
-- All credentials are stored securely in DuckDB's secret manager

-- Option A: Using a pre-obtained OIDC token (most common)
CREATE SECRET snowflake_oidc (
    TYPE snowflake,
    account 'YOUR_ACCOUNT',                       -- Your Snowflake account
    database 'YOUR_DATABASE',                     -- Database to connect to
    warehouse 'YOUR_WAREHOUSE',                   -- Warehouse to use
    role 'YOUR_ROLE',                             -- Role to assume
    oidc_token 'YOUR_JWT_ACCESS_TOKEN_HERE'       -- Your JWT access token
);

-- Option B: Using OIDC configuration for interactive flow
CREATE SECRET snowflake_oidc_config (
    TYPE snowflake,
    account 'YOUR_ACCOUNT',
    database 'YOUR_DATABASE',
    warehouse 'YOUR_WAREHOUSE',
    role 'YOUR_ROLE',
    oidc_client_id 'YOUR_OKTA_CLIENT_ID',                     -- Your Okta client ID
    oidc_issuer_url 'https://YOUR_OKTA_DOMAIN.okta.com/oauth2/YOUR_AUTH_SERVER_ID',  -- Okta issuer URL
    oidc_redirect_uri 'http://localhost:8080/callback',       -- Redirect URI
    oidc_scope 'openid profile email'                         -- OIDC scopes
);

-- Step 3: Verify Secret Creation
-- =============================================================================
-- List all available secrets to confirm creation
SELECT name, type FROM duckdb_secrets();

-- Step 4: Attach Snowflake using OIDC Secret (PREFERRED METHOD)
-- =============================================================================
-- This uses the secret-based authentication (recommended)
ATTACH '' AS snowflake_db (TYPE snowflake, SECRET snowflake_oidc, ACCESS_MODE READ_ONLY);

-- Alternative: Attach using OIDC configuration (will trigger interactive flow)
-- ATTACH '' AS snowflake_config_db (TYPE snowflake, SECRET snowflake_oidc_config, ACCESS_MODE READ_ONLY);

-- Step 5: Verify Connection
-- =============================================================================
-- Test the connection by querying Snowflake system tables
SELECT 
    table_catalog,
    table_schema,
    table_name,
    table_type
FROM snowflake_db.information_schema.tables 
WHERE table_schema = 'INFORMATION_SCHEMA'
LIMIT 10;

-- Step 6: Explore Available Data
-- =============================================================================
-- List all schemas in the connected database
SELECT schema_name 
FROM snowflake_db.information_schema.schemata 
ORDER BY schema_name;

-- List tables in a specific schema (e.g., TPCH_SF1)
SELECT table_name, table_type
FROM snowflake_db.information_schema.tables 
WHERE table_schema = 'TPCH_SF1'
ORDER BY table_name;

-- Step 7: Execute Sample Queries
-- =============================================================================
-- Query 1: Customer data from TPCH sample
SELECT 
    c_custkey,
    c_name,
    c_address,
    c_nationkey,
    c_phone,
    c_acctbal
FROM snowflake_db.tpch_sf1.customer 
LIMIT 5;

-- Query 2: Order data with aggregations
SELECT 
    o_orderdate,
    COUNT(*) as order_count,
    SUM(o_totalprice) as total_revenue
FROM snowflake_db.tpch_sf1.orders 
WHERE o_orderdate >= '1992-01-01'
GROUP BY o_orderdate
ORDER BY o_orderdate
LIMIT 10;

-- Query 3: Join customer and order data
SELECT 
    c.c_name as customer_name,
    c.c_phone as phone,
    o.o_orderdate,
    o.o_totalprice as order_total
FROM snowflake_db.tpch_sf1.customer c
JOIN snowflake_db.tpch_sf1.orders o ON c.c_custkey = o.o_custkey
WHERE o.o_orderdate >= '1992-01-01'
ORDER BY o.o_totalprice DESC
LIMIT 10;

-- Step 8: Advanced OIDC Features
-- =============================================================================
-- Check current connection details
SELECT 
    current_database() as current_db,
    current_schema() as current_schema,
    current_warehouse() as current_warehouse,
    current_role() as current_role;

-- Query with filtering and sorting
SELECT 
    p_name as product_name,
    p_brand as brand,
    p_type as product_type,
    p_retailprice as retail_price
FROM snowflake_db.tpch_sf1.part
WHERE p_retailprice > 1000
ORDER BY p_retailprice DESC
LIMIT 20;

-- Step 9: Performance Testing
-- =============================================================================
-- Test query performance with larger dataset
SELECT 
    l_shipmode,
    COUNT(*) as order_count,
    AVG(l_quantity) as avg_quantity,
    SUM(l_extendedprice) as total_revenue
FROM snowflake_db.tpch_sf1.lineitem
WHERE l_shipdate >= '1992-01-01' AND l_shipdate < '1993-01-01'
GROUP BY l_shipmode
ORDER BY total_revenue DESC;

-- Step 10: Cleanup (Optional)
-- =============================================================================
-- Detach the Snowflake database
DETACH snowflake_db;

-- Drop the secrets (optional - keep them for future use)
-- DROP SECRET snowflake_oidc;
-- DROP SECRET snowflake_oidc_config;

-- =============================================================================
-- OIDC Authentication Methods Summary
-- =============================================================================
-- 
-- 1. PREFERRED METHOD (DuckDB Secrets):
--    - CREATE SECRET with oidc_token parameter
--    - ATTACH using SECRET option
--    - Most secure and user-friendly
--
-- 2. CONFIGURATION METHOD (Interactive Flow):
--    - CREATE SECRET with OIDC configuration parameters
--    - ATTACH will trigger interactive browser flow
--    - Requires manual completion of OAuth flow
--
-- 3. LEGACY METHOD (Connection String):
--    - Direct connection string with OIDC parameters
--    - Less secure, not recommended for production
--
-- =============================================================================
-- Key Benefits of OIDC Authentication:
-- =============================================================================
-- 
-- ✅ Security: JWT tokens with expiration and scope control
-- ✅ SSO Integration: Works with enterprise identity providers
-- ✅ No Password Storage: Eliminates password management
-- ✅ Audit Trail: Better logging and compliance
-- ✅ Token Refresh: Automatic token renewal support
-- ✅ DuckDB Secrets: Encrypted credential storage
--
-- =============================================================================
-- Troubleshooting:
-- =============================================================================
-- 
-- If you encounter issues:
-- 1. Verify your OIDC token is valid and not expired
-- 2. Check that your Snowflake account allows OIDC authentication
-- 3. Ensure the token has the correct scopes and permissions
-- 4. Verify your Snowflake account, warehouse, database, and role settings
-- 5. Check that the extension is properly loaded
--
-- =============================================================================
