LOAD snowflake;

-- Create secret (if it doesn't exist, you may need to create it manually first)
-- First, try to use existing secret, or create a new one
CREATE PERSISTENT SECRET IF NOT EXISTS test_p8 (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Attach Snowflake
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test 1: Uppercase schema/table
SELECT 'Test 1: Uppercase schema/table' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 2: Lowercase schema/table
SELECT 'Test 2: Lowercase schema/table' AS test;
SELECT c_custkey, c_name FROM sf_test.tpch_sf1.customer LIMIT 3;

-- Test 3: Mixed case schema/table
SELECT 'Test 3: Mixed case schema/table' AS test;
SELECT c_custkey, c_name FROM sf_test.Tpch_Sf1.Customer LIMIT 3;

-- Test 4: Column case variations
SELECT 'Test 4: Uppercase columns' AS test;
SELECT C_CUSTKEY, C_NAME FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT 'Test 5: Lowercase columns' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT 'Test 6: Mixed case columns' AS test;
SELECT C_CustKey, c_Name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Verify column names are preserved (should show UPPERCASE)
SELECT 'Test 7: DESCRIBE (should show UPPERCASE)' AS test;
DESCRIBE sf_test.tpch_sf1.customer;

DETACH sf_test;
