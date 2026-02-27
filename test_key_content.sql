-- Test Key Content Reading and Validation
-- This tests that the actual key content is being read and used correctly

LOAD snowflake;

-- Test 1: Verify key content is read from file (already tested, but let's confirm)
DROP SECRET IF EXISTS test_key_file;
CREATE PERSISTENT SECRET test_key_file (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_test1 (TYPE snowflake, SECRET test_key_file, READ_ONLY, ENABLE_PUSHDOWN false);
SELECT '✅ Test 1: Key content from file works' AS test;
SELECT c_custkey FROM sf_test1.tpch_sf1.customer LIMIT 1;
DETACH sf_test1;
DROP SECRET test_key_file;

-- Test 2: Test direct key content (inline PEM, not file path)
-- First, read the key content
-- Note: This would require reading the file content and embedding it
-- For now, we'll skip this as it's complex to embed in SQL

-- Test 3: Test encrypted key with passphrase (if you have one)
-- DROP SECRET IF EXISTS test_encrypted_key;
-- CREATE PERSISTENT SECRET test_encrypted_key (
--     TYPE snowflake,
--     ACCOUNT 'NSKSXKF-CM91407',
--     USER 'test_keypair_user',
--     AUTH_TYPE 'key_pair',
--     PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key_encrypted.p8',
--     PRIVATE_KEY_PASSPHRASE 'your_passphrase',
--     DATABASE 'SNOWFLAKE_SAMPLE_DATA',
--     WAREHOUSE 'COMPUTE_WH'
-- );
-- ATTACH '' AS sf_test2 (TYPE snowflake, SECRET test_encrypted_key, READ_ONLY);
-- SELECT '✅ Test 2: Encrypted key with passphrase works' AS test;
-- SELECT c_custkey FROM sf_test2.tpch_sf1.customer LIMIT 1;
-- DETACH sf_test2;
-- DROP SECRET test_encrypted_key;

SELECT '✅ Key content tests completed' AS result;
