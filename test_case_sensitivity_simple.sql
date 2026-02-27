-- Test Case Sensitivity: Case-Insensitive Comparison with Case-Preserving Identifiers
-- This tests commit 276f566: Preserving casing for table name identifiers but ignoring it for comparison

LOAD snowflake;

-- Create secret for testing
CREATE PERSISTENT SECRET test_case_sens (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Verify secret was created
SELECT name, type FROM duckdb_secrets() WHERE name = 'test_case_sens';

-- Attach Snowflake database
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_case_sens, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test Case Sensitivity: All these queries should work regardless of case
SELECT '=== Test 1: Uppercase schema/table ===' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT '=== Test 2: Lowercase schema/table ===' AS test;
SELECT c_custkey, c_name FROM sf_test.tpch_sf1.customer LIMIT 3;

SELECT '=== Test 3: Mixed case schema/table ===' AS test;
SELECT c_custkey, c_name FROM sf_test.Tpch_Sf1.Customer LIMIT 3;

-- Test column case variations
SELECT '=== Test 4: Uppercase columns ===' AS test;
SELECT C_CUSTKEY, C_NAME FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT '=== Test 5: Lowercase columns ===' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT '=== Test 6: Mixed case columns ===' AS test;
SELECT C_CustKey, c_Name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Verify column names are preserved (should show UPPERCASE)
SELECT '=== Test 7: DESCRIBE (should show UPPERCASE column names) ===' AS test;
DESCRIBE sf_test.tpch_sf1.customer;

-- Cleanup
DETACH sf_test;
DROP SECRET test_case_sens;
