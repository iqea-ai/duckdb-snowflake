-- Simple test query to verify DuckDB is working
SELECT 
    current_setting('max_memory') as configured_memory,
    current_setting('threads') as configured_threads,
    COUNT(*) as test_count
FROM generate_series(1, 1000000);
