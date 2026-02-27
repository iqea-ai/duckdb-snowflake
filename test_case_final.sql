LOAD snowflake;
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
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test all case variations (should all work)
SELECT '✅ Test 1: lowercase' AS result;
SELECT c_custkey FROM sf_test.tpch_sf1.customer LIMIT 1;

SELECT '✅ Test 2: UPPERCASE' AS result;
SELECT c_custkey FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 1;

SELECT '✅ Test 3: Mixed case' AS result;
SELECT c_custkey FROM sf_test.Tpch_Sf1.Customer LIMIT 1;

SELECT '✅ Test 4: Column names preserved (UPPERCASE)' AS result;
DESCRIBE sf_test.tpch_sf1.customer;

DETACH sf_test;
