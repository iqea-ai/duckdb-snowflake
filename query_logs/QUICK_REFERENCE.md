# Quick Reference Card

## One-Line Commands

```bash
# Run complete analysis
./run_analysis.sh

# Generate visualizations
python visualize_results.py

# View top cache candidates
duckdb query_analysis.db -c "SELECT query_pattern, execution_count, ROUND(cache_priority_score, 2) as priority FROM query_classification WHERE caching_tier = 'CACHE' ORDER BY cache_priority_score DESC LIMIT 10;"

# View tier summary
duckdb query_analysis.db -c "SELECT caching_tier, COUNT(*) as patterns, SUM(execution_count) as executions, ROUND(SUM(total_warehouse_cost_credits), 2) as cost FROM query_classification GROUP BY caching_tier;"

# Export cache candidates to CSV
duckdb query_analysis.db -c "COPY (SELECT * FROM query_classification WHERE caching_tier = 'CACHE' ORDER BY cache_priority_score DESC) TO 'cache_candidates.csv' (HEADER, DELIMITER ',');"
```

## Key Metrics Explained

| Metric | Good | Caution | Bad |
|--------|------|---------|-----|
| **Execution Count** | 100+ | 10-99 | <10 |
| **Runtime CV** | <0.3 | 0.3-0.5 | >0.5 |
| **Result Size** | <100MB | 100-500MB | >1GB |
| **Avg Runtime** | <10s | 10-30s | >60s |
| **Days Since** | <7 | 7-30 | >30 |

## Decision Tree

```
Is execution_count >= 5?
├─ YES: Is runtime_cv <= 0.5?
│  ├─ YES: Is result_size <= 500MB?
│  │  ├─ YES: Is avg_runtime <= 30s?
│  │  │  ├─ YES: Is last_seen <= 7d?
│  │  │  │  ├─ YES: → CACHE
│  │  │  │  └─ NO:  → ARROW_EXTENSION
│  │  │  └─ NO:  → Check size
│  │  └─ NO:  → Check size/runtime
│  └─ NO:  → ARROW_EXTENSION or BATCH
└─ NO:  → ARROW_EXTENSION or BATCH

Result size <= 1GB AND runtime <= 60s?
├─ YES: → ARROW_EXTENSION
└─ NO:  → BATCH_TRANSPORT
```

## File Outputs

| File | Contents |
|------|----------|
| `query_history.parquet` | Raw query logs from Snowflake |
| `query_metrics.parquet/csv` | Aggregated metrics per query pattern |
| `query_classification.parquet/csv` | Final tier assignments + reasoning |
| `reports/cache_queries.csv` | CACHE tier queries sorted by priority |
| `reports/arrow_extension_queries.csv` | ARROW_EXTENSION tier queries |
| `reports/batch_transport_queries.csv` | BATCH_TRANSPORT tier queries |
| `reports/summary_report.txt` | Text summary with top candidates |
| `reports/overview_charts.png` | Visualizations |

## Tier Breakdown

### 🎯 CACHE
**When**: Frequent, stable, moderate-sized, recent
**Action**: Implement AWS S3 + DuckDB caching
**Savings**: 95%+ cost reduction

### ⚡ ARROW_EXTENSION
**When**: Fast enough for direct connection
**Action**: Use DuckDB Snowflake extension
**Savings**: No caching overhead

### 📦 BATCH_TRANSPORT
**When**: Too large/slow for streaming
**Action**: Scheduled batch exports
**Savings**: One-time export, fast local analysis

## Customization Snippets

### Increase cache size limit
```sql
-- In 03_classify_caching_strategy.sql
500.0 as cache_max_avg_mb,  -- Change to 1000.0
```

### Decrease frequency requirement
```sql
-- In 03_classify_caching_strategy.sql
5 as cache_min_execution_count,  -- Change to 3
```

### Extend time window
```sql
-- In 01_extract_query_history.sql
WHERE START_TIME >= CURRENT_TIMESTAMP - INTERVAL 30 DAYS  -- Change to 90
```

## Troubleshooting

| Error | Solution |
|-------|----------|
| "ADBC driver not found" | Copy `adbc_drivers/libadbc_driver_snowflake.so` to `~/.duckdb/extensions/v1.4.3/osx_arm64/` |
| "Permission denied on ACCOUNT_USAGE" | Need ACCOUNTADMIN role or granted access |
| "MFA required" | Use service account or configure MFA in extension |
| "Connection failed" | Check env variables: `echo $SNOWFLAKE_ACCOUNT` |
| No data returned | Check time window, verify queries exist in period |

## Environment Setup

```bash
# Copy template
cp .env.template .env

# Edit .env with your credentials
export SNOWFLAKE_ACCOUNT=your_account
export SNOWFLAKE_USER=your_user
export SNOWFLAKE_PASSWORD=your_password
export SNOWFLAKE_WAREHOUSE=your_warehouse

# Source it
source .env

# Verify
env | grep SNOWFLAKE
```

## Sample Queries

### Top 10 most expensive queries
```sql
SELECT query_pattern, execution_count, total_warehouse_cost_credits
FROM query_classification
ORDER BY total_warehouse_cost_credits DESC
LIMIT 10;
```

### Queries with unstable runtimes
```sql
SELECT query_pattern, execution_count, runtime_coefficient_of_variation
FROM query_classification
WHERE runtime_coefficient_of_variation > 0.5
ORDER BY runtime_coefficient_of_variation DESC;
```

### Recent cache candidates
```sql
SELECT query_pattern, days_since_last_seen, cache_priority_score
FROM query_classification
WHERE caching_tier = 'CACHE' AND days_since_last_seen <= 1
ORDER BY cache_priority_score DESC;
```

### Large result queries
```sql
SELECT query_pattern, caching_tier, estimated_result_mb
FROM query_classification
WHERE estimated_result_mb > 100
ORDER BY estimated_result_mb DESC;
```

## Implementation Priority

1. **Week 1**: Top 10 CACHE tier by priority score
2. **Week 2**: Next 20 CACHE tier queries
3. **Week 3**: High-cost CACHE tier queries
4. **Week 4**: Monitor, adjust, expand

Focus on high frequency × high cost queries first for maximum ROI.
