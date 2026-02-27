-- ============================================================================
-- Troubleshoot SAML Authentication Error
-- ============================================================================
-- Error: SAML response is invalid or matching user is not found
-- This usually means LOGIN_NAME doesn't match Auth0 SAML assertion
-- ============================================================================

USE ROLE ACCOUNTADMIN;

-- ============================================================================
-- Step 1: Check User Configuration
-- ============================================================================
-- Check all three users
SELECT '=== sabida@narveetech.com ===' AS check_user;
DESCRIBE USER "sabida@narveetech.com";
SHOW USERS LIKE 'sabida@narveetech.com';

SELECT '=== anand@narveetech.com ===' AS check_user;
DESCRIBE USER "anand@narveetech.com";
SHOW USERS LIKE 'anand@narveetech.com';

SELECT '=== vamshi@narveetech.com ===' AS check_user;
DESCRIBE USER "vamshi@narveetech.com";
SHOW USERS LIKE 'vamshi@narveetech.com';

-- ============================================================================
-- Step 2: Check LOGIN_NAME (Critical!)
-- ============================================================================
-- LOGIN_NAME must match EXACTLY what Auth0 sends (case-sensitive)
-- Auth0 typically sends lowercase email addresses

SELECT 
    name AS user_name,
    login_name,
    default_role,
    must_change_password,
    disabled
FROM TABLE(INFORMATION_SCHEMA.USERS())
WHERE name LIKE '%@narveetech.com'
ORDER BY name;

-- ============================================================================
-- Step 3: Verify Integration Configuration
-- ============================================================================
DESCRIBE INTEGRATION auth0_saml;

-- Check integration is enabled
SELECT 
    property,
    property_value
FROM TABLE(FLATTEN(INPUT => PARSE_JSON(
    SYSTEM$SHOW_INTEGRATION('auth0_saml')
))) 
WHERE property IN ('ENABLED', 'SAML2_ISSUER', 'SAML2_SSO_URL');

-- ============================================================================
-- Step 4: Check User Roles
-- ============================================================================
SHOW GRANTS TO USER "sabida@narveetech.com";
SHOW GRANTS TO USER "anand@narveetech.com";
SHOW GRANTS TO USER "vamshi@narveetech.com";

-- ============================================================================
-- Common Fixes:
-- ============================================================================
-- If LOGIN_NAME doesn't match Auth0 assertion, update it:
-- ALTER USER "sabida@narveetech.com" SET LOGIN_NAME = 'sabida@narveetech.com';
-- 
-- Make sure it's lowercase (Auth0 typically sends lowercase)
-- Make sure there are no extra spaces or special characters










