LOAD snowflake;
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test lowercase (should work)
SELECT 'Testing lowercase' AS test;
SELECT c_custkey FROM sf_test.tpch_sf1.customer LIMIT 1;

-- Test uppercase (should fail but let's see debug output)
SELECT 'Testing uppercase' AS test;
SELECT c_custkey FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 1;

DETACH sf_test;
