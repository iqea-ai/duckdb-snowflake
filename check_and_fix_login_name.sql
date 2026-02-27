-- ============================================================================
-- Check and Fix LOGIN_NAME for SAML Users
-- ============================================================================
-- The browser authentication worked, but Snowflake can't match the user.
-- This means LOGIN_NAME doesn't match what Auth0 sends in the SAML assertion.
-- Auth0 typically sends lowercase email addresses.
-- ============================================================================

USE ROLE ACCOUNTADMIN;

-- ============================================================================
-- Step 1: Check Current LOGIN_NAME Values
-- ============================================================================
SELECT 
    name AS user_name,
    login_name,
    CASE 
        WHEN LOWER(login_name) != login_name THEN '⚠️ UPPERCASE - Needs fix'
        WHEN login_name != LOWER(name) THEN '⚠️ Different from lowercase name'
        ELSE '✅ OK'
    END AS status_check
FROM TABLE(INFORMATION_SCHEMA.USERS())
WHERE name LIKE '%@narveetech.com'
ORDER BY name;

-- ============================================================================
-- Step 2: Fix LOGIN_NAME to Lowercase (Auth0 sends lowercase)
-- ============================================================================
-- Auth0 SAML assertions send the email in lowercase format
-- LOGIN_NAME must match EXACTLY (case-sensitive!)

ALTER USER "sabida@narveetech.com" 
  SET LOGIN_NAME = 'sabida@narveetech.com';  -- Ensure lowercase

ALTER USER "anand@narveetech.com" 
  SET LOGIN_NAME = 'anand@narveetech.com';  -- Ensure lowercase

ALTER USER "vamshi@narveetech.com" 
  SET LOGIN_NAME = 'vamshi@narveetech.com';  -- Ensure lowercase

-- ============================================================================
-- Step 3: Verify Changes
-- ============================================================================
SELECT 
    name AS user_name,
    login_name,
    '✅ Fixed - Ready for SAML' AS status
FROM TABLE(INFORMATION_SCHEMA.USERS())
WHERE name LIKE '%@narveetech.com'
ORDER BY name;

-- ============================================================================
-- Step 4: Ensure Other SAML Requirements
-- ============================================================================
-- MUST_CHANGE_PASSWORD must be FALSE for SAML users
ALTER USER "sabida@narveetech.com" SET MUST_CHANGE_PASSWORD = FALSE;
ALTER USER "anand@narveetech.com" SET MUST_CHANGE_PASSWORD = FALSE;
ALTER USER "vamshi@narveetech.com" SET MUST_CHANGE_PASSWORD = FALSE;

-- Verify users are not disabled
SELECT name, disabled, must_change_password
FROM TABLE(INFORMATION_SCHEMA.USERS())
WHERE name LIKE '%@narveetech.com';










