LOAD snowflake;
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test all case variations
SELECT 'Test 1: lowercase' AS test;
SELECT c_custkey FROM sf_test.tpch_sf1.customer LIMIT 1;

SELECT 'Test 2: UPPERCASE' AS test;
SELECT c_custkey FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 1;

SELECT 'Test 3: Mixed case' AS test;
SELECT c_custkey FROM sf_test.Tpch_Sf1.Customer LIMIT 1;

SELECT 'Test 4: Schema names preserved' AS test;
DESCRIBE sf_test.tpch_sf1.customer;

DETACH sf_test;
