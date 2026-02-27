-- CPU-intensive query: Complex calculations and window functions
-- This query will benefit from more CPU threads
WITH numbers AS (
    SELECT generate_series(1, 5000000) as n
),
calculated AS (
    SELECT 
        n,
        SQRT(n) as sqrt_n,
        LOG(n) as log_n,
        SIN(n) as sin_n,
        COS(n) as cos_n,
        POWER(n, 2) as n_squared,
        MOD(n, 1000) as mod_n
    FROM numbers
)
SELECT 
    mod_n,
    COUNT(*) as count,
    AVG(sqrt_n) as avg_sqrt,
    AVG(log_n) as avg_log,
    SUM(sin_n) as sum_sin,
    SUM(cos_n) as sum_cos,
    AVG(n_squared) as avg_squared,
    -- Window function for ranking
    RANK() OVER (ORDER BY count DESC) as rank
FROM calculated
GROUP BY mod_n
ORDER BY count DESC
LIMIT 100;
