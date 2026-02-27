-- Comprehensive Key Content Testing
-- Tests: encrypted keys, passphrase, direct key content, error cases

LOAD snowflake;

-- ============================================
-- Test 1: Encrypted Key with Passphrase
-- ============================================
-- First, let's create an encrypted key if it doesn't exist
-- Run this in terminal first:
-- openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out ~/snowflake_keys/rsa_key_encrypted.p8 -passout pass:testpass123
-- Then register the public key in Snowflake

-- Uncomment and test if you have an encrypted key:
-- DROP SECRET IF EXISTS test_encrypted;
-- CREATE PERSISTENT SECRET test_encrypted (
--     TYPE snowflake,
--     ACCOUNT 'NSKSXKF-CM91407',
--     USER 'test_keypair_user',
--     AUTH_TYPE 'key_pair',
--     PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key_encrypted.p8',
--     PRIVATE_KEY_PASSPHRASE 'testpass123',
--     DATABASE 'SNOWFLAKE_SAMPLE_DATA',
--     WAREHOUSE 'COMPUTE_WH'
-- );
-- ATTACH '' AS sf_encrypted (TYPE snowflake, SECRET test_encrypted, READ_ONLY, ENABLE_PUSHDOWN false);
-- SELECT '✅ Test 1: Encrypted key with passphrase' AS test;
-- SELECT c_custkey FROM sf_encrypted.tpch_sf1.customer LIMIT 1;
-- DETACH sf_encrypted;
-- DROP SECRET test_encrypted;

-- ============================================
-- Test 2: Direct Key Content (Inline PEM)
-- ============================================
-- Test passing key content directly instead of file path
-- This tests the else branch in snowflake_client.cpp (line 252-257)

-- First, read the key content (we'll do this via a helper script)
-- For now, let's test with a minimal example
-- Note: This requires the full PEM content including BEGIN/END markers

-- Uncomment to test (requires reading key file first):
-- DROP SECRET IF EXISTS test_inline_key;
-- CREATE PERSISTENT SECRET test_inline_key (
--     TYPE snowflake,
--     ACCOUNT 'NSKSXKF-CM91407',
--     USER 'test_keypair_user',
--     AUTH_TYPE 'key_pair',
--     PRIVATE_KEY '-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC...\n-----END PRIVATE KEY-----',
--     DATABASE 'SNOWFLAKE_SAMPLE_DATA',
--     WAREHOUSE 'COMPUTE_WH'
-- );
-- ATTACH '' AS sf_inline (TYPE snowflake, SECRET test_inline_key, READ_ONLY);
-- SELECT '✅ Test 2: Direct key content (inline PEM)' AS test;
-- SELECT c_custkey FROM sf_inline.tpch_sf1.customer LIMIT 1;
-- DETACH sf_inline;
-- DROP SECRET test_inline_key;

-- ============================================
-- Test 3: Wrong Key Rejection
-- ============================================
-- Test that using a wrong key fails authentication
-- Create a dummy key file that doesn't match the public key in Snowflake

-- DROP SECRET IF EXISTS test_wrong_key;
-- CREATE PERSISTENT SECRET test_wrong_key (
--     TYPE snowflake,
--     ACCOUNT 'NSKSXKF-CM91407',
--     USER 'test_keypair_user',
--     AUTH_TYPE 'key_pair',
--     PRIVATE_KEY '/path/to/wrong_key.p8',  -- Key that doesn't match public key
--     DATABASE 'SNOWFLAKE_SAMPLE_DATA',
--     WAREHOUSE 'COMPUTE_WH'
-- );
-- ATTACH '' AS sf_wrong (TYPE snowflake, SECRET test_wrong_key, READ_ONLY);
-- -- This should fail with authentication error
-- SELECT c_custkey FROM sf_wrong.tpch_sf1.customer LIMIT 1;
-- DETACH sf_wrong;
-- DROP SECRET test_wrong_key;

-- ============================================
-- Test 4: Wrong Passphrase Rejection
-- ============================================
-- Test that wrong passphrase fails for encrypted keys

-- DROP SECRET IF EXISTS test_wrong_passphrase;
-- CREATE PERSISTENT SECRET test_wrong_passphrase (
--     TYPE snowflake,
--     ACCOUNT 'NSKSXKF-CM91407',
--     USER 'test_keypair_user',
--     AUTH_TYPE 'key_pair',
--     PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key_encrypted.p8',
--     PRIVATE_KEY_PASSPHRASE 'wrong_password',  -- Wrong passphrase
--     DATABASE 'SNOWFLAKE_SAMPLE_DATA',
--     WAREHOUSE 'COMPUTE_WH'
-- );
-- ATTACH '' AS sf_wrong_pass (TYPE snowflake, SECRET test_wrong_passphrase, READ_ONLY);
-- -- This should fail with authentication error
-- SELECT c_custkey FROM sf_wrong_pass.tpch_sf1.customer LIMIT 1;
-- DETACH sf_wrong_pass;
-- DROP SECRET test_wrong_passphrase;

-- ============================================
-- Test 5: Verify Key Content is Actually Read
-- ============================================
-- This test confirms the key file content is being read correctly
-- by testing with the working key

DROP SECRET IF EXISTS test_key_verification;
CREATE PERSISTENT SECRET test_key_verification (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_verify (TYPE snowflake, SECRET test_key_verification, READ_ONLY, ENABLE_PUSHDOWN false);
SELECT '✅ Test 5: Key content verification (file read works)' AS test;
SELECT COUNT(*) as customer_count FROM sf_verify.tpch_sf1.customer;
SELECT 'Key content successfully read and used for authentication' AS verification;
DETACH sf_verify;
DROP SECRET test_key_verification;

SELECT '✅ Key content tests framework ready' AS result;
SELECT 'Note: Uncomment tests 1-4 after setting up encrypted keys' AS note;












