-- ============================================================================
-- Check Current User and SAML Configuration
-- ============================================================================
-- Verify which user you're logged in as and check SAML user configuration
-- ============================================================================

USE ROLE ACCOUNTADMIN;

-- ============================================================================
-- Step 1: Check Current Session User
-- ============================================================================
SELECT CURRENT_USER() AS current_user;
SELECT CURRENT_ROLE() AS current_role;

-- ============================================================================
-- Step 2: Check All Users with @narveetech.com
-- ============================================================================
SELECT 
    name AS user_name,
    login_name,
    email,
    default_role,
    default_warehouse,
    must_change_password,
    disabled
FROM TABLE(INFORMATION_SCHEMA.USERS())
WHERE name LIKE '%@narveetech.com' OR email LIKE '%@narveetech.com'
ORDER BY name;

-- ============================================================================
-- Step 3: Check Your Specific User (if you know your email)
-- ============================================================================
-- Replace with your actual email address
-- DESCRIBE USER "your.email@narveetech.com";
-- SHOW GRANTS TO USER "your.email@narveetech.com";

-- ============================================================================
-- Step 4: Verify SAML Integration Configuration
-- ============================================================================
DESCRIBE INTEGRATION auth0_saml;

-- Check what Auth0 is configured to send
-- Auth0 Name Identifier Format should be: emailAddress
-- Auth0 Name Identifier should be: {email}

-- ============================================================================
-- Step 5: Check Auth0 Logs (Manual Step)
-- ============================================================================
-- To see what email Auth0 is actually sending:
-- 1. Go to Auth0 Dashboard → Monitoring → Logs
-- 2. Look for SAML responses
-- 3. Check the NameID field in the SAML assertion
-- 4. Ensure LOGIN_NAME in Snowflake matches exactly (case-sensitive)

-- ============================================================================
-- Common Issue: Email Case Mismatch
-- ============================================================================
-- If Auth0 sends: vchikkam.work@gmail.com (lowercase)
-- But Snowflake LOGIN_NAME is: VCHIKKAM.WORK@GMAIL.COM (uppercase)
-- Then authentication will fail!

-- Fix: Set LOGIN_NAME to match exactly what Auth0 sends
-- ALTER USER "your.email@narveetech.com" SET LOGIN_NAME = 'your.email@narveetech.com';










