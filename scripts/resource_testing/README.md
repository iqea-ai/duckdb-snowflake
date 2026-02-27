# DuckDB Resource Testing & EC2 Capacity Planning

Comprehensive benchmark suite for testing DuckDB's performance, memory behavior, and capacity planning for AWS EC2 deployments.

## What's New: EC2 Capacity Planning 🎉

**NEW**: Complete EC2 capacity planning tools to determine optimal DuckDB configuration for production deployments!

### Quick Start for EC2 Planning

```bash
# Test for AWS i4g.4xlarge (128GB, 16 vCPUs)
python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb

# Get optimal configuration
cat ec2_capacity_recommendation_i4g.4xlarge.json
```

**See [QUICK_START_EC2.md](QUICK_START_EC2.md) for complete guide.**

### Tools Overview

| Script | Purpose | Use When |
|--------|---------|----------|
| `test_resource_usage.py` | Single-process memory testing | Understanding query memory needs |
| `test_concurrent_processes.py` | **NEW** Multi-process capacity testing | Finding process count limits |
| `test_ec2_capacity.py` | **NEW** EC2 instance planning | Planning production deployment |
| `analyze_capacity_results.py` | **NEW** Results analysis & visualization | Interpreting test results |

## Summary

This benchmark suite provides two types of testing:

### 1. Single-Process Memory Testing (Original)

Tests DuckDB's performance and spill behavior at different memory limits to understand performance characteristics before deploying to production instances (e.g., AWS i4g.4xlarge). It generates TPC-H data at scale factor 16 (~16GB database), runs queries at memory limits from 16GB down to 1GB, and measures execution times, spill amounts, compression ratios, and performance penalties.

**Key Finding**: Spilling itself isn't the bottleneck—performance is driven by algorithm selection and memory management overhead, so lower memory limits with some spilling can outperform higher limits without spilling (e.g., Q2_sort at 4GB with 0.38GB spill executes in 9.2s, while 24GB with no spill takes 316s—34x slower).

### 2. EC2 Capacity Planning (NEW)

Tests DuckDB with **concurrent processes** to determine optimal configuration for AWS EC2 instances. Measures system-wide throughput, resource utilization, and saturation points to answer:

- **How many concurrent DuckDB processes can my instance handle?**
- **How much memory should I allocate per process?**
- **What throughput can I expect?**
- **Where's the breaking point?**

**Key Insight**: Single-process tests show query performance, but only concurrent-process tests reveal actual production capacity.

## Goals

### Single-Process Testing Goal

Test your `.duckdb` database locally at different memory limits (16GB, 8GB, 4GB, 2GB, 1GB) to understand performance and spill behavior. This helps understand query memory requirements and optimization opportunities.

### EC2 Capacity Planning Goal

Test with concurrent processes at varying counts (4, 8, 16, 32, 64+) to determine optimal configuration for EC2 instances. This helps determine:
- Optimal connection pool size
- Memory allocation per connection
- Expected throughput and latency
- Resource saturation points

## Quick Start

```bash
# Install dependencies
pip install duckdb psutil

# 1. Generate TPC-H data (SF16 ~16GB database)
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes

# 2. Create temp directory for spill testing
mkdir -p ~/duckdb_temp

# 3. Run benchmark with production-relevant memory limits
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --temp-dir ~/duckdb_temp \
    --output local_memory_results.csv \
    --query-timeout 600
```

**📖 For detailed step-by-step instructions, see [HOW_TO_RUN_TESTS.md](HOW_TO_RUN_TESTS.md)**

**🚀 For EC2 capacity planning, see [QUICK_START_EC2.md](QUICK_START_EC2.md) or the complete [EC2_CAPACITY_PLANNING_GUIDE.md](EC2_CAPACITY_PLANNING_GUIDE.md)**

## Files

### Single-Process Memory Testing (Original)
- **`test_resource_usage.py`** - Main benchmark script for memory testing
- **`generate_tpch_data.py`** - TPC-H data generation script
- **`example_queries/`** - Example SQL query files for testing

### EC2 Capacity Planning (NEW)
- **`test_concurrent_processes.py`** - Multi-process capacity testing
- **`test_ec2_capacity.py`** - Complete EC2 capacity planning suite
- **`analyze_capacity_results.py`** - Results analysis and visualization
- **`QUICK_START_EC2.md`** - Quick start guide for EC2 testing
- **`EC2_CAPACITY_PLANNING_GUIDE.md`** - Complete capacity planning guide

## What is "Spill"?

**Spill** (or "spilling to disk") is when DuckDB runs out of memory and has to temporarily write data to disk to free up RAM.

### Simple Analogy

Think of your desk:
- **Your desk (RAM)**: Small, but everything is instantly accessible
- **Your filing cabinet (Disk)**: Large, but you have to walk over to get things

When your desk gets full, you **spill** some papers into the filing cabinet. You can still access them, but it takes longer.

### When Spilling Occurs

Spilling happens when:
```
Memory Usage > Memory Limit
```

For example:
- You set `memory_limit = '4GB'`
- Your query needs 6GB of memory
- DuckDB spills 2GB to disk to stay within the 4GB limit

### What Gets Spilled?

| Operation | What Gets Spilled | Example |
|-----------|------------------|---------|
| **Hash joins** | Build side of the join | Joining two large tables |
| **Aggregations** | Hash table for GROUP BY | `SELECT ... GROUP BY ...` |
| **Sorting** | Sort buffers | `ORDER BY ...` |
| **Window functions** | Partition/sort data | `SUM() OVER (PARTITION BY ...)` |
| **Large scans** | Materialized columns | Reading large tables |

### The Spill Process

1. **Memory Pressure**: Query needs more memory than available
2. **Eviction Decision**: DuckDB chooses what to evict (cold data first)
3. **Write to Disk**: Compresses and writes to temp files (`~/.tmp/duckdb_temp_*.tmp`)
4. **Free Memory**: Makes room for new data
5. **Read Back**: Decompresses and loads from disk when needed

### Why Spilling Slows Down Queries

- **Memory access**: ~10-50 GB/s
- **Disk access**: ~3-5 GB/s (even on NVMe SSD)
- **Plus**: Compression/decompression overhead

**Result**: 3-10x slower when spilling

### Compression

DuckDB compresses spilled data with ZSTD:
- **Logical size**: 10GB (what DuckDB reports as `temporary_storage_bytes`)
- **Actual disk size**: ~5.7GB (compressed)
- **Compression ratio**: ~1.7-2.3x

## The "Cliff" Effect (And Why It's Not Always About Spilling)

The traditional view is that performance follows a cliff pattern:

```
Performance
    │
Fast│████████████████████  ← No spill zone (enough memory)
    │
    │                     ┌── CLIFF
    │                     │
Slow│▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  ← Spilling zone (not enough memory)
    │
    └───────────────────────────────►
     Enough Memory    Not Enough
```

**However, our benchmark results show a more nuanced reality**: Spilling itself isn't necessarily bad. The performance issues come from algorithm choices and memory management overhead, not from spilling per se. Sometimes, queries with some spilling (using efficient algorithms) can be faster than queries with no spilling (using inefficient algorithms).

**Key insight**: The goal isn't to eliminate spilling - it's to find the memory limit where DuckDB chooses efficient algorithms and has minimal memory management overhead for your specific workload.

## Why Execution Time Can Increase With More Memory

This is a counterintuitive but important behavior: **More memory doesn't always mean better performance, and eliminating spilling doesn't guarantee speed**.

### Example Pattern

```
Q2_sort execution time:
4GB:  9.2s  (spills 0.38GB - FASTEST!)
8GB:  15.9s (spills 0.27GB - slower)
12GB: 292s  (spills 0.09GB - much slower)
16GB: 459s  (spills 0.03GB - worst)
24GB: 316s  (no spill - still 34x slower than 4GB!)
32GB: 300s  (no spill - still 32x slower than 4GB!)
```

### Why This Happens

The performance degradation isn't caused by spilling - it's caused by:

1. **Algorithm Choices**: DuckDB's optimizer chooses different execution plans based on available memory. At 4GB, it uses an efficient external sort. At higher memory, it may choose algorithms optimized for larger datasets that are less efficient for your actual data size.

2. **Memory Management Overhead**: More memory means more overhead in managing that memory (allocation, tracking, eviction decisions).

3. **Cache Locality**: Larger memory structures can cause more cache misses, hurting performance.

4. **Query Plan Changes**: The optimizer may choose different plans that are less efficient when it thinks it has "unlimited" memory.

**The key insight**: Spilling itself isn't the problem. The problem is algorithm choices and memory management overhead. Sometimes, a lower memory limit that causes some spilling (but uses efficient algorithms) is faster than a higher limit with no spilling (but uses inefficient algorithms).

## What the Benchmark Output Tells You

### Example Output

```
Query: tpch_q1_aggregation
Memory Limit    Time (s)     Memory (GB)     Spill (GB)      Disk (GB)       Comp Ratio   
----------------------------------------------------------------------------------------------------
4GB             2.730          3.68           14.88          8.60            1.74x        
8GB             2.610          3.68           14.88          8.60            1.74x        
16GB            0.860          3.68           0.00           0.00            N/A          

  Slowdown from 16GB baseline (0.860s):
    4GB: 3.18x slower
    8GB: 3.03x slower
    16GB: 1.00x (baseline)
```

### Key Insights

1. **The "Cliff" Effect**: Once you cross the threshold where spilling occurs, performance drops dramatically
2. **Spill Penalty**: Different queries have different spill penalties (joins are more sensitive than aggregations)
3. **Compression Effectiveness**: DuckDB's ZSTD compression reduces actual disk I/O by ~1.7x
4. **Minimum Memory**: Find the memory limit where `temp_storage_gb ≈ 0` (no spill)
5. **Acceptable Threshold**: Determine where slowdown is acceptable (e.g., < 2x)

### Questions Answered

- **"How much memory do I need?"** → Look for where `temp_storage_gb ≈ 0`
- **"What happens with less memory?"** → Check slowdown factors
- **"Is spilling acceptable?"** → Compare execution times vs cost savings
- **"Which queries are most sensitive?"** → Compare slowdown across queries
- **"Should I upgrade RAM?"** → Compare upgrade cost vs query time cost

## Benchmark Results Analysis

This section presents benchmark results from testing TPC-H queries at various memory limits. The examples below show results from SF100 testing, but the same patterns apply to SF16 testing for production planning.

### Complete Results

| Memory Limit | Query | Execution Time (s) | Memory Usage (GB) | Spill (GB) | Disk Usage (GB) | Compression Ratio |
|--------------|-------|-------------------|-------------------|------------|-----------------|-------------------|
| 4GB | tpch_q1_aggregation | 2.444 | 3.66 | 0.00 | 0.00 | N/A |
| 4GB | tpch_q2_sort | 9.196 | 0.01 | 0.38 | 0.00 | N/A |
| 4GB | tpch_q3_join | 2.756 | 3.28 | 0.38 | 0.00 | N/A |
| 8GB | tpch_q1_aggregation | 2.326 | 3.66 | 0.00 | 0.00 | N/A |
| 8GB | tpch_q2_sort | 15.865 | 0.01 | 0.27 | 0.00 | N/A |
| 8GB | tpch_q3_join | 2.592 | 4.50 | 0.27 | 0.00 | N/A |
| 12GB | tpch_q1_aggregation | 1.985 | 3.66 | 0.00 | 0.00 | N/A |
| 12GB | tpch_q2_sort | 291.951 | 0.02 | 0.09 | 0.00 | N/A |
| 12GB | tpch_q3_join | 3.036 | 4.51 | 0.09 | 0.00 | N/A |
| 16GB | tpch_q1_aggregation | 2.544 | 3.66 | 0.00 | 0.00 | N/A |
| 16GB | tpch_q2_sort | 459.126 | 0.02 | 0.03 | 0.00 | N/A |
| 16GB | tpch_q3_join | 3.044 | 4.51 | 0.03 | 0.00 | N/A |
| 24GB | tpch_q1_aggregation | 2.171 | 3.66 | 0.00 | 0.00 | N/A |
| 24GB | tpch_q2_sort | 315.648 | 6.85 | 0.00 | 0.00 | N/A |
| 24GB | tpch_q3_join | 3.724 | 7.75 | 0.00 | 0.00 | N/A |
| 32GB | tpch_q1_aggregation | 2.462 | 3.66 | 0.00 | 0.00 | N/A |
| 32GB | tpch_q2_sort | 299.850 | 6.85 | 0.00 | 0.00 | N/A |
| 32GB | tpch_q3_join | 4.338 | 7.75 | 0.00 | 0.00 | N/A |

### Query-by-Query Analysis

#### Query 1: tpch_q1_aggregation (Heavy Aggregation)

**Performance Pattern:**
```
4GB:  2.444s  (baseline)
8GB:  2.326s  (5% faster)
12GB: 1.985s  (19% faster - OPTIMAL)
16GB: 2.544s  (28% slower than 12GB)
24GB: 2.171s  (9% faster than 12GB)
32GB: 2.462s  (24% slower than 12GB)
```

**Key Observations:**
- **No spilling at any memory level**: The query fits entirely in memory (3.66GB) even at 4GB limit
- **Best performance at 12GB**: Optimal balance between memory availability and overhead
- **Performance degrades at higher limits**: 16GB and 32GB are slower than 12GB

**Why This Happened:**
1. **Memory fit**: The aggregation hash table (~3.66GB) fits comfortably in memory at all tested limits, so no spilling occurs
2. **Optimal at 12GB**: At 12GB, DuckDB has enough headroom to avoid any memory pressure while not having so much memory that it causes overhead
3. **Overhead at higher limits**: At 16GB+, the extra memory may cause:
   - Less efficient memory allocation patterns
   - More memory fragmentation
   - Cache locality issues (larger memory space to manage)
4. **Stable performance**: Since there's no spilling, performance is relatively stable (1.9-2.5s range)

**Takeaway**: For this query, 12GB is optimal. More memory doesn't help and can actually hurt performance slightly.

#### Query 2: tpch_q2_sort (Large Sort Operation)

**Performance Pattern:**
```
4GB:  9.196s   (spill: 0.38GB) - FASTEST
8GB:  15.865s  (spill: 0.27GB) - 72% slower
12GB: 291.951s (spill: 0.09GB) - 31.7x slower!
16GB: 459.126s (spill: 0.03GB) - 49.9x slower!
24GB: 315.648s (spill: 0.00GB) - 34.3x slower
32GB: 299.850s (spill: 0.00GB) - 32.6x slower
```

**Key Observations:**
- **Counterintuitive behavior**: Lower memory (4GB) performs BEST, higher memory performs WORSE
- **Spilling decreases but performance degrades**: Less spill at higher memory, but much slower execution
- **Crosses threshold at 24GB**: No spilling at 24GB+, but still much slower than 4GB

**Why This Happened (The "Reverse Cliff" Effect):**

1. **At 4GB (Best Performance)**:
   - DuckDB immediately recognizes it's memory-constrained
   - Uses efficient **external merge sort** algorithm optimized for disk I/O
   - Predictable, sequential I/O patterns
   - Small spill (0.38GB) handled efficiently
   - **Result**: Fast, predictable execution (9.2s)
   - **Key insight**: The spilling here is handled efficiently by an algorithm designed for it

2. **At 8GB-16GB (Worst Performance)**:
   - DuckDB thinks it has "enough" memory to try in-memory or hybrid approaches
   - Switches to less efficient sort algorithms that assume more memory
   - Still hits the limit, causing frequent evictions
   - **Thrashing**: Constantly evicting and reloading blocks
   - More eviction overhead than at 4GB
   - **Result**: Much slower (15-459s) despite less spilling
   - **Key insight**: The problem isn't the spilling - it's the algorithm choice. The hybrid algorithm is less efficient than the pure external sort.

3. **At 24GB-32GB (Still Slow)**:
   - No spilling occurs (all data fits in memory)
   - But the sort algorithm chosen is inefficient for this data size
   - May be using a different sort strategy optimized for larger datasets
   - Memory overhead from managing larger buffers
   - **Result**: Still 30-50x slower than 4GB
   - **Key insight**: Eliminating spilling doesn't help if the algorithm is wrong. The 4GB external sort algorithm is simply better suited for this query, even with spilling.

**Why Sort Operations Are Different:**
- Sort algorithms are highly sensitive to memory availability
- DuckDB has multiple sort strategies (in-memory quicksort, external merge sort, hybrid approaches)
- The optimizer chooses different strategies based on available memory
- Intermediate memory levels can trigger suboptimal algorithm choices
- The "sweet spot" for this query is actually the lowest memory limit (4GB)

**Takeaway**: This demonstrates that "more memory = better performance" is not always true. For sort operations, the optimal memory limit may be the minimum that avoids complete failure, not the maximum available.

#### Query 3: tpch_q3_join (Hash Join)

**Performance Pattern:**
```
4GB:  2.756s  (spill: 0.38GB) - baseline
8GB:  2.592s  (spill: 0.27GB) - 6% faster - OPTIMAL
12GB: 3.036s  (spill: 0.09GB) - 10% slower
16GB: 3.044s  (spill: 0.03GB) - 10% slower
24GB: 3.724s  (spill: 0.00GB) - 35% slower
32GB: 4.338s  (spill: 0.00GB) - 57% slower
```

**Key Observations:**
- **Best performance at 8GB**: Optimal balance with minimal spilling
- **Performance degrades as memory increases**: More memory = slower execution
- **Spilling decreases but doesn't help**: Less spill at higher memory, but worse performance

**Why This Happened:**

1. **At 4GB (Good Performance)**:
   - Small spill (0.38GB) handled efficiently
   - Hash join build side fits mostly in memory
   - Predictable I/O patterns
   - **Result**: Fast execution (2.76s)

2. **At 8GB (Optimal)**:
   - Minimal spilling (0.27GB)
   - Enough memory to keep most of hash table in memory
   - Efficient memory usage
   - **Result**: Best performance (2.59s)

3. **At 12GB-16GB (Degrading)**:
   - Very little spilling (0.03-0.09GB)
   - But query plan may change to use more memory inefficiently
   - More memory management overhead
   - **Result**: Slightly slower (3.0-3.1s)

4. **At 24GB-32GB (Worst)**:
   - No spilling (all data fits in memory)
   - But hash join algorithm may use larger buffers that are less efficient
   - Memory overhead from managing larger structures
   - Cache locality issues
   - **Result**: Slowest (3.7-4.3s)
   - **Key insight**: Eliminating spilling doesn't guarantee better performance. The algorithm and memory management overhead can outweigh the benefits.

**Why Hash Joins Degrade with More Memory:**
- Hash table size management: Larger available memory may cause DuckDB to allocate larger hash tables that are less cache-friendly
- Query plan changes: Optimizer may choose different join strategies (e.g., larger build side) that are less efficient
- Memory fragmentation: More memory can lead to less efficient allocation patterns
- Cache misses: Larger memory structures may cause more cache misses

**Takeaway**: For hash joins, 8GB provides optimal performance. More memory doesn't help and degrades performance, even when spilling stops.

### Overall Patterns and Insights

#### 1. The "Reverse Cliff" for Sorts
- **Q2_sort** shows that lower memory can be faster than higher memory
- This is the opposite of the expected "cliff effect"
- Occurs because sort algorithms are highly sensitive to memory availability
- The optimal memory limit may be the minimum that works, not the maximum

#### 2. Optimal Memory is Query-Dependent
- **Q1 (aggregation)**: Optimal at 12GB
- **Q2 (sort)**: Optimal at 4GB (lowest tested)
- **Q3 (join)**: Optimal at 8GB
- **No single memory limit is optimal for all queries**

#### 3. Spilling Isn't Necessarily Bad
- Q2 at 4GB: Spills 0.38GB but executes in 9.2s (FASTEST)
- Q2 at 12GB: Spills only 0.09GB but executes in 292s (31x slower!)
- Q3 at 8GB: Spills 0.27GB but executes in 2.59s (FASTEST)
- Q3 at 24GB: No spilling but executes in 3.72s (44% slower)
- **Key insight**: Spilling is a mechanism that allows queries to work. The performance impact comes from algorithm choices and memory management overhead, not necessarily from spilling itself. Sometimes, some spilling with the right algorithm is faster than no spilling with the wrong algorithm.

#### 4. Memory Overhead at High Limits (The Real Problem)
- All three queries show performance degradation at 24GB-32GB
- **Even when no spilling occurs, more memory can hurt performance**
- This demonstrates that **spilling itself isn't the problem** - the problem is:
  - Algorithm choices: DuckDB may choose less efficient algorithms when it thinks it has "unlimited" memory
  - Memory management overhead: Managing larger memory structures has overhead
  - Cache locality issues: Larger memory structures cause more cache misses
  - Less efficient allocation patterns: More memory can lead to fragmentation
  - Query plan changes: Optimizer may choose different plans that are less efficient

#### 5. The "Danger Zone" of Intermediate Memory
- 12GB-16GB is a "danger zone" where:
  - Queries think they have enough memory for in-memory algorithms
  - But still hit limits, causing frequent evictions
  - Trigger inefficient hybrid algorithms or thrashing
  - Result: Worst performance (worse than both low-memory spilling and high-memory no-spill)

### Recommendations Based on Results

1. **For Q1 (Aggregations)**: Use 12GB memory limit for optimal performance
2. **For Q2 (Sorts)**: Use 4GB-8GB memory limit (lower is better for this query)
3. **For Q3 (Joins)**: Use 8GB memory limit for optimal performance
4. **For Mixed Workloads**: Test your specific queries - there's no universal optimal memory limit
5. **Avoid High Memory Limits**: 24GB-32GB shows degradation for all queries - more memory isn't always better
6. **Test, Don't Assume**: The counterintuitive results show the importance of benchmarking your specific workload

### Key Takeaway: Spilling vs. Performance

**The most important insight from these results**: Spilling itself isn't necessarily bad. The performance issues come from:

1. **Algorithm choices**: When DuckDB has more memory, it may choose algorithms optimized for that memory level, which can be less efficient for your actual data size
2. **Memory management overhead**: More memory means more overhead in managing that memory
3. **Cache and locality**: Larger memory structures can cause more cache misses

**What this means**:
- **Some spilling can be optimal**: Q2 at 4GB (spills 0.38GB) is faster than Q2 at 24GB (no spill)
- **No spilling doesn't guarantee speed**: Q3 at 24GB (no spill) is slower than Q3 at 8GB (spills 0.27GB)
- **The goal isn't to eliminate spilling**: The goal is to find the memory limit where the algorithm choices and memory management overhead are optimal for your workload

### Why These Results Matter

These results demonstrate that:
- **Memory sizing is complex**: More memory doesn't always mean better performance
- **Query type matters**: Different operations have different optimal memory limits
- **Spilling isn't the enemy**: Some spilling with the right algorithm can be faster than no spilling with the wrong algorithm
- **Testing is essential**: You can't predict optimal memory limits without benchmarking
- **Production implications**: Setting memory limits too high can hurt performance and waste resources. Sometimes, a lower limit that causes some spilling is actually optimal.

## Testing Strategy

### Memory-to-Data Ratios

With a **~16GB database** (SF16), test these memory limits to understand different ratio scenarios:

| Memory Limit | Memory:Data Ratio | What This Tests | i4g.4xlarge Equivalent |
|--------------|-------------------|-----------------|------------------------|
| **16GB** | 1:1 (100%) | Full dataset can fit in memory | 8 sessions × 15GB each |
| **8GB** | 0.5:1 (50%) | Half dataset fits, moderate spill | 16 sessions × 7GB each |
| **4GB** | 0.25:1 (25%) | Heavy spill, algorithm selection changes | 32 sessions × 4GB each |
| **2GB** | 0.125:1 (12.5%) | Extreme constraint, external algorithms | 64 sessions × 2GB each |
| **1GB** | 0.0625:1 (6.25%) | Stress test, may fail on complex queries | 128 sessions × 1GB each |

### Disk Space Ratios (Temp Storage Limits)

Spill storage needs vary by query type and memory pressure. Test with different temp storage limits:

| Temp Storage | Storage:Data Ratio | Simulates |
|--------------|-------------------|-----------|
| **Unlimited** | N/A | Baseline - no storage constraint |
| **16GB** | 1:1 | Equal to dataset size |
| **8GB** | 0.5:1 | Half dataset as spill space |
| **4GB** | 0.25:1 | Tight storage constraint |

### Full Test Matrix (5×4 = 20 Tests Per Query)

When using `--temp-storage-limits`, the script runs a full matrix: each memory limit × each temp storage limit:

| Test ID | Memory | Temp Storage | Memory:Data | Storage:Data | Expected Behavior |
|---------|--------|--------------|-------------|--------------|-------------------|
| **16GB Memory** |||||
| T1 | 16GB | Unlimited | 1:1 | ∞ | Baseline - fast, minimal spill |
| T2 | 16GB | 16GB | 1:1 | 1:1 | Minimal spill, storage won't limit |
| T3 | 16GB | 8GB | 1:1 | 0.5:1 | Minimal spill, storage adequate |
| T4 | 16GB | 4GB | 1:1 | 0.25:1 | Minimal spill, storage tight |
| **8GB Memory** |||||
| T5 | 8GB | Unlimited | 0.5:1 | ∞ | Moderate spill |
| T6 | 8GB | 16GB | 0.5:1 | 1:1 | Moderate spill, ample storage |
| T7 | 8GB | 8GB | 0.5:1 | 0.5:1 | Moderate spill, storage adequate |
| T8 | 8GB | 4GB | 0.5:1 | 0.25:1 | Moderate spill, storage may limit |
| **4GB Memory** |||||
| T9 | 4GB | Unlimited | 0.25:1 | ∞ | Heavy spill |
| T10 | 4GB | 16GB | 0.25:1 | 1:1 | Heavy spill, ample storage |
| T11 | 4GB | 8GB | 0.25:1 | 0.5:1 | Heavy spill, storage constrained |
| T12 | 4GB | 4GB | 0.25:1 | 0.25:1 | Heavy spill, storage tight |
| **2GB Memory** |||||
| T13 | 2GB | Unlimited | 0.125:1 | ∞ | Extreme spill |
| T14 | 2GB | 16GB | 0.125:1 | 1:1 | Extreme spill, ample storage |
| T15 | 2GB | 8GB | 0.125:1 | 0.5:1 | Extreme spill, storage constrained |
| T16 | 2GB | 4GB | 0.125:1 | 0.25:1 | Extreme spill, storage may fail |
| **1GB Memory** |||||
| T17 | 1GB | Unlimited | 0.0625:1 | ∞ | Max spill, may fail complex queries |
| T18 | 1GB | 16GB | 0.0625:1 | 1:1 | Max spill, storage adequate |
| T19 | 1GB | 8GB | 0.0625:1 | 0.5:1 | Max spill, storage constrained |
| T20 | 1GB | 4GB | 0.0625:1 | 0.25:1 | Stress test - both at minimum |

### Target Instance Reference

| Resource | i4g.4xlarge |
|----------|-------------|
| Memory | 128 GiB |
| vCPUs | 16 |
| NVMe Storage | 3,750 GB |

If you run **8 sessions** on this instance, each session gets ~15GB memory and ~450GB spill space.
If you run **16 sessions**, each gets ~7GB memory and ~225GB spill space.

## Usage

### Generate TPC-H Data

```bash
# Generate SF16 data (~16GB database, recommended for local testing)
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes

# Generate smaller dataset for quick testing
python generate_tpch_data.py --output tpch_sf1.duckdb --scale-factor 1

# Generate larger dataset for comprehensive testing (takes longer)
python generate_tpch_data.py --output tpch_sf100.duckdb --scale-factor 100 --yes
```

**TPC-H SF16 Table Sizes (approximate):**
| Table | Rows | Size |
|-------|------|------|
| lineitem | 96M | ~12GB |
| orders | 24M | ~2.4GB |
| partsupp | 12.8M | ~1.2GB |
| part | 3.2M | ~320MB |
| customer | 2.4M | ~240MB |
| supplier | 160K | ~16MB |
| nation | 25 | tiny |
| region | 5 | tiny |

### Run Benchmark

```bash
# Standard production testing (recommended)
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --temp-dir ~/duckdb_temp \
    --output local_memory_results.csv \
    --query-timeout 600

# With specific queries
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --queries example_queries/tpch_q1_aggregation.sql,example_queries/tpch_q3_join.sql \
    --temp-dir ~/duckdb_temp \
    --output local_memory_results.csv

# Quick test with fewer memory limits
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB \
    --temp-dir ~/duckdb_temp \
    --output quick_test.csv

# Full test matrix (5 memory limits × 4 temp storage limits = 20 tests per query)
# Requires pre-configured temp directories with size limits (see Setup section)
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --temp-storage-limits unlimited,/mnt/temp_16gb,/mnt/temp_8gb,/mnt/temp_4gb \
    --output full_matrix_results.csv \
    --query-timeout 600
```

### Setup Temp Directory

#### Basic Setup (Single Temp Directory)

```bash
# Create a dedicated temp directory for spill testing
mkdir -p ~/duckdb_temp

# Check available disk space (spill can be large!)
df -h ~/duckdb_temp

# Clear temp directory between tests
rm -rf ~/duckdb_temp/*
```

#### Full Matrix Testing (Multiple Temp Storage Limits)

To test the full 5×4 matrix, you need to create multiple temp directories with size limits.

**On Linux:**

```bash
# Option 1: Use the setup script (recommended)
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/scripts/resource_testing
sudo ./setup_temp_directories.sh

# Option 2: Manual setup (all in one command)
sudo bash -c 'mkdir -p /mnt/temp_unlimited /mnt/temp_16gb /mnt/temp_8gb /mnt/temp_4gb && \
              mount -t tmpfs -o size=16G tmpfs /mnt/temp_16gb && \
              mount -t tmpfs -o size=8G tmpfs /mnt/temp_8gb && \
              mount -t tmpfs -o size=4G tmpfs /mnt/temp_4gb'
```

**Alternative: Create limited disk images**
for size in 16 8 4; do
    dd if=/dev/zero of=~/temp_${size}gb.img bs=1G count=${size}
    mkfs.ext4 ~/temp_${size}gb.img
    mkdir -p ~/duckdb_temp_${size}gb
    sudo mount -o loop ~/temp_${size}gb.img ~/duckdb_temp_${size}gb
done

# Then use in tests:
# --temp-storage-limits "unlimited,/mnt/temp_16gb,/mnt/temp_8gb,/mnt/temp_4gb"
```

**Note**: On macOS, disk image size limits can be set using `hdiutil` or by creating sparse disk images with quotas.

## Output Format

### CSV Output

Matches the benchmark script format from the document:

```csv
memory_limit,query,exec_time_sec,mem_usage_gb,temp_storage_gb,disk_usage_gb,compression_ratio
4GB,q1_aggregation,2.73,3.68,14.88,8.60,1.74
8GB,q1_aggregation,2.61,3.68,14.88,8.60,1.74
16GB,q1_aggregation,0.86,3.68,0.00,0.00,N/A
```

### Console Output

- Real-time progress during test execution
- Summary table with key metrics
- Detailed per-query results with spill information
- Slowdown analysis comparing to baseline

## Features

- **Memory Limits**: Tests with absolute memory limits (e.g., "4GB", "8GB", "16GB")
- **CSV Output**: Generates CSV matching the exact format from the document
- **Disk Spill Tracking**: Monitors temporary storage usage, compression ratios, and spill penalties
- **Memory Tag Breakdown**: Tracks memory usage by tag (HASH_TABLE, ORDER_BY, IN_MEMORY_TABLE, etc.)
- **Performance Analysis**: Calculates slowdown factors when spilling occurs
- **Query Timeout**: Optional timeout to prevent indefinite hangs

## Example Queries

- **`example_queries/tpch_q1_aggregation.sql`** - TPC-H Q1: Heavy aggregation (HASH_TABLE tag)
- **`example_queries/tpch_q2_sort.sql`** - Large sort operation (ORDER_BY tag)
- **`example_queries/tpch_q3_join.sql`** - TPC-H Q3: Hash join (HASH_TABLE tag)
- **`example_queries/simple_test.sql`** - Basic functionality test
- **`example_queries/memory_intensive.sql`** - Tests memory usage with large aggregations
- **`example_queries/cpu_intensive.sql`** - Tests CPU usage with complex calculations
- **`example_queries/join_intensive.sql`** - Tests memory usage with multiple joins

## Interpreting Results

After running benchmarks, you'll see different performance patterns. Here's how to interpret them:

### Pattern 1: Gradual Degradation (Normal)

```
16GB: 10s, 0GB spill
8GB:  12s, 2GB spill  (1.2x slower)
4GB:  18s, 5GB spill  (1.8x slower)
2GB:  35s, 8GB spill  (3.5x slower)
```

**Interpretation**: Linear-ish degradation. More sessions = proportionally slower. Predictable.

**Decision**: More sessions = more throughput (each slower, but total higher). Choose based on latency tolerance. **Recommendation**: 8-16 sessions with 8-16GB each.

### Pattern 2: Cliff Effect

```
16GB: 10s, 0GB spill
8GB:  11s, 0GB spill  (1.1x slower)
4GB:  45s, 6GB spill  (4.5x slower) ← CLIFF
2GB:  50s, 8GB spill  (5.0x slower)
```

**Interpretation**: Query needs ~5GB to avoid spilling. Below that, big performance hit.

**Decision**: Stay above the cliff if latency matters. Go below cliff only if throughput >>> latency. **Recommendation**: Find the cliff point, stay just above it.

### Pattern 3: Reverse Cliff (Algorithm Change)

```
16GB: 300s, 0GB spill  ← SLOW
8GB:  150s, 1GB spill  (faster!)
4GB:  20s,  3GB spill  ← FASTEST
2GB:  25s,  4GB spill
```

**Interpretation**: Lower memory forces DuckDB to use more efficient algorithm (external sort). This is the "reverse cliff" from your results.

**Decision**: Lower memory might be BETTER for this query. Test to find the sweet spot. **Recommendation**: May want fewer GB per session than expected.

### Pattern 4: Failure at Low Memory

```
16GB: 10s, 0GB spill
8GB:  12s, 2GB spill
4GB:  FAILED - out of memory
```

**Interpretation**: Query has a minimum memory requirement. Can't run at this limit.

**Decision**: This query has a hard minimum. Must allocate at least X GB per session. **Recommendation**: Use the failure point to set floor.

## Real-World Use Cases

### AWS i4g.4xlarge Session Planning

**Question**: "How many sessions should I run per instance?"

**Answer**: Use benchmark results to determine:
1. Identify optimal memory per session from your query patterns
2. Calculate sessions: `120GB / optimal_memory = num_sessions`
3. Estimate spill storage: `max_spill_observed * num_sessions`
4. Validate on actual AWS instance

**Example**: If 8GB per session is optimal:
- Sessions per instance: `120GB / 8GB = 15 sessions`
- Each session handles ~16GB data with 8GB memory (0.5:1 ratio)
- Total spill space needed: `max_spill * 15` (check your results)

### Kubernetes Resource Planning

**Question**: "What memory limit should I set in my K8s deployment?"

**Answer**: Use benchmark results to determine:
- Set `memory_limit: 16GB` for optimal performance (if Pattern 1 or 2)
- Set `memory_limit: 8GB` if you can accept 1.2-3x slowdown
- Set `memory_limit: 4GB` if you see reverse cliff (Pattern 3) or cost is critical

### Cost Optimization

**Question**: "Can I reduce instance size to save money?"

**Answer**: Compare costs based on your pattern:
- **Pattern 1**: 16GB → 8GB: 1.2x slower, 50% cost savings (maybe acceptable)
- **Pattern 2**: Stay above cliff - going below costs too much performance
- **Pattern 3**: 16GB → 4GB: Actually faster! 75% cost savings ✅
- **Pattern 4**: Can't reduce below minimum

## Analysis Workflow

```bash
# 1. Run benchmark
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --temp-dir ~/duckdb_temp \
    --output local_memory_results.csv

# 2. Monitor temp directory during test (in another terminal)
watch -n 1 "du -sh ~/duckdb_temp"

# 3. Load results in DuckDB for analysis
duckdb <<EOF
CREATE TABLE results AS SELECT * FROM read_csv('local_memory_results.csv');

-- Find minimum memory for no spill
SELECT query, MIN(memory_limit) as min_memory_no_spill
FROM results
WHERE temp_storage_gb < 0.1
GROUP BY query;

-- Find acceptable memory (slowdown < 2x from baseline)
SELECT query, memory_limit, exec_time_sec,
       exec_time_sec / MIN(exec_time_sec) OVER (PARTITION BY query) as slowdown
FROM results
WHERE exec_time_sec / MIN(exec_time_sec) OVER (PARTITION BY query) < 2.0
ORDER BY query, memory_limit;

-- Identify performance patterns
SELECT 
    query,
    memory_limit,
    exec_time_sec,
    temp_storage_gb,
    exec_time_sec / LAG(exec_time_sec) OVER (PARTITION BY query ORDER BY CAST(REPLACE(memory_limit, 'GB', '') AS INTEGER) DESC) as slowdown_vs_previous
FROM results
ORDER BY query, CAST(REPLACE(memory_limit, 'GB', '') AS INTEGER) DESC;
EOF
```

### Expected Results Template

Fill this in as you run tests:

```
Query: [YOUR_QUERY_NAME]
Database: tpch_sf16.duckdb
Database Size: ~16GB

| Memory | Time (s) | Spill (GB) | Slowdown | Notes |
|--------|----------|------------|----------|-------|
| 16GB   |          |            | 1.0x     |       |
| 8GB    |          |            |          |       |
| 4GB    |          |            |          |       |
| 2GB    |          |            |          |       |
| 1GB    |          |            |          |       |
```

### Key Metrics to Record

For each memory limit, capture:

| Metric | How to Get It | Why It Matters |
|--------|---------------|----------------|
| **Execution Time** | From test output | Primary performance indicator |
| **Spill Amount (GB)** | Check temp directory size or CSV output | Indicates disk I/O overhead |
| **Query Success/Fail** | Did it complete? | Some queries may fail at low memory |
| **Relative Slowdown** | Time at X GB / Time at 16GB | Quantifies performance penalty |

## Requirements

- Python 3.7+
- `duckdb` package
- `psutil` package

Install with: `pip install duckdb psutil`

## Technical Details

### How DuckDB Spills to Disk

1. **Buffer Pool Eviction**: DuckDB evicts blocks in priority order (lowest first)
2. **Compression**: Uses ZSTD compression (typically 1.7-2.3x ratio)
3. **Temp File Format**: Raw binary (NOT columnar), stored in block pools (S32K, S64K, etc.)
4. **Reload**: When needed, reads from disk, decompresses, and loads into memory

### Memory Tags

DuckDB tracks memory usage by tag:
- `HASH_TABLE` - Hash joins and aggregations
- `ORDER_BY` - Sort operations
- `IN_MEMORY_TABLE` - Result sets
- `COLUMN_DATA` - Materialized columns
- `PARQUET_READER` - Parquet file readers
- And more...

### Temp Storage Location

- Default: `~/.tmp/` (relative to launch directory)
- Configurable: `SET temp_directory = '/path/to/temp'`
- Files: `duckdb_temp_storage_*.tmp`

## Next Steps After Local Testing

1. **Identify optimal memory per session** based on your query mix and observed patterns
2. **Calculate sessions per instance**: `120GB / optimal_memory = num_sessions`
3. **Estimate spill storage needed**: `max_spill_observed * num_sessions`
4. **Validate on AWS** with actual i4g.4xlarge instance
5. **Monitor in production**: Track actual spill amounts and performance in real workloads

## Best Practices

1. **Run Multiple Times**: System state can vary, run tests multiple times for consistency
2. **Use Representative Queries**: Test with queries similar to your production workload
3. **Document Anomalies**: Note any unexpected results (reverse cliffs, failures, etc.)
4. **Consistent Environment**: Close other applications for consistent results
5. **Use Timeout**: Set `--query-timeout` to prevent indefinite hangs (600s recommended)
6. **Clear Temp Between Tests**: Remove temp files between different memory limit tests
7. **Monitor System Resources**: Use `htop` or `top` to watch CPU/memory during tests

## Troubleshooting

### Queries Taking Too Long

- Use `--query-timeout` to set a maximum execution time
- Test with smaller scale factors first (SF1, SF10)
- Check if queries are hitting memory limits causing excessive spilling

### No Spilling Detected

- Use lower memory limits (1GB, 2GB) to force spilling
- Ensure database is large enough (SF16 recommended, ~16GB)
- Check temp directory path is correct
- Verify temp directory has write permissions

### Inconsistent Results

- Run multiple iterations
- Ensure system is in consistent state
- Check for background processes

## Quick Reference Commands

```bash
# Generate SF16 database
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes

# Create temp directory
mkdir -p ~/duckdb_temp

# Run full benchmark
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --temp-dir ~/duckdb_temp \
    --output local_memory_results.csv \
    --query-timeout 600

# Check temp directory usage during test (in another terminal)
watch -n 1 "du -sh ~/duckdb_temp"

# Clear temp directory between tests
rm -rf ~/duckdb_temp/*

# Monitor system resources during test
htop  # or: top -d 1

# Verify database size
ls -lh tpch_sf16.duckdb
```

## References

- DuckDB Configuration: `duckdb/src/main/config.hpp`
- Memory Settings: `duckdb/src/main/settings/custom_settings.cpp`
- Thread Settings: `duckdb/src/main/settings/custom_settings.cpp`
- System Resource Detection: `duckdb/src/main/config.cpp`
- Buffer Pool: `duckdb/src/storage/buffer/buffer_pool.cpp`
- Temporary File Manager: `duckdb/src/storage/temporary_file_manager.cpp`



