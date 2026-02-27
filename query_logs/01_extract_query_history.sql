-- Extract Snowflake Query History using DuckDB Snowflake Extension
-- This script pulls query history from Snowflake for analysis
-- Adjust the time window as needed (default: last 30 days)

-- Load the Snowflake extension
INSTALL snowflake FROM community;
LOAD snowflake;

-- Create Snowflake secret using environment variables
-- Uses environment variables for credentials:
--   SNOWFLAKE_ACCOUNT
--   SNOWFLAKE_USER
--   SNOWFLAKE_PASSWORD
--   SNOWFLAKE_WAREHOUSE (optional, defaults to COMPUTE_WH)
--
-- Set these before running:
-- export SNOWFLAKE_ACCOUNT=your_account
-- export SNOWFLAKE_USER=your_user
-- export SNOWFLAKE_PASSWORD=your_password
-- export SNOWFLAKE_WAREHOUSE=your_warehouse

CREATE OR REPLACE SECRET snowflake_secret (
    TYPE snowflake,
    ACCOUNT getenv('SNOWFLAKE_ACCOUNT'),
    USER getenv('SNOWFLAKE_USER'),
    PASSWORD getenv('SNOWFLAKE_PASSWORD'),
    DATABASE 'SNOWFLAKE',
    WAREHOUSE COALESCE(getenv('SNOWFLAKE_WAREHOUSE'), 'COMPUTE_WH')
);

-- Attach Snowflake database for query history access
ATTACH '' AS sf_account_usage (
    TYPE snowflake,
    SECRET snowflake_secret,
    READ_ONLY
);

CREATE OR REPLACE TABLE query_history AS
SELECT
    -- Query identification
    QUERY_ID,
    QUERY_TEXT,
    QUERY_TYPE,
    QUERY_TAG,

    -- Execution metadata
    USER_NAME,
    ROLE_NAME,
    DATABASE_NAME,
    SCHEMA_NAME,
    WAREHOUSE_NAME,
    WAREHOUSE_SIZE,
    WAREHOUSE_TYPE,

    -- Timing information
    START_TIME,
    END_TIME,
    EXECUTION_STATUS,
    EXECUTION_TIME as EXECUTION_TIME_MS,
    COMPILATION_TIME as COMPILATION_TIME_MS,
    QUEUED_PROVISIONING_TIME as QUEUED_PROVISIONING_TIME_MS,
    QUEUED_REPAIR_TIME as QUEUED_REPAIR_TIME_MS,
    QUEUED_OVERLOAD_TIME as QUEUED_OVERLOAD_TIME_MS,
    TOTAL_ELAPSED_TIME as TOTAL_ELAPSED_TIME_MS,

    -- Resource usage
    BYTES_SCANNED,
    BYTES_WRITTEN,
    BYTES_DELETED,
    BYTES_SPILLED_TO_LOCAL_STORAGE,
    BYTES_SPILLED_TO_REMOTE_STORAGE,
    BYTES_SENT_OVER_THE_NETWORK,

    -- Result information
    ROWS_PRODUCED,
    ROWS_INSERTED,
    ROWS_UPDATED,
    ROWS_DELETED,
    ROWS_UNLOADED,

    -- Cost information
    CREDITS_USED_CLOUD_SERVICES,

    -- Partition/cluster information
    PARTITIONS_SCANNED,
    PARTITIONS_TOTAL,

    -- Error information
    ERROR_CODE,
    ERROR_MESSAGE,

    -- Session information
    SESSION_ID

FROM sf_account_usage.ACCOUNT_USAGE.QUERY_HISTORY
WHERE START_TIME >= CURRENT_TIMESTAMP - INTERVAL 30 DAYS
  AND EXECUTION_STATUS = 'SUCCESS'  -- Focus on successful queries
  AND QUERY_TYPE IN ('SELECT', 'INSERT', 'UPDATE', 'DELETE', 'MERGE', 'CREATE_TABLE_AS_SELECT')
ORDER BY START_TIME DESC;

-- Export to Parquet for efficient storage and analysis
COPY query_history TO 'query_history.parquet' (FORMAT PARQUET, COMPRESSION 'zstd');

-- Show summary
SELECT
    COUNT(*) as total_queries,
    MIN(START_TIME) as earliest_query,
    MAX(START_TIME) as latest_query,
    COUNT(DISTINCT USER_NAME) as unique_users,
    COUNT(DISTINCT WAREHOUSE_NAME) as unique_warehouses,
    SUM(ROWS_PRODUCED) as total_rows_produced,
    SUM(BYTES_SCANNED) / (1024.0 * 1024.0 * 1024.0) as total_gb_scanned
FROM query_history;
