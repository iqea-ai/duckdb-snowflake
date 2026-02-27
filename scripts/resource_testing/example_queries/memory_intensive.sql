-- Memory-intensive query: Large aggregation with grouping
-- This query will use more memory as it processes larger datasets
WITH large_data AS (
    SELECT 
        generate_series(1, 10000000) as id,
        (random() * 1000)::INTEGER as category,
        random() * 10000 as value
)
SELECT 
    category,
    COUNT(*) as count,
    SUM(value) as total_value,
    AVG(value) as avg_value,
    MIN(value) as min_value,
    MAX(value) as max_value
FROM large_data
GROUP BY category
ORDER BY count DESC
LIMIT 100;
