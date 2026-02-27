# Snowflake Query Log Analysis for Caching Strategy

This directory contains SQL scripts to analyze Snowflake query logs and classify queries into optimal execution strategies for a DuckDB-based caching layer in AWS.

## Overview

The goal is to analyze historical Snowflake query patterns to determine the best approach for each query type:

1. **CACHE**: Frequently executed queries worth caching in AWS (S3 + DuckDB)
2. **ARROW_EXTENSION**: Queries efficient enough to use DuckDB Snowflake extension directly
3. **BATCH_TRANSPORT**: Large queries requiring batch data download and upload

## Key Metrics Computed

- **Execution Count**: How often each query pattern runs
- **Average Runtime**: Mean execution time in milliseconds
- **Runtime Variance**: Standard deviation and coefficient of variation
- **Result Size**: Estimated bytes returned (rows × estimated row size)
- **Warehouse Cost**: Credits consumed per execution and total
- **Last Seen**: Most recent execution timestamp
- **Runtime Stability**: Coefficient of variation (lower = more predictable)

## Prerequisites

```bash
# Set Snowflake credentials as environment variables
export SNOWFLAKE_ACCOUNT=your_account
export SNOWFLAKE_USER=your_user
export SNOWFLAKE_PASSWORD=your_password
export SNOWFLAKE_WAREHOUSE=your_warehouse  # Optional, defaults to COMPUTE_WH

# Ensure you have DuckDB installed
brew install duckdb  # macOS
# or download from https://duckdb.org/docs/installation/
```

## Quick Start

### Option 1: Run All Steps Together
```bash
./run_analysis.sh
```

### Option 2: Run Step by Step

#### Step 1: Extract Query History from Snowflake
```bash
duckdb query_analysis.db < 01_extract_query_history.sql
```

This script:
- Connects to Snowflake using the extension
- Pulls query history from `SNOWFLAKE.ACCOUNT_USAGE.QUERY_HISTORY`
- Filters successful queries from the last 30 days
- Exports to `query_history.parquet`

#### Step 2: Analyze Query Patterns
```bash
duckdb query_analysis.db < 02_analyze_queries.sql
```

This script:
- Normalizes queries to group similar patterns (removes literals)
- Computes aggregate metrics per query pattern
- Calculates execution statistics, costs, and runtime stability
- Exports to `query_metrics.parquet` and `query_metrics.csv`

#### Step 3: Classify Queries into Tiers
```bash
duckdb query_analysis.db < 03_classify_caching_strategy.sql
```

This script:
- Applies classification rules based on configurable thresholds
- Assigns each query pattern to CACHE, ARROW_EXTENSION, or BATCH_TRANSPORT
- Calculates priority scores for cache candidates
- Exports to `query_classification.parquet` and `query_classification.csv`

## Classification Logic

### CACHE Tier
Queries are cached when they meet ALL criteria:
- **Execution count** ≥ 5 (frequently run)
- **Runtime CV** ≤ 0.5 (predictable runtime)
- **Result size** ≤ 500 MB (moderate size)
- **Average runtime** ≤ 30 seconds
- **Last seen** within 7 days (recent)

**Benefits**: Eliminates Snowflake costs for repeated queries, provides instant results

### ARROW_EXTENSION Tier
Queries suitable for direct DuckDB → Snowflake Arrow streaming:
- **Result size** ≤ 1 GB
- **Average runtime** ≤ 60 seconds

**Benefits**: Fast enough without caching overhead, good for ad-hoc queries

### BATCH_TRANSPORT Tier
Large or slow queries requiring special handling:
- Anything exceeding Arrow extension thresholds

**Benefits**: Handles massive datasets that would timeout or be inefficient via Arrow

## Customizing Thresholds

Edit the thresholds in `03_classify_caching_strategy.sql`:

```sql
CREATE OR REPLACE TABLE classification_thresholds AS
SELECT
    5 as cache_min_execution_count,      -- Adjust based on your frequency needs
    0.5 as cache_max_cv,                 -- Lower = stricter runtime stability
    500.0 as cache_max_avg_mb,           -- Maximum result size for caching
    30.0 as cache_max_seconds,           -- Maximum runtime for caching
    7 as cache_recency_days,             -- How recent queries must be

    1000.0 as arrow_max_avg_mb,          -- Arrow streaming limit
    60.0 as arrow_max_seconds;           -- Arrow runtime limit
```

## Output Files

- `query_history.parquet`: Raw query history from Snowflake
- `query_metrics.parquet/csv`: Aggregated metrics per query pattern
- `query_classification.parquet/csv`: Final tier assignments with reasoning
- `query_analysis.db`: DuckDB database containing all tables

## Interpreting Results

### Cache Priority Score
Higher scores indicate queries that should be cached first:
```
priority = execution_count × total_cost × recency_factor
```

Focus on queries with:
- High execution count
- High total cost (many credits consumed)
- Recent activity

### Potential Savings
The `potential_cache_savings_credits` column estimates cost reduction:
```
savings = total_cost × 0.95  # Assumes 95% cost reduction when cached
```

### Classification Reason
Each query includes a human-readable reason explaining its tier assignment.

## Example Query

```sql
-- Find top cache candidates
SELECT
    query_pattern,
    execution_count,
    avg_execution_seconds,
    estimated_result_mb,
    total_warehouse_cost_credits,
    potential_cache_savings_credits
FROM query_classification
WHERE caching_tier = 'CACHE'
ORDER BY cache_priority_score DESC
LIMIT 10;
```

## Advanced Usage

### Filter by Time Window
Edit `01_extract_query_history.sql`:
```sql
WHERE START_TIME >= CURRENT_TIMESTAMP - INTERVAL 90 DAYS  -- Last 90 days
```

### Filter by Database/Schema
Edit `01_extract_query_history.sql`:
```sql
WHERE START_TIME >= CURRENT_TIMESTAMP - INTERVAL 30 DAYS
  AND DATABASE_NAME = 'PROD_DB'
  AND SCHEMA_NAME IN ('PUBLIC', 'ANALYTICS')
```

### Analyze Failed Queries
Remove the success filter in `01_extract_query_history.sql`:
```sql
-- AND EXECUTION_STATUS = 'SUCCESS'  -- Comment out to include failures
```

## Cost Estimation

Warehouse costs are estimated based on Snowflake credit consumption:
- X-Small: 1 credit/hour
- Small: 2 credits/hour
- Medium: 4 credits/hour
- Large: 8 credits/hour
- X-Large: 16 credits/hour
- 2X-Large: 32 credits/hour
- 3X-Large: 64 credits/hour
- 4X-Large: 128 credits/hour

Actual costs depend on your Snowflake pricing tier.

## Implementation Strategy

Based on classification results:

1. **Implement Cache Layer First** for CACHE tier queries:
   - Set up S3 bucket in AWS
   - Deploy DuckDB instances for serving
   - Build cache invalidation logic
   - Monitor hit rates

2. **Use Arrow Extension** for ARROW_EXTENSION tier:
   - Direct DuckDB → Snowflake connections
   - No caching overhead
   - Good for exploratory/ad-hoc queries

3. **Batch Processing** for BATCH_TRANSPORT tier:
   - Schedule periodic exports from Snowflake
   - Bulk upload to S3
   - Load into DuckDB for analysis
   - Consider partitioning large datasets

## Monitoring

Track these metrics post-implementation:
- Cache hit rate (target: >80% for CACHE tier)
- Cost reduction (target: match `potential_savings_credits`)
- Query latency (should improve for cached queries)
- Storage costs (S3 + DuckDB instances)

## Troubleshooting

### "Extension not found"
```bash
# Install DuckDB Snowflake extension manually
duckdb -c "INSTALL snowflake FROM community;"
```

### "Connection failed"
Verify environment variables:
```bash
echo $SNOWFLAKE_ACCOUNT
echo $SNOWFLAKE_USER
echo $SNOWFLAKE_WAREHOUSE
```

### "Permission denied on ACCOUNT_USAGE"
Requires `ACCOUNTADMIN` role or granted access to `SNOWFLAKE.ACCOUNT_USAGE` schema.

## Next Steps

1. Run initial analysis
2. Review classification results
3. Adjust thresholds if needed
4. Implement cache layer for top priority queries
5. Measure cost savings and performance improvements
6. Iterate and expand cache coverage

## Contributing

Adjust scripts based on your specific workload patterns and requirements.
