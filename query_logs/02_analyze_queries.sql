-- Analyze Query Logs with Key Metrics for Caching Strategy
-- This script computes execution patterns to inform caching decisions

-- Load the query history (either from table or parquet file)
-- If you exported to parquet in step 01:
-- CREATE OR REPLACE TABLE query_history AS SELECT * FROM 'query_history.parquet';

-- Normalize queries by removing literals and whitespace for grouping
-- This helps identify queries with the same structure but different parameters
CREATE OR REPLACE TABLE query_normalized AS
SELECT
    *,
    -- Create a normalized version of the query for grouping
    -- Remove extra whitespace, convert to uppercase, and hash for grouping
    MD5(REGEXP_REPLACE(
        REGEXP_REPLACE(
            REGEXP_REPLACE(
                UPPER(TRIM(QUERY_TEXT)),
                E'\'[^\']*\'', '''?''', 'g'  -- Replace string literals
            ),
            E'\\d+', '?', 'g'  -- Replace numbers
        ),
        E'\\s+', ' ', 'g'  -- Normalize whitespace
    )) as query_signature,

    -- Extract a readable query pattern (first 200 chars of normalized query)
    SUBSTRING(REGEXP_REPLACE(
        REGEXP_REPLACE(
            REGEXP_REPLACE(
                UPPER(TRIM(QUERY_TEXT)),
                E'\'[^\']*\'', '''?''', 'g'
            ),
            E'\\d+', '?', 'g'
        ),
        E'\\s+', ' ', 'g'
    ), 1, 200) as query_pattern
FROM query_history;

-- Compute aggregate metrics per query pattern
-- First, calculate cost per execution with warehouse size
CREATE OR REPLACE TEMP TABLE query_with_cost AS
SELECT
    *,
    CASE WAREHOUSE_SIZE
        WHEN 'X-Small' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 1
        WHEN 'Small' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 2
        WHEN 'Medium' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 4
        WHEN 'Large' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 8
        WHEN 'X-Large' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 16
        WHEN '2X-Large' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 32
        WHEN '3X-Large' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 64
        WHEN '4X-Large' THEN (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 128
        ELSE (EXECUTION_TIME_MS / 1000.0 / 3600.0) * 1  -- Default to X-Small
    END as warehouse_cost_credits
FROM query_normalized;

CREATE OR REPLACE TABLE query_metrics AS
SELECT
    query_signature,
    query_pattern,

    -- Execution statistics
    COUNT(*) as execution_count,
    MAX(START_TIME) as last_seen,
    MIN(START_TIME) as first_seen,

    -- Runtime metrics (in milliseconds)
    AVG(EXECUTION_TIME_MS) as avg_execution_time_ms,
    MEDIAN(EXECUTION_TIME_MS) as median_execution_time_ms,
    MIN(EXECUTION_TIME_MS) as min_execution_time_ms,
    MAX(EXECUTION_TIME_MS) as max_execution_time_ms,
    STDDEV(EXECUTION_TIME_MS) as stddev_execution_time_ms,
    VARIANCE(EXECUTION_TIME_MS) as variance_execution_time_ms,

    -- Result size metrics
    AVG(BYTES_SCANNED) as avg_bytes_scanned,
    AVG(ROWS_PRODUCED) as avg_rows_produced,
    MAX(ROWS_PRODUCED) as max_rows_produced,
    MIN(ROWS_PRODUCED) as min_rows_produced,
    STDDEV(ROWS_PRODUCED) as stddev_rows_produced,

    -- Estimated result size in bytes (assuming avg 100 bytes per row as baseline)
    -- You may want to adjust this based on your actual data
    AVG(ROWS_PRODUCED::BIGINT * 100) as estimated_result_size_bytes,
    MAX(ROWS_PRODUCED::BIGINT * 100) as max_estimated_result_size_bytes,

    -- Data volume scanned
    AVG(BYTES_SCANNED) / (1024.0 * 1024.0) as avg_mb_scanned,
    SUM(BYTES_SCANNED) / (1024.0 * 1024.0 * 1024.0) as total_gb_scanned,

    -- Network transfer
    AVG(BYTES_SENT_OVER_THE_NETWORK) / (1024.0 * 1024.0) as avg_mb_transferred,

    -- Warehouse information
    MODE(WAREHOUSE_NAME) as most_common_warehouse,
    MODE(WAREHOUSE_SIZE) as most_common_warehouse_size,

    -- Cost estimation (Snowflake credits)
    -- This is a rough estimate based on warehouse size and execution time
    -- XS: $2/credit, S: $4/credit, M: $8/credit, L: $16/credit, etc.
    -- Credits per second = size_multiplier / 3600
    SUM(CREDITS_USED_CLOUD_SERVICES) as total_credits_used,
    AVG(CREDITS_USED_CLOUD_SERVICES) as avg_credits_per_execution,

    -- Estimated warehouse cost per execution (rough calculation)
    AVG(warehouse_cost_credits) as avg_warehouse_cost_credits,

    -- Total estimated cost for all executions
    SUM(warehouse_cost_credits) as total_warehouse_cost_credits,

    -- User diversity
    COUNT(DISTINCT USER_NAME) as unique_users,

    -- Coefficient of variation (CV) for runtime stability
    -- Lower CV means more predictable runtime
    CASE
        WHEN AVG(EXECUTION_TIME_MS) > 0
        THEN STDDEV(EXECUTION_TIME_MS) / AVG(EXECUTION_TIME_MS)
        ELSE NULL
    END as runtime_coefficient_of_variation

FROM query_with_cost
WHERE EXECUTION_TIME_MS IS NOT NULL
  AND ROWS_PRODUCED IS NOT NULL
GROUP BY query_signature, query_pattern
HAVING execution_count >= 1  -- Adjust threshold as needed
ORDER BY total_warehouse_cost_credits DESC;

-- Export metrics
COPY query_metrics TO 'query_metrics.parquet' (FORMAT PARQUET, COMPRESSION 'zstd');
COPY query_metrics TO 'query_metrics.csv' (HEADER, DELIMITER ',');

-- Show top queries by various metrics
SELECT
    '=== TOP 20 QUERIES BY EXECUTION COUNT ===' as section;

SELECT
    execution_count,
    avg_execution_time_ms / 1000.0 as avg_seconds,
    avg_rows_produced,
    estimated_result_size_bytes / (1024.0 * 1024.0) as estimated_result_mb,
    avg_warehouse_cost_credits,
    total_warehouse_cost_credits,
    last_seen,
    query_pattern
FROM query_metrics
ORDER BY execution_count DESC
LIMIT 20;

SELECT
    '=== TOP 20 QUERIES BY TOTAL COST ===' as section;

SELECT
    execution_count,
    total_warehouse_cost_credits,
    avg_warehouse_cost_credits,
    avg_execution_time_ms / 1000.0 as avg_seconds,
    total_gb_scanned,
    last_seen,
    query_pattern
FROM query_metrics
ORDER BY total_warehouse_cost_credits DESC
LIMIT 20;

SELECT
    '=== TOP 20 QUERIES BY AVERAGE RUNTIME ===' as section;

SELECT
    execution_count,
    avg_execution_time_ms / 1000.0 as avg_seconds,
    variance_execution_time_ms / (1000.0 * 1000.0) as variance_seconds_squared,
    runtime_coefficient_of_variation,
    avg_warehouse_cost_credits,
    query_pattern
FROM query_metrics
ORDER BY avg_execution_time_ms DESC
LIMIT 20;
