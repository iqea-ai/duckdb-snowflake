LOAD snowflake;

-- Use existing secret (or create if needed)
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test 1: Uppercase
SELECT 'Test 1: Uppercase' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 2: Lowercase  
SELECT 'Test 2: Lowercase' AS test;
SELECT c_custkey, c_name FROM sf_test.tpch_sf1.customer LIMIT 3;

-- Test 3: Mixed case
SELECT 'Test 3: Mixed case' AS test;
SELECT c_custkey, c_name FROM sf_test.Tpch_Sf1.Customer LIMIT 3;

-- Test 4: Column case variations
SELECT 'Test 4: Uppercase columns' AS test;
SELECT C_CUSTKEY, C_NAME FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

SELECT 'Test 5: Lowercase columns' AS test;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Verify column names preserved
SELECT 'Test 6: DESCRIBE' AS test;
DESCRIBE sf_test.tpch_sf1.customer;

DETACH sf_test;
