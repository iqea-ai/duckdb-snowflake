-- Case Sensitivity Test - Run this in DuckDB CLI
-- Usage: ./build/debug/duckdb < test_case_cli.sql

LOAD snowflake;

-- Create secret (drop first if exists)
DROP SECRET IF EXISTS test_p8;
CREATE PERSISTENT SECRET test_p8 (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Attach Snowflake database
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test 1: Lowercase (should work)
SELECT '✅ Test 1: lowercase schema/table' AS test;
SELECT c_custkey, c_name FROM sf_test.tpch_sf1.customer LIMIT 3;

-- Test 2: UPPERCASE (should work)
SELECT '✅ Test 2: UPPERCASE schema/table' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 3: Mixed case (should work)
SELECT '✅ Test 3: Mixed case schema/table' AS test;
SELECT c_custkey, c_name FROM sf_test.Tpch_Sf1.Customer LIMIT 3;

-- Test 4: Column case variations
SELECT '✅ Test 4: Lowercase columns' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT '✅ Test 5: UPPERCASE columns' AS test;
SELECT C_CUSTKEY, C_NAME FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT '✅ Test 6: Mixed case columns' AS test;
SELECT C_CustKey, c_Name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 7: Verify column names are preserved (should show UPPERCASE)
SELECT '✅ Test 7: DESCRIBE (should show UPPERCASE column names)' AS test;
DESCRIBE sf_test.tpch_sf1.customer;

-- Test 8: Different table with case variations
SELECT '✅ Test 8: Different table (ORDERS) with lowercase' AS test;
SELECT o_orderkey, o_orderstatus FROM sf_test.tpch_sf1.orders LIMIT 3;

SELECT '✅ Test 9: Different table (ORDERS) with UPPERCASE' AS test;
SELECT o_orderkey, o_orderstatus FROM sf_test.TPCH_SF1.ORDERS LIMIT 3;

-- Cleanup
DETACH sf_test;
DROP SECRET test_p8;

SELECT '🎉 All tests completed!' AS result;












