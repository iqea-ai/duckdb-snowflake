# Query Metrics Reference Guide

This guide explains each metric computed during the analysis and how to interpret them for making caching decisions.

## Core Execution Metrics

### Execution Count
**What it measures**: Number of times a query pattern has been executed
**Range**: 1 to thousands+
**Interpretation**:
- **High (100+)**: Strong cache candidate - frequently used
- **Medium (10-99)**: Consider caching if other metrics favor it
- **Low (1-9)**: Probably not worth caching unless very expensive

**Use for**: Identifying frequently-run queries worth optimizing

---

### Last Seen / Days Since Last Seen
**What it measures**: When the query was most recently executed
**Interpretation**:
- **Recent (0-7 days)**: Active query, high priority
- **Moderate (8-30 days)**: Still relevant, medium priority
- **Stale (30+ days)**: May be obsolete, low priority

**Use for**: Filtering out outdated queries from cache candidates

---

## Runtime Metrics

### Average Execution Time (seconds)
**What it measures**: Mean query execution time across all runs
**Range**: Milliseconds to hours
**Interpretation**:
- **Fast (< 1s)**: Already performant, caching less critical
- **Moderate (1-30s)**: Good cache candidates if frequent
- **Slow (30s+)**: Strong candidate if result size is reasonable

**Use for**: Identifying queries where caching provides latency improvements

---

### Median Execution Time
**What it measures**: Middle value of execution times (50th percentile)
**Why it matters**: Less sensitive to outliers than mean
**Interpretation**: Compare to average:
- If median ≈ average: Consistent performance
- If median < average: Some slow outliers exist

**Use for**: Understanding typical query performance

---

### Standard Deviation (Runtime)
**What it measures**: How much execution times vary
**Range**: 0 (no variation) to large values
**Interpretation**:
- **Low stddev**: Predictable runtime
- **High stddev**: Variable performance (data-dependent queries)

**Use for**: Assessing runtime predictability

---

### Runtime Coefficient of Variation (CV)
**What it measures**: Relative variability (stddev / mean)
**Range**: 0.0 (perfectly stable) to 1.0+ (highly variable)
**Interpretation**:
- **CV < 0.3**: Very stable, excellent cache candidate
- **CV 0.3-0.5**: Moderately stable, acceptable for caching
- **CV > 0.5**: Unstable, results may change frequently

**Use for**: **PRIMARY METRIC** for cache suitability - lower is better

**Example**:
```
Query A: avg=10s, stddev=2s  → CV=0.2 (stable)
Query B: avg=10s, stddev=8s  → CV=0.8 (unstable)
```
Query A is a much better cache candidate!

---

## Result Size Metrics

### Average Rows Produced
**What it measures**: Mean number of rows returned
**Use for**: Estimating result dataset size

---

### Estimated Result Size (MB)
**What it measures**: Approximate result size in megabytes
**Calculation**: `rows_produced × estimated_bytes_per_row / (1024²)`
**Range**: KB to GB+
**Interpretation**:
- **Small (< 100 MB)**: Ideal for caching
- **Medium (100-500 MB)**: Good for caching if frequently accessed
- **Large (500-1000 MB)**: Consider caching only if very frequent
- **Very Large (1 GB+)**: Better for batch transport

**Use for**: Determining storage costs and transfer efficiency

**Note**: Default assumes 100 bytes/row - adjust based on your schema

---

### Max Estimated Result Size
**What it measures**: Largest result size ever produced
**Use for**: Capacity planning for cache storage

---

## Data Scan Metrics

### Average MB Scanned
**What it measures**: Average amount of data read from Snowflake per execution
**Interpretation**:
- High scan volume → expensive queries
- Scan ≫ result size → significant filtering/aggregation

**Use for**: Understanding query selectivity

---

### Total GB Scanned
**What it measures**: Cumulative data scanned across all executions
**Use for**: Identifying data-intensive query patterns

---

## Cost Metrics

### Average Warehouse Cost (Credits)
**What it measures**: Estimated Snowflake credits per execution
**Calculation**: `(runtime_seconds / 3600) × warehouse_credits_per_hour`
**Interpretation**:
- Based on warehouse size (X-Small = 1 credit/hr, Small = 2, etc.)
- Does not include cloud services costs

**Use for**: Per-execution cost estimation

---

### Total Warehouse Cost (Credits)
**What it measures**: Cumulative cost across all executions
**Formula**: `execution_count × avg_warehouse_cost`
**Interpretation**:
- **High cost queries** are priority targets for optimization
- Even low-frequency queries can be expensive if slow

**Use for**: **KEY METRIC** for ROI analysis - highest cost queries have biggest savings potential

---

### Potential Cache Savings (Credits)
**What it measures**: Estimated cost reduction if query is cached
**Calculation**: `total_cost × 0.95` (assumes 95% cost elimination)
**Interpretation**:
- Only computed for cache-eligible queries
- Represents maximum possible savings
- Actual savings depend on cache hit rate

**Use for**: Prioritizing which queries to cache first

---

### Cache Priority Score
**What it measures**: Combined metric for cache implementation priority
**Calculation**: `execution_count × total_cost × recency_factor`
**Range**: 0 to very large numbers
**Interpretation**:
- **Higher = higher priority** for cache implementation
- Balances frequency, cost, and recency
- Top 20 by this metric should be your first cache targets

**Use for**: **PRIMARY RANKING METRIC** for implementation order

---

## Classification Tiers

### CACHE
**Criteria** (ALL must be true):
- Execution count ≥ 5
- Runtime CV ≤ 0.5
- Result size ≤ 500 MB
- Avg runtime ≤ 30 seconds
- Last seen within 7 days

**Strategy**: Implement caching layer in AWS (S3 + DuckDB)
**Benefits**:
- Near-instant query response
- 95%+ cost reduction
- Reduced Snowflake load

**Considerations**:
- Cache invalidation strategy needed
- Storage costs (typically very low)
- Stale data risk (mitigate with TTL)

---

### ARROW_EXTENSION
**Criteria**:
- Result size ≤ 1 GB
- Avg runtime ≤ 60 seconds
- Doesn't meet CACHE criteria

**Strategy**: Use DuckDB Snowflake extension directly
**Benefits**:
- No cache management overhead
- Always fresh data
- Good for ad-hoc queries

**Considerations**:
- Still incurs Snowflake costs
- Subject to Snowflake performance
- Network latency on each execution

---

### BATCH_TRANSPORT
**Criteria**:
- Exceeds ARROW_EXTENSION thresholds
- Large results or slow queries

**Strategy**: Batch export from Snowflake, bulk load to DuckDB
**Benefits**:
- Handles massive datasets
- One-time export cost
- Fast local analysis after loading

**Considerations**:
- More complex pipeline
- Data freshness depends on export schedule
- Storage costs higher
- Best for periodic reporting/analytics

---

## Quick Decision Matrix

| Metric | Cache? | Arrow? | Batch? |
|--------|--------|--------|--------|
| Execution count: 100+ | ✅ | ✅ | ✅ |
| Execution count: 5-99 | ✅ | ✅ | ✅ |
| Execution count: 1-4 | ❌ | ✅ | ✅ |
| Runtime CV < 0.3 | ✅ | ✅ | ✅ |
| Runtime CV > 0.5 | ❌ | ✅ | ✅ |
| Result size < 100 MB | ✅ | ✅ | ❌ |
| Result size 100-500 MB | ✅ | ✅ | ✅ |
| Result size 500-1000 MB | ⚠️ | ✅ | ✅ |
| Result size > 1 GB | ❌ | ❌ | ✅ |
| Runtime < 10s | ⚠️ | ✅ | ❌ |
| Runtime 10-30s | ✅ | ✅ | ✅ |
| Runtime > 60s | ❌ | ❌ | ✅ |
| Last seen < 7 days | ✅ | ✅ | ✅ |
| Last seen > 30 days | ❌ | ✅ | ⚠️ |

✅ = Good fit | ⚠️ = Maybe | ❌ = Not recommended

---

## Example Query Evaluations

### Example 1: Dashboard Query
```
Execution count: 250
Runtime CV: 0.15
Avg runtime: 5 seconds
Result size: 50 MB
Last seen: Yesterday
Total cost: 2.5 credits
```
**Decision**: ✅ **CACHE** - Perfect candidate! High frequency, stable, fast, recent
**Priority**: HIGH - implement immediately
**Expected savings**: ~2.4 credits + improved UX

---

### Example 2: Large Analytics Query
```
Execution count: 3
Runtime CV: 0.4
Avg runtime: 120 seconds
Result size: 2 GB
Last seen: 5 days ago
Total cost: 12 credits
```
**Decision**: ✅ **BATCH_TRANSPORT** - Too large and infrequent for cache
**Priority**: LOW - schedule weekly export
**Expected approach**: Automated nightly export to S3, load into DuckDB

---

### Example 3: Ad-Hoc Exploration Query
```
Execution count: 15
Runtime CV: 0.8 (highly variable)
Avg runtime: 8 seconds
Result size: 200 MB
Last seen: Yesterday
Total cost: 0.5 credits
```
**Decision**: ✅ **ARROW_EXTENSION** - Variable results, not worth caching
**Priority**: N/A - use extension directly
**Reasoning**: High CV suggests data changes frequently or query varies significantly

---

## Advanced Tips

### Adjusting Result Size Estimation
The default assumes 100 bytes/row. To calibrate:

1. Sample actual result sizes:
   ```sql
   SELECT
       query_id,
       rows_produced,
       bytes_sent_over_the_network,
       bytes_sent_over_the_network / rows_produced as bytes_per_row
   FROM query_history
   WHERE rows_produced > 0
   LIMIT 1000;
   ```

2. Update the calculation in `02_analyze_queries.sql`:
   ```sql
   AVG(ROWS_PRODUCED * <your_avg_bytes_per_row>) as estimated_result_size_bytes
   ```

### Tuning Thresholds
If you're getting:
- **Too few cache candidates**: Relax thresholds (increase CV max, decrease exec count min)
- **Too many cache candidates**: Tighten thresholds (decrease CV max, increase exec count min)
- **Wrong classifications**: Adjust size/runtime limits based on your infrastructure

### Cost-Benefit Analysis
For each cache candidate:
```
ROI = (potential_savings_credits × $price_per_credit × expected_hit_rate) /
      (S3_storage_cost + DuckDB_compute_cost)
```

Target ROI > 10:1 for first cache implementations.

---

## Questions?

If metrics seem off:
1. Check your warehouse size assumptions (affects cost calculations)
2. Verify the bytes_per_row estimation
3. Ensure ACCOUNT_USAGE data is complete (requires ACCOUNTADMIN access)
4. Review query normalization logic (are similar queries being grouped?)
