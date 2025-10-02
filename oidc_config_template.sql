-- =============================================================================
-- DuckDB Snowflake Extension - OIDC Configuration Templates
-- =============================================================================
-- Template configurations for different OIDC authentication scenarios
-- =============================================================================

-- Load the Snowflake extension
LOAD 'snowflake';

-- =============================================================================
-- Template 1: Pre-obtained OIDC Token (Most Common)
-- =============================================================================
-- Use this when you already have a valid JWT access token
-- Replace the values with your actual configuration

CREATE SECRET snowflake_oidc_token (
    TYPE snowflake,
    account 'YOUR_ACCOUNT',                            -- e.g., 'FLWILYJ-FL43292'
    database 'YOUR_DATABASE',                          -- e.g., 'SNOWFLAKE_SAMPLE_DATA'
    warehouse 'YOUR_WAREHOUSE',                        -- e.g., 'COMPUTE_WH'
    role 'YOUR_ROLE',                                  -- e.g., 'PUBLIC'
    oidc_token 'YOUR_JWT_ACCESS_TOKEN_HERE'            -- Your actual JWT token
);

-- Attach using the token
ATTACH '' AS snowflake_db (TYPE snowflake, SECRET snowflake_oidc_token, ACCESS_MODE READ_ONLY);

-- =============================================================================
-- Template 2: OIDC Configuration for Interactive Flow
-- =============================================================================
-- Use this when you want to trigger the interactive OIDC flow
-- Replace with your Okta/OIDC provider configuration

CREATE SECRET snowflake_oidc_config (
    TYPE snowflake,
    account 'YOUR_ACCOUNT',
    database 'YOUR_DATABASE',
    warehouse 'YOUR_WAREHOUSE',
    role 'YOUR_ROLE',
    oidc_client_id 'YOUR_CLIENT_ID',                   -- From your OIDC app
    oidc_issuer_url 'YOUR_ISSUER_URL',                 -- e.g., 'https://your-domain.okta.com/oauth2/your-auth-server'
    oidc_redirect_uri 'http://localhost:8080/callback', -- Must match your OIDC app config
    oidc_scope 'openid profile email'                  -- OIDC scopes
);

-- Attach using configuration (will trigger interactive flow)
-- ATTACH '' AS snowflake_config_db (TYPE snowflake, SECRET snowflake_oidc_config, ACCESS_MODE READ_ONLY);

-- =============================================================================
-- Template 3: OIDC with Client Secret (Confidential Client)
-- =============================================================================
-- Use this for confidential OIDC clients that require a client secret

CREATE SECRET snowflake_oidc_confidential (
    TYPE snowflake,
    account 'YOUR_ACCOUNT',
    database 'YOUR_DATABASE',
    warehouse 'YOUR_WAREHOUSE',
    role 'YOUR_ROLE',
    oidc_client_id 'YOUR_CLIENT_ID',
    oidc_issuer_url 'YOUR_ISSUER_URL',
    oidc_redirect_uri 'http://localhost:8080/callback',
    oidc_scope 'openid profile email',
    oidc_token 'YOUR_JWT_ACCESS_TOKEN_HERE'            -- Pre-obtained token
);

-- =============================================================================
-- Template 4: Multiple Environments
-- =============================================================================
-- Create secrets for different environments (dev, staging, prod)

-- Development environment
CREATE SECRET snowflake_dev (
    TYPE snowflake,
    account 'DEV_ACCOUNT',
    database 'DEV_DATABASE',
    warehouse 'DEV_WAREHOUSE',
    role 'DEV_ROLE',
    oidc_token 'DEV_JWT_TOKEN'
);

-- Production environment
CREATE SECRET snowflake_prod (
    TYPE snowflake,
    account 'PROD_ACCOUNT',
    database 'PROD_DATABASE',
    warehouse 'PROD_WAREHOUSE',
    role 'PROD_ROLE',
    oidc_token 'PROD_JWT_TOKEN'
);

-- =============================================================================
-- Usage Examples
-- =============================================================================

-- Attach development environment
-- ATTACH '' AS dev_db (TYPE snowflake, SECRET snowflake_dev, ACCESS_MODE READ_ONLY);

-- Attach production environment
-- ATTACH '' AS prod_db (TYPE snowflake, SECRET snowflake_prod, ACCESS_MODE READ_ONLY);

-- =============================================================================
-- Secret Management
-- =============================================================================

-- List all secrets
SELECT name, type FROM duckdb_secrets();

-- Drop a secret
-- DROP SECRET snowflake_oidc_token;

-- =============================================================================
-- Configuration Notes
-- =============================================================================
--
-- 1. Account Format: Use the full account identifier (e.g., 'FLWILYJ-FL43292')
-- 2. Database: Must be accessible with your role and warehouse
-- 3. Warehouse: Must be running or auto-resume enabled
-- 4. Role: Must have necessary permissions for the database
-- 5. OIDC Token: Must be a valid JWT with appropriate scopes
-- 6. Client ID: From your OIDC provider (Okta, Auth0, etc.)
-- 7. Issuer URL: Your OIDC provider's issuer endpoint
-- 8. Redirect URI: Must match exactly what's configured in your OIDC app
-- 9. Scope: Standard OIDC scopes (openid, profile, email, etc.)
--
-- =============================================================================
