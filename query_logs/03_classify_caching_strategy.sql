-- Classify Queries into Caching Strategy Tiers
-- Based on execution patterns, result sizes, and costs
--
-- Three tiers:
-- 1. CACHE: Frequently executed, predictable queries worth caching in AWS
-- 2. ARROW_EXTENSION: Fast queries that can use DuckDB Snowflake extension directly
-- 3. BATCH_TRANSPORT: Large result sets that need batch download and upload to DuckDB

-- Load query metrics if not already in memory
-- CREATE OR REPLACE TABLE query_metrics AS SELECT * FROM 'query_metrics.parquet';

-- Define classification parameters (adjust these based on your workload)
CREATE OR REPLACE TABLE classification_thresholds AS
SELECT
    -- CACHE tier thresholds
    5 as cache_min_execution_count,           -- Must run at least 5 times
    0.5 as cache_max_cv,                      -- Coefficient of variation < 0.5 (stable runtime)
    500.0 as cache_max_avg_mb,                -- Average result < 500 MB
    30.0 as cache_max_seconds,                -- Average runtime < 30 seconds
    7 as cache_recency_days,                  -- Last seen within 7 days

    -- ARROW_EXTENSION tier thresholds
    1000.0 as arrow_max_avg_mb,               -- Can handle up to 1 GB efficiently via Arrow
    60.0 as arrow_max_seconds,                -- Runtime < 1 minute is acceptable

    -- BATCH_TRANSPORT tier (everything else)
    -- These queries exceed Arrow capabilities and need batch processing
    1 as batch_min_execution_count;           -- Any query that runs at least once

-- Classify queries into tiers
CREATE OR REPLACE TABLE query_classification AS
WITH thresholds AS (
    SELECT * FROM classification_thresholds
),
classified AS (
    SELECT
        qm.*,
        t.*,

        -- Days since last execution
        DATE_DIFF('day', qm.last_seen, CURRENT_TIMESTAMP) as days_since_last_seen,

        -- Estimated result size in MB
        qm.estimated_result_size_bytes / (1024.0 * 1024.0) as estimated_result_mb,

        -- Classify into tiers
        CASE
            -- CACHE: High frequency, stable, moderate size, recent
            WHEN qm.execution_count >= t.cache_min_execution_count
             AND (qm.runtime_coefficient_of_variation IS NULL OR qm.runtime_coefficient_of_variation <= t.cache_max_cv)
             AND qm.estimated_result_size_bytes / (1024.0 * 1024.0) <= t.cache_max_avg_mb
             AND qm.avg_execution_time_ms / 1000.0 <= t.cache_max_seconds
             AND DATE_DIFF('day', qm.last_seen, CURRENT_TIMESTAMP) <= t.cache_recency_days
            THEN 'CACHE'

            -- ARROW_EXTENSION: Reasonable size and runtime for Arrow streaming
            WHEN qm.estimated_result_size_bytes / (1024.0 * 1024.0) <= t.arrow_max_avg_mb
             AND qm.avg_execution_time_ms / 1000.0 <= t.arrow_max_seconds
            THEN 'ARROW_EXTENSION'

            -- BATCH_TRANSPORT: Everything else (large or slow queries)
            ELSE 'BATCH_TRANSPORT'
        END as caching_tier,

        -- Provide reasoning for classification
        CASE
            WHEN qm.execution_count >= t.cache_min_execution_count
             AND (qm.runtime_coefficient_of_variation IS NULL OR qm.runtime_coefficient_of_variation <= t.cache_max_cv)
             AND qm.estimated_result_size_bytes / (1024.0 * 1024.0) <= t.cache_max_avg_mb
             AND qm.avg_execution_time_ms / 1000.0 <= t.cache_max_seconds
             AND DATE_DIFF('day', qm.last_seen, CURRENT_TIMESTAMP) <= t.cache_recency_days
            THEN 'High frequency (' || qm.execution_count || 'x), stable runtime (CV=' ||
                 ROUND(COALESCE(qm.runtime_coefficient_of_variation, 0), 2) || '), moderate size (' ||
                 ROUND(qm.estimated_result_size_bytes / (1024.0 * 1024.0), 2) || ' MB), recent'

            WHEN qm.estimated_result_size_bytes / (1024.0 * 1024.0) <= t.arrow_max_avg_mb
             AND qm.avg_execution_time_ms / 1000.0 <= t.arrow_max_seconds
            THEN 'Suitable for Arrow streaming: ' ||
                 ROUND(qm.estimated_result_size_bytes / (1024.0 * 1024.0), 2) || ' MB avg, ' ||
                 ROUND(qm.avg_execution_time_ms / 1000.0, 2) || 's avg runtime'

            ELSE 'Large/slow query: ' ||
                 ROUND(qm.estimated_result_size_bytes / (1024.0 * 1024.0), 2) || ' MB avg, ' ||
                 ROUND(qm.avg_execution_time_ms / 1000.0, 2) || 's avg runtime - needs batch processing'
        END as classification_reason,

        -- Calculate potential savings from caching
        CASE
            WHEN qm.execution_count >= t.cache_min_execution_count
            THEN qm.total_warehouse_cost_credits * 0.95  -- Assume 95% cost reduction if cached
            ELSE 0
        END as potential_cache_savings_credits,

        -- Priority score for cache implementation (higher = more important to cache)
        CASE
            WHEN qm.execution_count >= t.cache_min_execution_count
             AND (qm.runtime_coefficient_of_variation IS NULL OR qm.runtime_coefficient_of_variation <= t.cache_max_cv)
             AND qm.estimated_result_size_bytes / (1024.0 * 1024.0) <= t.cache_max_avg_mb
             AND qm.avg_execution_time_ms / 1000.0 <= t.cache_max_seconds
             AND DATE_DIFF('day', qm.last_seen, CURRENT_TIMESTAMP) <= t.cache_recency_days
            THEN
                -- Priority based on: frequency * cost * recency_factor
                qm.execution_count *
                qm.total_warehouse_cost_credits *
                (1.0 / (1.0 + DATE_DIFF('day', qm.last_seen, CURRENT_TIMESTAMP)))
            ELSE 0
        END as cache_priority_score

    FROM query_metrics qm
    CROSS JOIN thresholds t
)
SELECT
    query_signature,
    query_pattern,
    caching_tier,
    classification_reason,
    execution_count,
    last_seen,
    days_since_last_seen,

    -- Runtime metrics
    avg_execution_time_ms / 1000.0 as avg_execution_seconds,
    median_execution_time_ms / 1000.0 as median_execution_seconds,
    stddev_execution_time_ms / 1000.0 as stddev_execution_seconds,
    runtime_coefficient_of_variation,

    -- Size metrics
    avg_rows_produced,
    estimated_result_mb,
    max_estimated_result_size_bytes / (1024.0 * 1024.0) as max_estimated_result_mb,
    avg_mb_scanned,
    total_gb_scanned,

    -- Cost metrics
    avg_warehouse_cost_credits,
    total_warehouse_cost_credits,
    potential_cache_savings_credits,
    cache_priority_score,

    -- Warehouse info
    most_common_warehouse,
    most_common_warehouse_size,
    unique_users

FROM classified
ORDER BY cache_priority_score DESC, total_warehouse_cost_credits DESC;

-- Export classification results
COPY query_classification TO 'query_classification.parquet' (FORMAT PARQUET, COMPRESSION 'zstd');
COPY query_classification TO 'query_classification.csv' (HEADER, DELIMITER ',');

-- Summary statistics by tier
SELECT
    '=== CLASSIFICATION SUMMARY ===' as section;

SELECT
    caching_tier,
    COUNT(*) as query_pattern_count,
    SUM(execution_count) as total_executions,
    ROUND(SUM(total_warehouse_cost_credits), 2) as total_cost_credits,
    ROUND(SUM(potential_cache_savings_credits), 2) as potential_savings_credits,
    ROUND(AVG(avg_execution_seconds), 2) as avg_runtime_seconds,
    ROUND(AVG(estimated_result_mb), 2) as avg_result_mb,
    ROUND(SUM(total_gb_scanned), 2) as total_gb_scanned
FROM query_classification
GROUP BY caching_tier
ORDER BY
    CASE caching_tier
        WHEN 'CACHE' THEN 1
        WHEN 'ARROW_EXTENSION' THEN 2
        WHEN 'BATCH_TRANSPORT' THEN 3
    END;

-- Top cache candidates
SELECT
    '=== TOP 20 CACHE CANDIDATES (by priority score) ===' as section;

SELECT
    ROUND(cache_priority_score, 2) as priority,
    execution_count,
    ROUND(avg_execution_seconds, 2) as avg_sec,
    ROUND(estimated_result_mb, 2) as result_mb,
    ROUND(total_warehouse_cost_credits, 2) as total_cost,
    ROUND(potential_cache_savings_credits, 2) as savings,
    days_since_last_seen as days_ago,
    query_pattern
FROM query_classification
WHERE caching_tier = 'CACHE'
ORDER BY cache_priority_score DESC
LIMIT 20;

-- Batch transport queries (need special handling)
SELECT
    '=== TOP 20 BATCH TRANSPORT QUERIES ===' as section;

SELECT
    execution_count,
    ROUND(avg_execution_seconds, 2) as avg_sec,
    ROUND(estimated_result_mb, 2) as result_mb,
    ROUND(max_estimated_result_mb, 2) as max_result_mb,
    ROUND(total_warehouse_cost_credits, 2) as total_cost,
    classification_reason,
    query_pattern
FROM query_classification
WHERE caching_tier = 'BATCH_TRANSPORT'
ORDER BY total_warehouse_cost_credits DESC
LIMIT 20;

-- Arrow extension queries (can use direct connection)
SELECT
    '=== TOP 20 ARROW EXTENSION QUERIES ===' as section;

SELECT
    execution_count,
    ROUND(avg_execution_seconds, 2) as avg_sec,
    ROUND(estimated_result_mb, 2) as result_mb,
    ROUND(total_warehouse_cost_credits, 2) as total_cost,
    days_since_last_seen as days_ago,
    query_pattern
FROM query_classification
WHERE caching_tier = 'ARROW_EXTENSION'
ORDER BY total_warehouse_cost_credits DESC
LIMIT 20;
