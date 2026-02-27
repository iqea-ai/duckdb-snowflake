#!/bin/bash
# Script to test inline key content (direct PEM, not file path)

KEY_FILE="$HOME/snowflake_keys/rsa_key.p8"
KEY_CONTENT=$(cat "$KEY_FILE")

# Escape single quotes and newlines for SQL
ESCAPED_KEY=$(echo "$KEY_CONTENT" | sed "s/'/''/g" | tr '\n' '\\n')

cat > /tmp/test_inline_key.sql << SQL
LOAD snowflake;

DROP SECRET IF EXISTS test_inline;
CREATE PERSISTENT SECRET test_inline (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '$ESCAPED_KEY',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_inline (TYPE snowflake, SECRET test_inline, READ_ONLY, ENABLE_PUSHDOWN false);
SELECT '✅ Test: Direct key content (inline PEM)' AS test;
SELECT c_custkey FROM sf_inline.tpch_sf1.customer LIMIT 3;
DETACH sf_inline;
DROP SECRET test_inline;
SQL

echo "Generated test file: /tmp/test_inline_key.sql"
echo "Run: ./build/debug/duckdb < /tmp/test_inline_key.sql"
