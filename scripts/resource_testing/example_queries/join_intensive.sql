-- Join-intensive query: Multiple table joins
-- This query tests memory usage with hash joins
WITH 
table1 AS (
    SELECT 
        generate_series(1, 1000000) as id,
        (random() * 100)::INTEGER as key1,
        'data_' || generate_series(1, 1000000) as data1
),
table2 AS (
    SELECT 
        generate_series(1, 1000000) as id,
        (random() * 100)::INTEGER as key2,
        'data_' || generate_series(1, 1000000) as data2
),
table3 AS (
    SELECT 
        generate_series(1, 1000000) as id,
        (random() * 100)::INTEGER as key3,
        'data_' || generate_series(1, 1000000) as data3
)
SELECT 
    t1.key1,
    t2.key2,
    t3.key3,
    COUNT(*) as join_count,
    COUNT(DISTINCT t1.id) as distinct_t1,
    COUNT(DISTINCT t2.id) as distinct_t2,
    COUNT(DISTINCT t3.id) as distinct_t3
FROM table1 t1
JOIN table2 t2 ON t1.key1 = t2.key2
JOIN table3 t3 ON t2.key2 = t3.key3
GROUP BY t1.key1, t2.key2, t3.key3
ORDER BY join_count DESC
LIMIT 50;
