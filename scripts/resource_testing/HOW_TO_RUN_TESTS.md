# How to Run Tests and View Results

This guide walks you through running the DuckDB memory and spill benchmarks step-by-step.

## Step 1: Install Dependencies

```bash
pip install duckdb psutil
```

## Step 2: Generate Test Database

Generate a TPC-H SF16 database (~16GB) for testing:

```bash
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/scripts/resource_testing

# Generate the database (takes a few minutes)
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes
```

**Expected output:**
```
Generating TPC-H data (SF=16)...
Output database: tpch_sf16.duckdb

Installing TPC-H extension...
✓ TPC-H extension loaded
Generating TPC-H data with scale factor 16...
Using parallel generation with 4 workers...
  Step 1/4... ✓ (45.2s)
  Step 2/4... ✓ (42.1s)
  Step 3/4... ✓ (41.8s)
  Step 4/4... ✓ (40.5s)
✓ Data generation completed in 169.6 seconds

Verifying table sizes...
Table            Size          
------------------------------
lineitem         12.34 GB      
orders           2.45 GB       
partsupp         1.23 GB       
...
Total            16.12 GB

✓ TPC-H database created successfully: tpch_sf16.duckdb
```

## Step 3: Create Temp Directory

```bash
# Create temp directory for spill files
mkdir -p ~/duckdb_temp

# Verify you have enough space (spill can be large!)
df -h ~/duckdb_temp
```

## Step 4: Run Basic Benchmark

Run the benchmark with default memory limits (16GB, 8GB, 4GB, 2GB, 1GB):

```bash
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --temp-dir ~/duckdb_temp \
    --output local_memory_results.csv \
    --query-timeout 600
```

**What you'll see:**

### Real-Time Console Output

```
DuckDB Disk Spill Benchmark
================================================================================
Database: tpch_sf16.duckdb
Temp directory: /Users/venkata/duckdb_temp
Memory limits to test: 16GB, 8GB, 4GB, 2GB, 1GB
Queries to test: tpch_q1_aggregation, tpch_q2_sort, tpch_q3_join

================================================================================
Testing with memory_limit = 16GB
================================================================================
  [1/15] Executing query: tpch_q1_aggregation... ✓ (2.171s)
  [2/15] Executing query: tpch_q2_sort... ✓ (8.234s)
  [3/15] Executing query: tpch_q3_join... ✓ (2.456s, spill: 0.00GB)

================================================================================
Testing with memory_limit = 8GB
================================================================================
  [4/15] Executing query: tpch_q1_aggregation... ✓ (2.326s)
  [5/15] Executing query: tpch_q2_sort... ✓ (9.196s, spill: 0.38GB)
  [6/15] Executing query: tpch_q3_join... ✓ (2.592s, spill: 0.27GB)

... (continues for all memory limits)
```

### Summary Table (at the end)

```
================================================================================
BENCHMARK SUMMARY
================================================================================

Query: tpch_q1_aggregation
Memory Limit    Time (s)     Memory (GB)     Spill (GB)      Disk (GB)       Comp Ratio   
----------------------------------------------------------------------------------------------------
16GB            2.171        3.66            0.00            0.00            N/A          
8GB             2.326        3.66            0.00            0.00            N/A          
4GB             2.444        3.66            0.00            0.00            N/A          
2GB             3.125        3.66            0.00            0.00            N/A          
1GB             4.892        3.66            0.00            0.00            N/A          

  Slowdown from 16GB baseline (2.171s):
    8GB: 1.07x slower
    4GB: 1.13x slower
    2GB: 1.44x slower
    1GB: 2.25x slower

Query: tpch_q2_sort
Memory Limit    Time (s)     Memory (GB)     Spill (GB)      Disk (GB)       Comp Ratio   
----------------------------------------------------------------------------------------------------
16GB            8.234        0.01            0.00            0.00            N/A          
8GB             9.196        0.01            0.38            0.00            N/A          
4GB             9.196        0.01            0.38            0.00            N/A          
2GB             12.456       0.01            0.75            0.00            N/A          
1GB             18.234       0.01            1.12            0.00            N/A          

  Slowdown from 4GB baseline (9.196s):
    16GB: 0.90x slower (1.11x faster!)
    8GB: 1.00x (baseline)
    4GB: 1.00x (baseline)
    2GB: 1.35x slower
    1GB: 1.98x slower

... (continues for all queries)

Results saved to CSV: local_memory_results.csv
```

## Step 5: View CSV Results

The CSV file contains all the detailed data:

```bash
# View the CSV file
cat local_memory_results.csv

# Or open in a spreadsheet
open local_memory_results.csv  # macOS
# Or: xdg-open local_memory_results.csv  # Linux
```

**CSV Format:**
```csv
memory_limit,query,exec_time_sec,mem_usage_gb,temp_storage_gb,disk_usage_gb,compression_ratio
16GB,tpch_q1_aggregation,2.171,3.66,0.00,0.00,N/A
8GB,tpch_q1_aggregation,2.326,3.66,0.00,0.00,N/A
4GB,tpch_q1_aggregation,2.444,3.66,0.00,0.00,N/A
16GB,tpch_q2_sort,8.234,0.01,0.00,0.00,N/A
8GB,tpch_q2_sort,9.196,0.01,0.38,0.00,N/A
4GB,tpch_q2_sort,9.196,0.01,0.38,0.00,N/A
...
```

## Step 6: Analyze Results

### Quick Analysis with DuckDB

```bash
# Load results into DuckDB for analysis
duckdb <<EOF
CREATE TABLE results AS SELECT * FROM read_csv('local_memory_results.csv');

-- Find minimum memory for no spill per query
SELECT query, MIN(memory_limit) as min_memory_no_spill
FROM results
WHERE temp_storage_gb < 0.1
GROUP BY query;

-- Find queries with best performance at each memory level
SELECT memory_limit, query, exec_time_sec,
       RANK() OVER (PARTITION BY memory_limit ORDER BY exec_time_sec) as rank
FROM results
WHERE exec_time_sec IS NOT NULL
ORDER BY memory_limit, rank;

-- Calculate slowdown factors
SELECT 
    query,
    memory_limit,
    exec_time_sec,
    exec_time_sec / MIN(exec_time_sec) OVER (PARTITION BY query) as slowdown
FROM results
ORDER BY query, CAST(REPLACE(memory_limit, 'GB', '') AS INTEGER) DESC;
EOF
```

### What to Look For

1. **Execution Time**: Primary performance indicator
   - Lower is better
   - Compare across memory limits

2. **Spill Amount**: How much data spilled to disk
   - `temp_storage_gb`: Logical (uncompressed) size
   - `disk_usage_gb`: Actual disk usage (compressed)
   - `compression_ratio`: How much compression helped

3. **Slowdown Factors**: Performance penalty
   - `1.0x` = baseline (fastest)
   - `2.0x` = 2x slower
   - `0.5x` = 2x faster (relative to baseline)

4. **Patterns**:
   - **Gradual degradation**: Linear slowdown as memory decreases
   - **Cliff effect**: Sharp performance drop at a specific memory limit
   - **Reverse cliff**: Lower memory performs better (algorithm change)

## Step 7: Run Full Matrix Test (Optional)

If you want to test with different temp storage limits (requires setup):

```bash
# First, set up limited temp directories (see README for details)
# Then run full matrix:

python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --temp-storage-limits unlimited,/mnt/temp_16gb,/mnt/temp_8gb,/mnt/temp_4gb \
    --output full_matrix_results.csv \
    --query-timeout 600
```

This runs **20 tests per query** (5 memory × 4 temp storage = 20).

## Step 8: Test with Your Own Queries

```bash
# Create your query file
cat > my_query.sql <<EOF
SELECT 
    l_orderkey,
    SUM(l_extendedprice * (1 - l_discount)) AS revenue
FROM lineitem
WHERE l_shipdate >= '1994-01-01'
GROUP BY l_orderkey
ORDER BY revenue DESC
LIMIT 100;
EOF

# Run benchmark with your query
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --memory-limits 16GB,8GB,4GB,2GB,1GB \
    --queries my_query.sql \
    --temp-dir ~/duckdb_temp \
    --output my_query_results.csv
```

## Monitoring During Tests

### Watch Temp Directory Size

In another terminal:

```bash
# Monitor temp directory size in real-time
watch -n 1 "du -sh ~/duckdb_temp && ls -lhS ~/duckdb_temp/*.tmp 2>/dev/null | head -5"
```

### Monitor System Resources

```bash
# Watch CPU and memory usage
htop  # or: top -d 1
```

## Expected Test Duration

- **SF16 database generation**: ~3-5 minutes
- **Basic benchmark** (5 memory limits × 3 queries = 15 tests): ~5-30 minutes depending on your system
- **Full matrix** (5 memory × 4 temp × 3 queries = 60 tests): ~20-120 minutes

## Troubleshooting

### Queries Taking Too Long

```bash
# Increase timeout (default is no timeout)
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --query-timeout 1200  # 20 minutes
```

### Out of Disk Space

```bash
# Check available space
df -h ~/duckdb_temp

# Clear old spill files
rm -rf ~/duckdb_temp/duckdb_temp_*.tmp
```

### Database Not Found

```bash
# Verify database exists
ls -lh tpch_sf16.duckdb

# If missing, regenerate:
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes
```

## Next Steps

After reviewing results:

1. **Identify optimal memory per session** based on your query patterns
2. **Calculate sessions per instance**: `120GB / optimal_memory = num_sessions`
3. **Estimate spill storage needed**: `max_spill_observed * num_sessions`
4. **Validate on AWS** with actual i4g.4xlarge instance

## Quick Reference

```bash
# Generate database
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes

# Run basic benchmark
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --temp-dir ~/duckdb_temp \
    --output results.csv \
    --query-timeout 600

# View results
cat results.csv
open results.csv  # macOS
```




