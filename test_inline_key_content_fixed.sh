#!/bin/bash
# Script to test inline key content (direct PEM, not file path)
# This tests the else branch in snowflake_client.cpp where key content is passed directly

KEY_FILE="$HOME/snowflake_keys/rsa_key.p8"

# Read key content and properly escape for SQL
KEY_CONTENT=$(cat "$KEY_FILE")

# Create SQL file with proper escaping
cat > /tmp/test_inline_key.sql << 'SQL'
LOAD snowflake;

-- Test direct key content (not file path)
-- This tests that FileExists() returns false and key content is used directly
DROP SECRET IF EXISTS test_inline;
CREATE PERSISTENT SECRET test_inline (
    TYPE snowflake,
    ACCOUNT 'NSKSXKF-CM91407',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY 'SQL

# Append the actual key content (properly escaped)
echo "$KEY_CONTENT" | sed "s/'/''/g" >> /tmp/test_inline_key.sql

cat >> /tmp/test_inline_key.sql << 'SQL'
',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_inline (TYPE snowflake, SECRET test_inline, READ_ONLY, ENABLE_PUSHDOWN false);
SELECT '✅ Test: Direct key content (inline PEM)' AS test;
SELECT c_custkey FROM sf_inline.tpch_sf1.customer LIMIT 3;
DETACH sf_inline;
DROP SECRET test_inline;
SQL

echo "✅ Generated test file: /tmp/test_inline_key.sql"
echo "Run: ./build/debug/duckdb < /tmp/test_inline_key.sql"
