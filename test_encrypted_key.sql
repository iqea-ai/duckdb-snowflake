-- Test Encrypted Key with Passphrase
LOAD snowflake;

DROP SECRET IF EXISTS test_encrypted;
CREATE PERSISTENT SECRET test_encrypted (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/venkata/snowflake_keys/rsa_key_encrypted.p8',
    PRIVATE_KEY_PASSPHRASE 'testpass123',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_encrypted (TYPE snowflake, SECRET test_encrypted, READ_ONLY, ENABLE_PUSHDOWN false);
SELECT '✅ Test: Encrypted key with passphrase' AS test;
SELECT c_custkey FROM sf_encrypted.tpch_sf1.customer LIMIT 3;
DETACH sf_encrypted;
DROP SECRET test_encrypted;
