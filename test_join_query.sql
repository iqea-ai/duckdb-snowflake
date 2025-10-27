-- Test join query with pushdown disabled and enabled

LOAD snowflake;

CREATE SECRET test_secret (
    TYPE snowflake,
    ACCOUNT 'FLWILYJ-FL43292',
    USER 'venkata@iqea.ai',
    PASSWORD 'DuckDBSnowflake2025',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA'
);

-- Test 1: Pushdown DISABLED (default)
ATTACH '' AS snow_disabled (TYPE snowflake, SECRET test_secret, READ_ONLY);

SELECT 'Test 1: Pushdown DISABLED' as test_case;
SELECT c.C_CUSTKEY, c.C_NAME, n.N_NAME
FROM snow_disabled.tpch_sf1.customer c
JOIN snow_disabled.tpch_sf1.nation n ON c.C_NATIONKEY = n.N_NATIONKEY
WHERE n.N_NAME = 'GERMANY' AND c.C_CUSTKEY <= 100
ORDER BY c.C_CUSTKEY
LIMIT 10;

DETACH snow_disabled;

-- Test 2: Pushdown ENABLED
ATTACH '' AS snow_enabled (TYPE snowflake, SECRET test_secret, READ_ONLY, enable_pushdown true);

SELECT 'Test 2: Pushdown ENABLED' as test_case;
SELECT c.C_CUSTKEY, c.C_NAME, n.N_NAME
FROM snow_enabled.tpch_sf1.customer c
JOIN snow_enabled.tpch_sf1.nation n ON c.C_NATIONKEY = n.N_NATIONKEY
WHERE n.N_NAME = 'GERMANY' AND c.C_CUSTKEY <= 100
ORDER BY c.C_CUSTKEY
LIMIT 10;

DETACH snow_enabled;