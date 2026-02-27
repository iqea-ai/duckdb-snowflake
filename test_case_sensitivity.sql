-- Test Case Sensitivity: Case-Insensitive Comparison with Case-Preserving Identifiers
-- This tests commit 276f566: Preserving casing for table name identifiers but ignoring it for comparison

LOAD snowflake;

-- Create secret if it doesn't exist (or use existing one)
-- Drop existing secret first to ensure clean state
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

-- Test 1: Schema name case variations (should all work)
-- Snowflake stores schemas as UPPERCASE (TPCH_SF1)
-- But queries should work with any case

ATTACH '' AS sf_test (TYPE snowflake, SECRET test_p8, READ_ONLY, ENABLE_PUSHDOWN false);

-- Test 1a: Query with uppercase schema (matches Snowflake's stored case)
SELECT 'Test 1a: Uppercase schema' AS test_name;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 1b: Query with lowercase schema (should work via case-insensitive lookup)
SELECT 'Test 1b: Lowercase schema' AS test_name;
SELECT c_custkey, c_name FROM sf_test.tpch_sf1.customer LIMIT 3;

-- Test 1c: Query with mixed case schema (should work via case-insensitive lookup)
SELECT 'Test 1c: Mixed case schema' AS test_name;
SELECT c_custkey, c_name FROM sf_test.Tpch_Sf1.Customer LIMIT 3;

-- Test 2: Table name case variations
-- Test 2a: Uppercase table (matches Snowflake's stored case)
SELECT 'Test 2a: Uppercase table' AS test_name;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 2b: Lowercase table
SELECT 'Test 2b: Lowercase table' AS test_name;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.customer LIMIT 3;

-- Test 2c: Mixed case table
SELECT 'Test 2c: Mixed case table' AS test_name;
SELECT c_custkey, c_name FROM sf_test.TPCH_SF1.Customer LIMIT 3;

-- Test 3: Column name case variations
-- Test 3a: Uppercase columns (matches Snowflake's stored case)
SELECT 'Test 3a: Uppercase columns' AS test_name;
SELECT C_CUSTKEY, C_NAME, C_ACCTBAL FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 3b: Lowercase columns (DuckDB handles this internally)
SELECT 'Test 3b: Lowercase columns' AS test_name;
SELECT c_custkey, c_name, c_acctbal FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 3c: Mixed case columns
SELECT 'Test 3c: Mixed case columns' AS test_name;
SELECT C_CustKey, c_Name, C_ACCTBAL FROM sf_test.TPCH_SF1.CUSTOMER LIMIT 3;

-- Test 4: Verify original case is preserved in DESCRIBE/SHOW commands
-- This tests that identifiers are case-preserving
SELECT 'Test 4: Check schema names (should show original UPPERCASE)' AS test_name;
SELECT schema_name FROM information_schema.schemata WHERE catalog_name = 'sf_test' ORDER BY schema_name;

SELECT 'Test 5: Check table names (should show original UPPERCASE)' AS test_name;
SELECT table_name FROM information_schema.tables WHERE table_schema = 'TPCH_SF1' AND table_catalog = 'sf_test' ORDER BY table_name;

SELECT 'Test 6: Check column names (should show original UPPERCASE)' AS test_name;
DESCRIBE sf_test.tpch_sf1.customer;  -- lowercase query, uppercase column names

-- Test 7: Complex query with mixed cases
SELECT 'Test 7: Complex query with mixed cases' AS test_name;
SELECT 
    c_custkey AS customer_id,
    c_name AS customer_name,
    c_acctbal AS account_balance
FROM sf_test.tpch_sf1.customer  -- lowercase schema.table
WHERE C_ACCTBAL > 0  -- uppercase column in WHERE
ORDER BY customer_id
LIMIT 5;

-- Test 8: Verify case-insensitive works with different table
SELECT 'Test 8: Test with different table (ORDERS)' AS test_name;
SELECT o_orderkey, o_orderstatus FROM sf_test.tpch_sf1.orders LIMIT 3;
SELECT o_orderkey, o_orderstatus FROM sf_test.tpch_sf1.ORDERS LIMIT 3;
SELECT o_orderkey, o_orderstatus FROM sf_test.TPCH_SF1.orders LIMIT 3;

DETACH sf_test;

