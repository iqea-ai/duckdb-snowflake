-- ============================================================================
-- Template: Create New User with UI Read-Only Role
-- ============================================================================
-- This template creates a new Snowflake user and assigns the UI_READONLY_ROLE role
-- with appropriate grants for SAML authentication and read-only access.
-- 
-- CUSTOMIZATION REQUIRED:
--   1. Replace <USER_EMAIL> with the actual user email address
--   2. Replace <LOGIN_NAME> with the SAML login name (usually lowercase email)
--   3. Verify DEFAULT_WAREHOUSE, DEFAULT_NAMESPACE match your environment
--   4. Role name: UI_READONLY_ROLE (adjust if different)
-- ============================================================================

USE ROLE ACCOUNTADMIN;

-- Verify current role
SELECT CURRENT_ROLE() AS current_role;

-- ============================================================================
-- Step 1: Create User (SAML-enabled)
-- ============================================================================
-- For SAML users, LOGIN_NAME should match the SAML assertion (usually lowercase)
-- MUST_CHANGE_PASSWORD = FALSE because SAML users authenticate via IdP

CREATE USER IF NOT EXISTS "<USER_EMAIL>"
  LOGIN_NAME = '<LOGIN_NAME>'
  DEFAULT_ROLE = UI_READONLY_ROLE
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'SNOWFLAKE_SAMPLE_DATA.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

-- ============================================================================
-- Step 2: Assign Roles
-- ============================================================================
-- Grant PUBLIC role (required for all users)
GRANT ROLE PUBLIC TO USER "<USER_EMAIL>";

-- Grant UI read-only role
GRANT ROLE UI_READONLY_ROLE TO USER "<USER_EMAIL>";

-- Set default role to UI_READONLY_ROLE
ALTER USER "<USER_EMAIL>" SET DEFAULT_ROLE = UI_READONLY_ROLE;

-- ============================================================================
-- Step 3: Grant Database Access (if role doesn't already have it)
-- ============================================================================
-- Note: If ui_dev_readonly role already has these grants, these may be redundant
-- but they won't cause errors if run multiple times.

-- For imported/shared databases (like SNOWFLAKE_SAMPLE_DATA)
GRANT IMPORTED PRIVILEGES ON DATABASE SNOWFLAKE_SAMPLE_DATA TO ROLE PUBLIC;
GRANT IMPORTED PRIVILEGES ON DATABASE SNOWFLAKE_SAMPLE_DATA TO ROLE UI_READONLY_ROLE;

-- ============================================================================
-- Step 4: Grant Warehouse Access (if role doesn't already have it)
-- ============================================================================
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE UI_READONLY_ROLE;

-- ============================================================================
-- Step 4b: Grant SAML Integration Access
-- ============================================================================
-- Ensure user can use the auth0_saml integration
GRANT USAGE ON INTEGRATION auth0_saml TO ROLE PUBLIC;
GRANT USAGE ON INTEGRATION auth0_saml TO ROLE UI_READONLY_ROLE;

-- ============================================================================
-- Step 5: Verify User Configuration
-- ============================================================================
DESCRIBE USER "<USER_EMAIL>";
SHOW USERS LIKE '<USER_EMAIL>';

-- Verify role assignments
SHOW GRANTS TO USER "<USER_EMAIL>";

-- ============================================================================
-- Step 6: Ensure LOGIN_NAME matches SAML assertion (Important for SAML!)
-- ============================================================================
-- SAML assertions typically send lowercase email addresses
-- If your SAML IdP sends lowercase, ensure LOGIN_NAME is lowercase:
ALTER USER "<USER_EMAIL>" 
  SET LOGIN_NAME = '<LOGIN_NAME>';

-- Verify LOGIN_NAME is correct
SHOW USERS LIKE '<USER_EMAIL>';

-- ============================================================================
-- EXAMPLE: Creating user vchikkam.work@gmail.com
-- ============================================================================
/*
USE ROLE ACCOUNTADMIN;

CREATE USER IF NOT EXISTS "vchikkam.work@gmail.com"
  LOGIN_NAME = 'vchikkam.work@gmail.com'
  DEFAULT_ROLE = UI_READONLY_ROLE
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'SNOWFLAKE_SAMPLE_DATA.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "vchikkam.work@gmail.com";
GRANT ROLE UI_READONLY_ROLE TO USER "vchikkam.work@gmail.com";

ALTER USER "vchikkam.work@gmail.com" SET DEFAULT_ROLE = UI_READONLY_ROLE;

GRANT IMPORTED PRIVILEGES ON DATABASE SNOWFLAKE_SAMPLE_DATA TO ROLE PUBLIC;
GRANT IMPORTED PRIVILEGES ON DATABASE SNOWFLAKE_SAMPLE_DATA TO ROLE UI_READONLY_ROLE;

GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE UI_READONLY_ROLE;

GRANT USAGE ON INTEGRATION auth0_saml TO ROLE PUBLIC;
GRANT USAGE ON INTEGRATION auth0_saml TO ROLE UI_READONLY_ROLE;

DESCRIBE USER "vchikkam.work@gmail.com";
SHOW USERS LIKE 'vchikkam.work@gmail.com';
SHOW GRANTS TO USER "vchikkam.work@gmail.com";

ALTER USER "vchikkam.work@gmail.com" 
  SET LOGIN_NAME = 'vchikkam.work@gmail.com';
*/

-- ============================================================================
-- NOTES:
-- ============================================================================
-- 1. LOGIN_NAME must match the SAML assertion NameID exactly (case-sensitive)
-- 2. For Auth0, the NameID is typically the email address in lowercase
-- 3. If SAML authentication fails, check LOGIN_NAME matches the assertion
-- 4. The UI_READONLY_ROLE role must exist before assigning it to users
-- 5. The auth0_saml integration must exist and be enabled
-- 6. For non-SAML users, you may want to set PASSWORD and MUST_CHANGE_PASSWORD = TRUE
-- 7. For existing users, use enable_saml_for_existing_users.sql instead










