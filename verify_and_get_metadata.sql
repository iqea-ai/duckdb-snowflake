-- Step-by-step verification and metadata retrieval
-- Run these queries one at a time in Snowflake

-- Step 1: Verify you're using ACCOUNTADMIN role
SELECT CURRENT_ROLE();
-- Should return: ACCOUNTADMIN

-- Step 2: Check if the integration exists
SHOW INTEGRATIONS;

-- Look for 'auth0_saml' in the name column
-- If you don't see it, the integration wasn't created successfully

-- Step 3: If integration exists, describe it
DESCRIBE INTEGRATION auth0_saml;

-- Step 4: Try to get metadata URL
-- The function name might vary by Snowflake version
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('auth0_saml');

-- Step 5: Alternative - Check Snowflake version
SELECT CURRENT_VERSION();

-- Step 6: Manual metadata URL construction
-- If the function doesn't work, you can construct the URL manually:
-- Format: https://YOUR_ACCOUNT.snowflakecomputing.com/fed/saml/metadata?id=INTEGRATION_ID
-- 
-- To get the integration ID, run:
SELECT 
    name,
    type,
    created_on
FROM TABLE(RESULT_SCAN(LAST_QUERY_ID()))
WHERE name = 'AUTH0_SAML';

-- Then construct: https://YFEQLOK-BD05926.snowflakecomputing.com/fed/saml/metadata?id=INTEGRATION_ID










