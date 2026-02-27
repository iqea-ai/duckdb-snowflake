-- Troubleshooting SAML Metadata Function
-- Run these queries one by one to diagnose the issue

-- Step 1: Check if the integration exists
SHOW INTEGRATIONS;

-- Look for 'auth0_saml' in the results
-- If you see it, note the exact name (it might be different)

-- Step 2: Check integration details
DESCRIBE INTEGRATION auth0_saml;

-- Step 3: Try alternative function names (some Snowflake versions use different names)
-- Try this one:
SELECT SYSTEM$GET_SAML_IDP_METADATA('auth0_saml');

-- Or try:
SELECT SYSTEM$SHOW_SAML_METADATA('auth0_saml');

-- Step 4: Check Snowflake version
SELECT CURRENT_VERSION();

-- Step 5: Alternative - Get metadata URL from integration properties
-- After running DESCRIBE, look for a property that shows the metadata URL

-- Step 6: Manual construction (if function doesn't exist)
-- The metadata URL is typically:
-- https://YOUR_ACCOUNT.snowflakecomputing.com/fed/saml/metadata?id=INTEGRATION_ID
-- You can try constructing it manually if you know the integration ID










