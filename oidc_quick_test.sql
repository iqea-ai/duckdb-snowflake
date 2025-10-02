-- =============================================================================
-- DuckDB Snowflake Extension - OIDC Quick Test
-- =============================================================================
-- Quick demonstration of OIDC authentication using DuckDB secrets
-- =============================================================================

-- Load extension
LOAD 'snowflake';

-- Create OIDC secret with your token
CREATE SECRET snowflake_oidc (
    TYPE snowflake,
    account 'YOUR_ACCOUNT',
    database 'YOUR_DATABASE',
    warehouse 'YOUR_WAREHOUSE',
    role 'YOUR_ROLE',
    oidc_token 'YOUR_JWT_ACCESS_TOKEN_HERE'  -- Replace with your actual token
);

-- Attach using secret (PREFERRED METHOD)
ATTACH '' AS sf (TYPE snowflake, SECRET snowflake_oidc, ACCESS_MODE READ_ONLY);

-- Test connection
SELECT COUNT(*) as table_count 
FROM sf.information_schema.tables 
WHERE table_schema = 'INFORMATION_SCHEMA';

-- Sample query
SELECT c_name, c_phone, c_acctbal 
FROM sf.tpch_sf1.customer 
LIMIT 5;

-- Cleanup
DETACH sf;
-- DROP SECRET snowflake_oidc;  -- Uncomment to remove secret
