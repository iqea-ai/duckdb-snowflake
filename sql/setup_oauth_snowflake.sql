-- Snowflake SQL Script: Setup OAuth Security Integration
-- Run this script as ACCOUNTADMIN role
-- Replace placeholders with your actual values

USE ROLE ACCOUNTADMIN;

-- ============================================================================
-- OAuth Integration for Auth0
-- ============================================================================

-- Step 1: Create OAuth Security Integration
CREATE OR REPLACE SECURITY INTEGRATION auth0_oauth_integration
  TYPE = EXTERNAL_OAUTH
  ENABLED = TRUE
  EXTERNAL_OAUTH_TYPE = CUSTOM
  EXTERNAL_OAUTH_ISSUER = 'https://YOUR_AUTH0_DOMAIN/'  -- e.g., 'https://dev-abc123.us.auth0.com/'
  EXTERNAL_OAUTH_JWS_KEYS_URL = 'https://YOUR_AUTH0_DOMAIN/.well-known/jwks.json'
  EXTERNAL_OAUTH_AUDIENCE_LIST = ('https://YOUR_ACCOUNT.snowflakecomputing.com')
  EXTERNAL_OAUTH_TOKEN_USER_MAPPING_CLAIM = 'sub'
  EXTERNAL_OAUTH_SNOWFLAKE_USER_MAPPING_ATTRIBUTE = 'LOGIN_NAME'
  EXTERNAL_OAUTH_ANY_ROLE_MODE = 'ENABLE';

-- Grant usage to appropriate role
GRANT USAGE ON INTEGRATION auth0_oauth_integration TO ROLE PUBLIC;

-- Verify integration
DESCRIBE INTEGRATION auth0_oauth_integration;

-- Step 2: Create Snowflake User (replace YOUR_CLIENT_ID with actual Auth0 Client ID)
-- The LOGIN_NAME must match the 'sub' claim in the OAuth token (format: auth0|CLIENT_ID)
CREATE USER IF NOT EXISTS "auth0|YOUR_CLIENT_ID"
  LOGIN_NAME = 'auth0|YOUR_CLIENT_ID'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "auth0|YOUR_CLIENT_ID";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;

-- Verify user
SHOW USERS LIKE 'auth0|YOUR_CLIENT_ID';

-- ============================================================================
-- OAuth Integration for Okta (Alternative)
-- ============================================================================

-- Uncomment and modify for Okta OAuth setup:

/*
CREATE OR REPLACE SECURITY INTEGRATION okta_oauth_integration
  TYPE = EXTERNAL_OAUTH
  ENABLED = TRUE
  EXTERNAL_OAUTH_TYPE = CUSTOM
  EXTERNAL_OAUTH_ISSUER = 'https://yourcompany.okta.com/oauth2/default'
  EXTERNAL_OAUTH_JWS_KEYS_URL = 'https://yourcompany.okta.com/oauth2/default/v1/keys'
  EXTERNAL_OAUTH_AUDIENCE_LIST = ('https://YOUR_ACCOUNT.snowflakecomputing.com')
  EXTERNAL_OAUTH_TOKEN_USER_MAPPING_CLAIM = 'sub'
  EXTERNAL_OAUTH_SNOWFLAKE_USER_MAPPING_ATTRIBUTE = 'LOGIN_NAME'
  EXTERNAL_OAUTH_ANY_ROLE_MODE = 'ENABLE';

GRANT USAGE ON INTEGRATION okta_oauth_integration TO ROLE PUBLIC;

CREATE USER IF NOT EXISTS "okta_user@yourcompany.com"
  LOGIN_NAME = 'okta_user@yourcompany.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "okta_user@yourcompany.com";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
*/

-- ============================================================================
-- Verification Queries
-- ============================================================================

-- List all security integrations
SHOW INTEGRATIONS;

-- Check OAuth integration details
SELECT 
    name,
    type,
    category,
    enabled,
    created_on
FROM TABLE(RESULT_SCAN(LAST_QUERY_ID()))
WHERE type = 'EXTERNAL_OAUTH';

-- Check users
SHOW USERS;

-- ============================================================================
-- Cleanup (if needed)
-- ============================================================================

-- To remove OAuth integration:
-- DROP SECURITY INTEGRATION IF EXISTS auth0_oauth_integration;

-- To remove user:
-- DROP USER IF EXISTS "auth0|YOUR_CLIENT_ID";










