-- ============================================================================
-- Verify SAML Setup for Users with UI_READONLY_ROLE
-- ============================================================================
-- This script verifies that SAML authentication is properly configured
-- for users with the UI_READONLY_ROLE role
-- ============================================================================

USE ROLE ACCOUNTADMIN;

-- ============================================================================
-- Step 1: Verify auth0_saml Integration
-- ============================================================================
SELECT '=== SAML Integration Status ===' AS status;
DESCRIBE INTEGRATION auth0_saml;

-- Check if integration is enabled
SELECT 
    property,
    property_value,
    property_default
FROM TABLE(FLATTEN(INPUT => PARSE_JSON(
    SYSTEM$SHOW_INTEGRATION('auth0_saml')
))) 
WHERE property = 'ENABLED';

-- ============================================================================
-- Step 2: Verify Integration Grants
-- ============================================================================
SELECT '=== Integration Grants ===' AS status;
SHOW GRANTS ON INTEGRATION auth0_saml;

-- ============================================================================
-- Step 3: List All Users with UI_READONLY_ROLE
-- ============================================================================
SELECT '=== Users with UI_READONLY_ROLE ===' AS status;
SHOW GRANTS TO ROLE UI_READONLY_ROLE;

-- ============================================================================
-- Step 4: Verify User Configuration for SAML
-- ============================================================================
-- Check specific users (replace with actual email addresses)
-- This shows LOGIN_NAME, MUST_CHANGE_PASSWORD, and roles

SELECT '=== User Configuration ===' AS status;
-- Replace with your actual user emails
SHOW USERS LIKE '%@company.com';

-- For a specific user:
-- DESCRIBE USER "user@company.com";
-- SHOW GRANTS TO USER "user@company.com";

-- ============================================================================
-- Step 5: Check for Common SAML Issues
-- ============================================================================
SELECT '=== SAML Configuration Checklist ===' AS status;

-- Check if integration is enabled
SELECT 
    CASE 
        WHEN property_value = 'true' THEN '✓ Integration is ENABLED'
        ELSE '✗ Integration is DISABLED'
    END AS integration_status
FROM TABLE(FLATTEN(INPUT => PARSE_JSON(
    SYSTEM$SHOW_INTEGRATION('auth0_saml')
))) 
WHERE property = 'ENABLED';

-- Check if PUBLIC role has integration access
SELECT 
    CASE 
        WHEN COUNT(*) > 0 THEN '✓ PUBLIC role has integration access'
        ELSE '✗ PUBLIC role missing integration access'
    END AS public_access
FROM TABLE(FLATTEN(INPUT => PARSE_JSON(
    SYSTEM$SHOW_GRANTS('INTEGRATION', 'auth0_saml')
)))
WHERE granted_to = 'ROLE' AND granted_to_name = 'PUBLIC';

-- ============================================================================
-- Step 6: Sample Query to Test SAML (for DuckDB)
-- ============================================================================
-- After verifying setup, users can test SAML authentication in DuckDB:
/*
CREATE SECRET my_saml_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    AUTH_TYPE 'ext_browser'
);

ATTACH '' AS sf (TYPE snowflake, SECRET my_saml_secret, READ_ONLY);
SELECT * FROM sf.information_schema.tables LIMIT 5;
*/










