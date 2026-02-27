-- Snowflake SQL Script: Setup SAML Security Integration
-- Run this script as ACCOUNTADMIN role
-- Replace placeholders with your actual values

USE ROLE ACCOUNTADMIN;

-- ============================================================================
-- SAML Integration for Auth0
-- ============================================================================

-- Step 1: Create SAML Security Integration
-- Note: You'll need to update SAML2_X509_CERT after downloading from Auth0
CREATE OR REPLACE SECURITY INTEGRATION auth0_saml
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'urn:YOUR_AUTH0_DOMAIN'  -- e.g., 'urn:dev-abc123.us.auth0.com'
  SAML2_SSO_URL = 'https://YOUR_AUTH0_DOMAIN/samlp/YOUR_CLIENT_ID'
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = 'YOUR_AUTH0_SIGNING_CERTIFICATE'  -- Replace with actual certificate from Auth0
  SAML2_ENABLE_SP_INITIATED = TRUE;

-- Grant usage to appropriate role
GRANT USAGE ON INTEGRATION auth0_saml TO ROLE PUBLIC;

-- Step 2: Get Snowflake SAML Metadata URL
-- This URL contains the ACS URL and other metadata needed for IdP configuration
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('auth0_saml');

-- Copy the URL from the result and use it to configure Auth0 SAML application

-- Step 3: After configuring Auth0, update the certificate and SSO URL if needed
-- ALTER SECURITY INTEGRATION auth0_saml SET
--   SAML2_X509_CERT = '-----BEGIN CERTIFICATE-----
-- YOUR_CERTIFICATE_CONTENT_HERE
-- -----END CERTIFICATE-----',
--   SAML2_SSO_URL = 'https://YOUR_AUTH0_DOMAIN/samlp/YOUR_CLIENT_ID';

-- Step 4: Create Snowflake User
-- The LOGIN_NAME should match the email/identifier sent in SAML assertion
CREATE USER IF NOT EXISTS "user@company.com"
  LOGIN_NAME = 'user@company.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "user@company.com";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;

-- Verify user
SHOW USERS LIKE 'user@company.com';

-- ============================================================================
-- SAML Integration for Okta (Alternative)
-- ============================================================================

-- Uncomment and modify for Okta SAML setup:

/*
CREATE OR REPLACE SECURITY INTEGRATION okta_saml
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'http://www.okta.com/YOUR_OKTA_APP_ID'
  SAML2_SSO_URL = 'https://yourcompany.okta.com/app/snowflake/YOUR_APP_ID/sso/saml'
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = '-----BEGIN CERTIFICATE-----
YOUR_OKTA_CERTIFICATE_CONTENT_HERE
-----END CERTIFICATE-----'
  SAML2_ENABLE_SP_INITIATED = TRUE;

GRANT USAGE ON INTEGRATION okta_saml TO ROLE PUBLIC;

-- Get Snowflake metadata
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('okta_saml');

CREATE USER IF NOT EXISTS "user@company.com"
  LOGIN_NAME = 'user@company.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "user@company.com";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
*/

-- ============================================================================
-- Verification Queries
-- ============================================================================

-- List all security integrations
SHOW INTEGRATIONS;

-- Check SAML integration details
SELECT 
    name,
    type,
    category,
    enabled,
    created_on
FROM TABLE(RESULT_SCAN(LAST_QUERY_ID()))
WHERE type = 'SAML2';

-- Describe integration to see all settings
DESCRIBE INTEGRATION auth0_saml;

-- Check users
SHOW USERS;

-- ============================================================================
-- Troubleshooting Queries
-- ============================================================================

-- Check if integration is enabled
SELECT 
    name,
    enabled,
    type
FROM TABLE(INFORMATION_SCHEMA.SECURITY_INTEGRATIONS)
WHERE name = 'AUTH0_SAML';

-- View integration properties
SELECT 
    property,
    property_type,
    property_value
FROM TABLE(INFORMATION_SCHEMA.SECURITY_INTEGRATION_OPTIONS)
WHERE integration_name = 'AUTH0_SAML';

-- ============================================================================
-- Cleanup (if needed)
-- ============================================================================

-- To remove SAML integration:
-- DROP SECURITY INTEGRATION IF EXISTS auth0_saml;

-- To remove user:
-- DROP USER IF EXISTS "user@company.com";










