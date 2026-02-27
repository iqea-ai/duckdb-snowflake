-- Create Demo Query History Data
-- This simulates realistic Snowflake query logs for demonstration
-- Run this to test the analysis system without ACCOUNT_USAGE access

-- Create simulated query history
CREATE OR REPLACE TABLE query_history AS
SELECT * FROM (VALUES
    -- High frequency, stable dashboard queries (CACHE candidates)
    ('qid-001', 'SELECT customer_id, SUM(total_amount) FROM orders WHERE order_date >= ''2024-01-01'' GROUP BY customer_id', 'SELECT', NULL, 'analyst1', 'PUBLIC', 'ANALYTICS_DB', 'PUBLIC', 'COMPUTE_WH', 'X-Small', 'STANDARD',
     '2024-12-20 10:00:00'::TIMESTAMP, '2024-12-20 10:00:02'::TIMESTAMP, 'SUCCESS', 2000, 500, 0, 0, 0, 2500, 1048576, 0, 0, 0, 0, 524288, 1500, 0, 0, 0, 0, 0.001, 10, 100, NULL, NULL, 'session-1'),
    ('qid-002', 'SELECT customer_id, SUM(total_amount) FROM orders WHERE order_date >= ''2024-01-02'' GROUP BY customer_id', 'SELECT', NULL, 'analyst1', 'PUBLIC', 'ANALYTICS_DB', 'PUBLIC', 'COMPUTE_WH', 'X-Small', 'STANDARD',
     '2024-12-20 11:00:00'::TIMESTAMP, '2024-12-20 11:00:02'::TIMESTAMP, 'SUCCESS', 1950, 480, 0, 0, 0, 2430, 1048576, 0, 0, 0, 0, 524288, 1480, 0, 0, 0, 0, 0.001, 10, 100, NULL, NULL, 'session-2'),
    ('qid-003', 'SELECT customer_id, SUM(total_amount) FROM orders WHERE order_date >= ''2024-01-03'' GROUP BY customer_id', 'SELECT', NULL, 'analyst2', 'PUBLIC', 'ANALYTICS_DB', 'PUBLIC', 'COMPUTE_WH', 'X-Small', 'STANDARD',
     '2024-12-20 12:00:00'::TIMESTAMP, '2024-12-20 12:00:02'::TIMESTAMP, 'SUCCESS', 2100, 510, 0, 0, 0, 2610, 1048576, 0, 0, 0, 0, 524288, 1520, 0, 0, 0, 0, 0.001, 10, 100, NULL, NULL, 'session-3'),
    ('qid-004', 'SELECT customer_id, SUM(total_amount) FROM orders WHERE order_date >= ''2024-01-04'' GROUP BY customer_id', 'SELECT', NULL, 'analyst1', 'PUBLIC', 'ANALYTICS_DB', 'PUBLIC', 'COMPUTE_WH', 'X-Small', 'STANDARD',
     '2024-12-21 10:00:00'::TIMESTAMP, '2024-12-21 10:00:02'::TIMESTAMP, 'SUCCESS', 2050, 495, 0, 0, 0, 2545, 1048576, 0, 0, 0, 0, 524288, 1505, 0, 0, 0, 0, 0.001, 10, 100, NULL, NULL, 'session-4'),
    ('qid-005', 'SELECT customer_id, SUM(total_amount) FROM orders WHERE order_date >= ''2024-01-05'' GROUP BY customer_id', 'SELECT', NULL, 'analyst2', 'PUBLIC', 'ANALYTICS_DB', 'PUBLIC', 'COMPUTE_WH', 'X-Small', 'STANDARD',
     '2024-12-21 11:00:00'::TIMESTAMP, '2024-12-21 11:00:02'::TIMESTAMP, 'SUCCESS', 1980, 485, 0, 0, 0, 2465, 1048576, 0, 0, 0, 0, 524288, 1490, 0, 0, 0, 0, 0.001, 10, 100, NULL, NULL, 'session-5'),
    ('qid-006', 'SELECT customer_id, SUM(total_amount) FROM orders WHERE order_date >= ''2024-01-06'' GROUP BY customer_id', 'SELECT', NULL, 'analyst1', 'PUBLIC', 'ANALYTICS_DB', 'PUBLIC', 'COMPUTE_WH', 'X-Small', 'STANDARD',
     '2024-12-22 10:00:00'::TIMESTAMP, '2024-12-22 10:00:02'::TIMESTAMP, 'SUCCESS', 2020, 492, 0, 0, 0, 2512, 1048576, 0, 0, 0, 0, 524288, 1495, 0, 0, 0, 0, 0.001, 10, 100, NULL, NULL, 'session-6'),

    -- Medium frequency, moderate size queries (ARROW_EXTENSION candidates)
    ('qid-100', 'SELECT * FROM large_table WHERE category = ''Electronics'' AND date >= ''2024-01-01''', 'SELECT', NULL, 'data_scientist', 'ANALYST', 'PROD_DB', 'SALES', 'ANALYTICS_WH', 'Medium', 'STANDARD',
     '2024-12-20 14:00:00'::TIMESTAMP, '2024-12-20 14:00:25'::TIMESTAMP, 'SUCCESS', 25000, 3000, 0, 0, 0, 28000, 52428800, 0, 0, 0, 0, 10485760, 50000, 0, 0, 0, 0, 0.003, 50, 500, NULL, NULL, 'session-100'),
    ('qid-101', 'SELECT * FROM large_table WHERE category = ''Home'' AND date >= ''2024-01-01''', 'SELECT', NULL, 'data_scientist', 'ANALYST', 'PROD_DB', 'SALES', 'ANALYTICS_WH', 'Medium', 'STANDARD',
     '2024-12-21 14:00:00'::TIMESTAMP, '2024-12-21 14:00:23'::TIMESTAMP, 'SUCCESS', 23000, 2900, 0, 0, 0, 25900, 52428800, 0, 0, 0, 0, 10485760, 48000, 0, 0, 0, 0, 0.003, 50, 500, NULL, NULL, 'session-101'),
    ('qid-102', 'SELECT * FROM large_table WHERE category = ''Sports'' AND date >= ''2024-01-01''', 'SELECT', NULL, 'analyst3', 'ANALYST', 'PROD_DB', 'SALES', 'ANALYTICS_WH', 'Medium', 'STANDARD',
     '2024-12-22 14:00:00'::TIMESTAMP, '2024-12-22 14:00:27'::TIMESTAMP, 'SUCCESS', 27000, 3100, 0, 0, 0, 30100, 52428800, 0, 0, 0, 0, 10485760, 52000, 0, 0, 0, 0, 0.003, 50, 500, NULL, NULL, 'session-102'),

    -- Low frequency, large data queries (BATCH_TRANSPORT candidates)
    ('qid-200', 'SELECT * FROM fact_sales fs JOIN dim_customer dc ON fs.customer_key = dc.customer_key WHERE fs.sale_date >= ''2023-01-01''', 'SELECT', NULL, 'etl_user', 'ETL_ROLE', 'DW_DB', 'DW_SCHEMA', 'ETL_WH', 'Large', 'STANDARD',
     '2024-12-18 02:00:00'::TIMESTAMP, '2024-12-18 02:15:30'::TIMESTAMP, 'SUCCESS', 930000, 120000, 0, 0, 0, 1050000, 2147483648, 0, 0, 0, 0, 536870912, 10000000, 0, 0, 0, 0, 0.035, 200, 2000, NULL, NULL, 'session-200'),
    ('qid-201', 'SELECT product_id, SUM(quantity * unit_price) FROM fact_sales WHERE sale_date BETWEEN ''2020-01-01'' AND ''2024-12-31'' GROUP BY product_id', 'SELECT', NULL, 'etl_user', 'ETL_ROLE', 'DW_DB', 'DW_SCHEMA', 'ETL_WH', 'X-Large', 'STANDARD',
     '2024-12-19 03:00:00'::TIMESTAMP, '2024-12-19 03:25:45'::TIMESTAMP, 'SUCCESS', 1545000, 180000, 0, 0, 0, 1725000, 5368709120, 0, 0, 0, 0, 1073741824, 50000000, 0, 0, 0, 0, 0.075, 500, 5000, NULL, NULL, 'session-201'),

    -- Unstable runtime queries (variable performance)
    ('qid-300', 'SELECT * FROM events WHERE user_id IN (SELECT user_id FROM active_users WHERE last_login > CURRENT_DATE - 30)', 'SELECT', NULL, 'app_user', 'APP_ROLE', 'APP_DB', 'PUBLIC', 'APP_WH', 'Small', 'STANDARD',
     '2024-12-20 16:00:00'::TIMESTAMP, '2024-12-20 16:00:05'::TIMESTAMP, 'SUCCESS', 5000, 800, 0, 0, 0, 5800, 10485760, 0, 0, 0, 0, 2097152, 100000, 0, 0, 0, 0, 0.002, 25, 250, NULL, NULL, 'session-300'),
    ('qid-301', 'SELECT * FROM events WHERE user_id IN (SELECT user_id FROM active_users WHERE last_login > CURRENT_DATE - 30)', 'SELECT', NULL, 'app_user', 'APP_ROLE', 'APP_DB', 'PUBLIC', 'APP_WH', 'Small', 'STANDARD',
     '2024-12-21 16:00:00'::TIMESTAMP, '2024-12-21 16:00:15'::TIMESTAMP, 'SUCCESS', 15000, 1200, 0, 0, 0, 16200, 10485760, 0, 0, 0, 0, 2097152, 120000, 0, 0, 0, 0, 0.005, 25, 250, NULL, NULL, 'session-301'),
    ('qid-302', 'SELECT * FROM events WHERE user_id IN (SELECT user_id FROM active_users WHERE last_login > CURRENT_DATE - 30)', 'SELECT', NULL, 'app_user', 'APP_ROLE', 'APP_DB', 'PUBLIC', 'APP_WH', 'Small', 'STANDARD',
     '2024-12-22 16:00:00'::TIMESTAMP, '2024-12-22 16:00:08'::TIMESTAMP, 'SUCCESS', 8000, 900, 0, 0, 0, 8900, 10485760, 0, 0, 0, 0, 2097152, 105000, 0, 0, 0, 0, 0.003, 25, 250, NULL, NULL, 'session-302'),
    ('qid-303', 'SELECT * FROM events WHERE user_id IN (SELECT user_id FROM active_users WHERE last_login > CURRENT_DATE - 30)', 'SELECT', NULL, 'app_user', 'APP_ROLE', 'APP_DB', 'PUBLIC', 'APP_WH', 'Small', 'STANDARD',
     '2024-12-23 16:00:00'::TIMESTAMP, '2024-12-23 16:00:20'::TIMESTAMP, 'SUCCESS', 20000, 1500, 0, 0, 0, 21500, 10485760, 0, 0, 0, 0, 2097152, 150000, 0, 0, 0, 0, 0.007, 25, 250, NULL, NULL, 'session-303'),
    ('qid-304', 'SELECT * FROM events WHERE user_id IN (SELECT user_id FROM active_users WHERE last_login > CURRENT_DATE - 30)', 'SELECT', NULL, 'app_user', 'APP_ROLE', 'APP_DB', 'PUBLIC', 'APP_WH', 'Small', 'STANDARD',
     '2024-12-24 16:00:00'::TIMESTAMP, '2024-12-24 16:00:06'::TIMESTAMP, 'SUCCESS', 6000, 850, 0, 0, 0, 6850, 10485760, 0, 0, 0, 0, 2097152, 102000, 0, 0, 0, 0, 0.002, 25, 250, NULL, NULL, 'session-304')

) AS t(
    QUERY_ID, QUERY_TEXT, QUERY_TYPE, QUERY_TAG,
    USER_NAME, ROLE_NAME, DATABASE_NAME, SCHEMA_NAME, WAREHOUSE_NAME, WAREHOUSE_SIZE, WAREHOUSE_TYPE,
    START_TIME, END_TIME, EXECUTION_STATUS,
    EXECUTION_TIME_MS, COMPILATION_TIME_MS, QUEUED_PROVISIONING_TIME_MS, QUEUED_REPAIR_TIME_MS, QUEUED_OVERLOAD_TIME_MS, TOTAL_ELAPSED_TIME_MS,
    BYTES_SCANNED, BYTES_WRITTEN, BYTES_DELETED, BYTES_SPILLED_TO_LOCAL_STORAGE, BYTES_SPILLED_TO_REMOTE_STORAGE, BYTES_SENT_OVER_THE_NETWORK,
    ROWS_PRODUCED, ROWS_INSERTED, ROWS_UPDATED, ROWS_DELETED, ROWS_UNLOADED,
    CREDITS_USED_CLOUD_SERVICES,
    PARTITIONS_SCANNED, PARTITIONS_TOTAL,
    ERROR_CODE, ERROR_MESSAGE,
    SESSION_ID
);

-- Export demo data
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

SELECT 'Demo data created successfully! Run steps 2 and 3 to analyze.' as message;
