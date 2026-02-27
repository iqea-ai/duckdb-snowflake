# EC2 Capacity Planning Guide for DuckDB

Complete guide for determining optimal DuckDB configuration for AWS EC2 instances.

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Understanding the Problem](#understanding-the-problem)
4. [Testing Strategy](#testing-strategy)
5. [Running Tests](#running-tests)
6. [Analyzing Results](#analyzing-results)
7. [Deployment Recommendations](#deployment-recommendations)

## Overview

This guide helps you answer:

- **How many concurrent DuckDB processes can my EC2 instance handle?**
- **How much memory should I allocate per process?**
- **What throughput can I expect?**
- **Where's the breaking point?**

### What's Different from Single-Process Testing

| Single-Process Testing | Concurrent Process Testing (This Guide) |
|------------------------|----------------------------------------|
| Tests query performance at different memory limits | Tests **system capacity** with multiple processes |
| Answers: "How fast is this query?" | Answers: "How many processes can I run?" |
| Measures: Query latency, spill behavior | Measures: **Throughput, saturation, total resource usage** |
| Good for: Understanding query behavior | Good for: **Production capacity planning** |

## Quick Start

### For AWS i4g.4xlarge (128GB, 16 vCPUs)

```bash
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/scripts/resource_testing

# 1. Ensure database is ready (already generated)
ls -lh tpch_sf16.duckdb

# 2. Run EC2 capacity test
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb

# 3. View results
cat ec2_capacity_recommendation_i4g.4xlarge.json
```

This will test multiple scenarios and give you a recommendation like:

```
Optimal Process Count: 24
Memory per Process: 4.8GB
Expected Throughput: 156.4 queries/second
Expected P95 Latency: 2.3 seconds
```

### For Quick Testing (Faster)

```bash
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb \
    --quick
```

## Understanding the Problem

### The Core Question

When deploying to EC2, you need to know:

```
Total Available Memory
        ÷
Memory per DuckDB Process
        =
Max Concurrent Processes
```

But it's not that simple because:

1. **OS needs memory** (15-20% overhead)
2. **Processes interfere** with each other (CPU contention, I/O bottlenecks)
3. **Performance degrades** beyond a certain point
4. **Different queries** have different resource needs

### What We're Testing

```
Test Setup:
  - Launch N concurrent DuckDB processes
  - Each runs the same query
  - Measure system-wide metrics

Metrics:
  - Throughput (queries/second)
  - Individual query latency (avg, p95, p99)
  - Total memory usage
  - CPU utilization
  - When does it break?
```

### Performance Patterns

#### Pattern 1: Linear Scaling (Ideal)

```
Processes  Throughput   Notes
4          10 q/s       Baseline
8          20 q/s       Perfect 2x scaling
16         40 q/s       Still scaling well
32         80 q/s       Excellent!
```

**What this means**: Your workload scales perfectly. Go as high as resources allow.

#### Pattern 2: Saturation (Common)

```
Processes  Throughput   Notes
4          10 q/s       Baseline
8          19 q/s       Good scaling
16         34 q/s       Degradation starts
32         42 q/s       Saturated (only 1.2x improvement)
```

**What this means**: Sweet spot is around 8-16 processes. Beyond that, you hit bottlenecks.

#### Pattern 3: Cliff (Resource Limit)

```
Processes  Throughput   Notes
4          10 q/s       Baseline
8          20 q/s       Good
16         35 q/s       Good
32         8 q/s        CLIFF! (OOM or thrashing)
```

**What this means**: Hard limit at 16 processes. Don't exceed this.

## Testing Strategy

### Test Scenarios

The capacity test runs multiple scenarios:

#### 1. Conservative (High Memory per Process)
- **Process counts**: 4, 8, 12
- **Memory allocation**: 20% buffer for OS, 80% for DuckDB
- **Use case**: Complex queries, high memory needs
- **Example**: For 128GB instance → 8 processes × 12.8GB each

#### 2. Balanced (Match vCPU Count)
- **Process counts**: 8, 16, 24 (for 16 vCPU instance)
- **Memory allocation**: 15% buffer for OS
- **Use case**: General workload, balanced performance
- **Example**: For 128GB instance → 16 processes × 7.2GB each

#### 3. Aggressive (High Concurrency)
- **Process counts**: 32, 48, 64
- **Memory allocation**: 10% buffer for OS, maximize processes
- **Use case**: Many small queries, prioritize throughput
- **Example**: For 128GB instance → 64 processes × 1.9GB each

#### 4. Saturation (Find Breaking Point)
- **Process counts**: Incrementally increase until failure
- **Purpose**: Find the hard limit
- **Example**: Test 16, 32, 48, 64, 80, 96... until degradation > 2x

### What Gets Tested

For each scenario:

```
1. Launch N concurrent processes
2. Each process:
   - Connects to DuckDB with memory limit
   - Executes test query
   - Measures latency
3. System monitor tracks:
   - Total memory usage
   - CPU utilization
   - I/O wait
4. Calculate:
   - Throughput (queries/second)
   - Avg/median/p95/p99 latency
   - Degradation factor vs baseline
```

## Running Tests

### Prerequisites

```bash
# Install dependencies
pip install duckdb psutil

# Verify database exists and has data
duckdb tpch_sf16.duckdb -c "SELECT COUNT(*) FROM lineitem;"
# Should return ~72 million rows
```

### Test Commands

#### Test for Specific EC2 Instance Type

```bash
# AWS i4g.4xlarge (128GB, 16 vCPUs, 3750GB NVMe)
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb

# AWS r7g.4xlarge (128GB, 16 vCPUs, memory optimized)
python test_ec2_capacity.py \
    --instance-type r7g.4xlarge \
    --db-path tpch_sf16.duckdb

# AWS m7g.4xlarge (64GB, 16 vCPUs, general purpose)
python test_ec2_capacity.py \
    --instance-type m7g.4xlarge \
    --db-path tpch_sf16.duckdb
```

#### Test with Custom Configuration

```bash
# Custom memory and vCPU count
python test_ec2_capacity.py \
    --memory 128GB \
    --vcpus 16 \
    --db-path tpch_sf16.duckdb
```

#### Test with Custom Queries

```bash
# Use your own queries instead of default TPC-H
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb \
    --queries my_query1.sql,my_query2.sql
```

#### Quick Test (Fewer Scenarios)

```bash
# Run quick test (takes 5-10 minutes instead of 30-60 minutes)
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb \
    --quick
```

### Advanced: Manual Concurrent Process Test

For more control, use the underlying concurrent process script:

```bash
# Test specific process counts
python test_concurrent_processes.py \
    --db-path tpch_sf16.duckdb \
    --process-counts 8,16,24,32,48 \
    --total-memory 128GB \
    --output my_test_results.csv
```

### What to Expect During Testing

```
EC2 Capacity Planning Test Suite
================================================================================
Instance Configuration: 128GB RAM, 16 vCPUs
Database: tpch_sf16.duckdb
Test Mode: Full

Generated 4 test scenario(s):
  - conservative: Conservative: High memory per process, low concurrency
  - balanced: Balanced: Match vCPU count
  - aggressive: Aggressive: High concurrency, lower memory per process
  - saturation: Saturation: Find resource limits

================================================================================
Scenario: conservative
Description: Conservative: High memory per process, low concurrency
================================================================================
Running: python test_concurrent_processes.py ...

  Testing with 4 concurrent processes (25.6GB per process)
  Launching 4 concurrent processes...
    ✓ Completed in 8.45s (avg: 8.32s, p95: 8.41s, throughput: 0.47 q/s)

  Testing with 8 concurrent processes (12.8GB per process)
  Launching 8 concurrent processes...
    ✓ Completed in 8.67s (avg: 8.54s, p95: 8.63s, throughput: 0.92 q/s)

[continues for all scenarios...]
```

### Test Duration

- **Quick mode**: 5-15 minutes
- **Full mode**: 30-90 minutes
- **With custom queries**: Depends on query complexity

## Analyzing Results

### Output Files

After testing, you'll have:

```
capacity_conservative_results.csv      # Conservative scenario results
capacity_balanced_results.csv          # Balanced scenario results
capacity_aggressive_results.csv        # Aggressive scenario results
capacity_saturation_results.csv        # Saturation test results
ec2_capacity_recommendation_i4g.4xlarge.json  # Final recommendation
```

### Reading the Recommendation

```json
{
  "instance_type": "i4g.4xlarge",
  "optimal_process_count": 24,
  "memory_per_process_gb": 4.8,
  "expected_throughput_qps": 156.4,
  "expected_p95_latency_sec": 2.3,
  "memory_utilization_percent": 87.5,
  "cpu_utilization_percent": 78.2,
  "notes": [
    "Moderate memory utilization (75-90%)",
    "CPU utilization is healthy"
  ]
}
```

**How to interpret**:

- **optimal_process_count**: Run this many concurrent DuckDB sessions
- **memory_per_process_gb**: Set `max_memory = '4.8GB'` per session
- **expected_throughput_qps**: You can handle ~156 queries/second
- **expected_p95_latency_sec**: 95% of queries complete in < 2.3s
- **memory_utilization**: Using 87.5% of available memory (good)
- **cpu_utilization**: Using 78% of CPUs (healthy, not saturated)

### Using the Analysis Script

```bash
# Analyze results from all scenarios
python analyze_capacity_results.py \
    --input "capacity_*_results.csv" \
    --output capacity_analysis_report.txt \
    --chart all

# Analyze specific scenario
python analyze_capacity_results.py \
    --input capacity_balanced_results.csv \
    --chart throughput
```

**Sample output**:

```
DUCKDB CAPACITY TEST ANALYSIS REPORT
================================================================================

OPTIMAL CONFIGURATION
--------------------------------------------------------------------------------
Process count: 24
Memory per process: 4.8GB
Throughput: 156.42 queries/second
Avg latency: 2.15s
P95 latency: 2.34s
P99 latency: 2.67s
Peak memory: 112.3GB
Avg CPU: 78.2%
Degradation factor: 1.12x

SATURATION ANALYSIS
--------------------------------------------------------------------------------
Performance starts degrading at: 32 processes
Recommendation: Stay at or below 32 concurrent processes

Throughput (queries/second)
============================================================
156.4 | ●
      |     ●
      |         ●
      |             ●
      |                 ●
  0.0 |_____________________________________________________
       4    8    12   16   20   24   28   32   48   64
                    Process Count
```

### Key Metrics to Watch

#### 1. Degradation Factor

```
1.0x = No degradation (perfect)
1.2x = 20% slower than baseline (acceptable)
1.5x = 50% slower (marginal)
2.0x = 2x slower (unacceptable)
```

**Rule of thumb**: Stay where degradation < 1.5x

#### 2. Throughput (Queries/Second)

This is your **primary metric** for capacity planning.

```
Higher is better
Look for the highest throughput before degradation kicks in
```

#### 3. P95 Latency

**95th percentile** - 95% of queries complete faster than this.

```
Lower is better
More important than average for user experience
Set SLAs based on P95, not average
```

#### 4. Resource Utilization

**Memory**:
- < 70%: Underutilized, could handle more processes
- 70-85%: Good utilization
- 85-95%: High utilization, close to limit
- > 95%: Risk of OOM, reduce processes

**CPU**:
- < 50%: Underutilized, could handle more processes
- 50-80%: Good utilization
- 80-95%: High utilization, performing well
- > 95%: Saturated, may need more vCPUs

## Deployment Recommendations

### Based on Test Results

#### Scenario 1: You Found Linear Scaling

```
Test showed:
  16 processes → 40 q/s
  32 processes → 79 q/s  (1.97x)
  64 processes → 155 q/s (1.96x)
```

**Recommendation**: Go aggressive! Scale to maximum processes your instance allows.

**Configuration**:
```python
# Application config
MAX_DB_CONNECTIONS = 64
MEMORY_PER_CONNECTION = "1.9GB"

# DuckDB connection
conn = duckdb.connect(config={'max_memory': '1.9GB'})
```

#### Scenario 2: You Found a Sweet Spot

```
Test showed:
  8 processes  → 20 q/s   (degrad: 1.0x)
  16 processes → 38 q/s   (degrad: 1.05x)
  24 processes → 52 q/s   (degrad: 1.15x)  ← Sweet spot
  32 processes → 58 q/s   (degrad: 1.45x)
  48 processes → 62 q/s   (degrad: 1.85x)  ← Degrading
```

**Recommendation**: Use 24 processes (optimal throughput before significant degradation).

**Configuration**:
```python
MAX_DB_CONNECTIONS = 24
MEMORY_PER_CONNECTION = "4.8GB"
```

#### Scenario 3: You Hit a Hard Limit

```
Test showed:
  16 processes → 40 q/s   (all success)
  24 processes → 55 q/s   (all success)
  32 processes → 35 q/s   (5 failures, OOM)
  48 processes → FAILED   (all OOM)
```

**Recommendation**: Stay at 24 processes maximum. Beyond that, you hit resource limits.

**Configuration**:
```python
MAX_DB_CONNECTIONS = 24  # Hard limit
MEMORY_PER_CONNECTION = "4.8GB"

# Add connection pool with queue
from queue import Queue
connection_pool = Queue(maxsize=24)
```

### Production Deployment Checklist

#### 1. Connection Pool Configuration

```python
# Example with Python connection pool
import duckdb
from queue import Queue, Empty
import threading

class DuckDBPool:
    def __init__(self, db_path, max_connections=24, memory_per_conn="4.8GB"):
        self.db_path = db_path
        self.max_connections = max_connections
        self.memory_per_conn = memory_per_conn
        self.pool = Queue(maxsize=max_connections)

        # Pre-create connections
        for _ in range(max_connections):
            conn = duckdb.connect(
                db_path,
                read_only=True,
                config={'max_memory': memory_per_conn}
            )
            self.pool.put(conn)

    def get_connection(self, timeout=30):
        try:
            return self.pool.get(timeout=timeout)
        except Empty:
            raise Exception("Connection pool exhausted")

    def return_connection(self, conn):
        self.pool.put(conn)

# Usage
pool = DuckDBPool(
    db_path="tpch_sf16.duckdb",
    max_connections=24,
    memory_per_conn="4.8GB"
)

conn = pool.get_connection()
try:
    result = conn.execute("SELECT ...").fetchall()
finally:
    pool.return_connection(conn)
```

#### 2. Monitoring Setup

Track these metrics in production:

```python
import psutil
import time

def monitor_resources():
    # Memory
    mem = psutil.virtual_memory()
    print(f"Memory: {mem.percent}% used")

    # CPU
    cpu = psutil.cpu_percent(interval=1)
    print(f"CPU: {cpu}%")

    # Active connections (example)
    active_connections = get_active_connection_count()
    print(f"Active connections: {active_connections}/{MAX_CONNECTIONS}")

    # Queue depth
    queue_depth = connection_pool.qsize()
    print(f"Queued requests: {queue_depth}")

# Monitor every 60 seconds
while True:
    monitor_resources()
    time.sleep(60)
```

#### 3. Alert Thresholds

Set up alerts for:

```
Memory > 90%           → WARNING: approaching limit
Memory > 95%           → CRITICAL: reduce load
CPU > 95%              → WARNING: saturated
Failed queries > 1%    → CRITICAL: investigate
P95 latency > SLA      → WARNING: performance degrading
Connection pool queue > 50% full → WARNING: high demand
```

#### 4. Load Testing Before Production

```bash
# Simulate production load
python test_concurrent_processes.py \
    --db-path production_clone.duckdb \
    --process-counts 24 \
    --queries production_query_mix.sql \
    --total-memory 128GB

# Run for extended period (1 hour)
timeout 3600 python simulate_production_load.py
```

### Cost Optimization

#### Compare Instance Types

After testing multiple instance types:

```
Instance        Cost/hr  Processes  Throughput  Cost per 1M queries
i4g.4xlarge     $1.12    24         156 q/s     $2.00
i4g.8xlarge     $2.24    48         310 q/s     $2.01
r7g.4xlarge     $1.15    20         140 q/s     $2.28
m7g.4xlarge     $0.69    12         85 q/s      $2.26
```

**Analysis**:
- i4g.4xlarge and i4g.8xlarge have similar cost efficiency
- i4g.4xlarge is best value for throughput < 200 q/s
- i4g.8xlarge needed for throughput > 200 q/s

#### Throughput vs Latency Tradeoff

Sometimes you can reduce costs by accepting higher latency:

```
Config A: 24 processes, P95 = 2.3s, throughput = 156 q/s
Config B: 16 processes, P95 = 3.1s, throughput = 145 q/s (93% of A)

Cost savings: Can use smaller instance (m7g.4xlarge instead of i4g.4xlarge)
Tradeoff: 35% higher latency but 38% lower cost
Decision: Worth it if latency SLA allows
```

## Troubleshooting

### Issue: All Tests Show Linear Scaling

**Possible causes**:
1. Database is too small for meaningful testing
2. Queries are too simple
3. System has excess resources

**Solution**:
```bash
# Use larger database
python generate_tpch_data.py --output tpch_sf100.duckdb --scale-factor 100 --yes

# Or use more complex queries
--queries complex_join.sql,heavy_aggregation.sql
```

### Issue: Tests Fail with OOM Errors

**Possible causes**:
1. Memory per process set too high
2. OS overhead underestimated
3. Other processes consuming memory

**Solution**:
```bash
# Increase OS buffer from 10% to 20%
# Edit test_ec2_capacity.py:
memory_buffer_percent=20.0

# Or reduce process count
--process-counts 4,8,12,16
```

### Issue: High CPU but Low Throughput

**Possible causes**:
1. CPU contention
2. Lock contention in DuckDB
3. I/O bottleneck

**Solution**:
```bash
# Check I/O wait
python analyze_capacity_results.py --input results.csv
# Look for high avg_io_wait_percent

# Use instance with better I/O (i4g vs m7g)
# Or reduce concurrent processes
```

### Issue: Results Are Inconsistent

**Possible causes**:
1. Background processes
2. System state varies
3. Thermal throttling (on some instances)

**Solution**:
```bash
# Run tests multiple times
for i in 1 2 3; do
    python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb
    sleep 300  # Cool down between runs
done

# Average the results
python analyze_capacity_results.py --input capacity_*_results.csv
```

## Advanced Topics

### Testing with Mixed Query Workloads

Real production workloads have mixed queries (fast + slow):

```sql
-- Create mixed_workload.sql
-- 70% fast queries
SELECT COUNT(*) FROM lineitem WHERE l_shipdate > '1998-01-01';

-- 20% medium queries
SELECT l_orderkey, SUM(l_quantity) FROM lineitem GROUP BY l_orderkey LIMIT 100;

-- 10% slow queries
SELECT * FROM lineitem l JOIN orders o ON l.l_orderkey = o.o_orderkey
WHERE o.o_totalprice > 100000 LIMIT 1000;
```

```bash
python test_concurrent_processes.py \
    --db-path tpch_sf16.duckdb \
    --queries mixed_workload.sql \
    --process-counts 16,24,32
```

### Testing Spill Behavior at Scale

```bash
# Force spilling by limiting memory
python test_concurrent_processes.py \
    --db-path tpch_sf16.duckdb \
    --process-counts 32 \
    --total-memory 64GB \  # Constrained memory
    --temp-dir /mnt/nvme0n1/duckdb_spill  # Use NVMe for spill
```

### Instance Type Selection Guide

#### When to Use i4g (Storage Optimized)

```
✓ Heavy spilling workloads
✓ Large temp storage needed
✓ High I/O requirements
✓ Large datasets (>100GB)

Example: Analytics on 1TB+ datasets
```

#### When to Use r7g (Memory Optimized)

```
✓ Queries fit mostly in memory
✓ Minimal spilling
✓ High memory:data ratio
✓ Latency-sensitive workloads

Example: Interactive BI dashboards
```

#### When to Use m7g (General Purpose)

```
✓ Mixed workloads
✓ Moderate memory needs
✓ Cost-sensitive deployments
✓ Smaller datasets (<50GB)

Example: Departmental analytics
```

## Summary

### Testing Workflow

```
1. Generate test database (SF16 recommended)
   ↓
2. Run EC2 capacity test for your instance type
   ↓
3. Analyze results to find optimal configuration
   ↓
4. Deploy with recommended settings
   ↓
5. Monitor production and adjust
```

### Key Takeaways

1. **Always test with concurrent processes** - single-process tests don't reveal capacity limits
2. **Look for degradation factor** - stay where degradation < 1.5x
3. **Reserve 15-20% memory for OS** - don't allocate all memory to DuckDB
4. **Match vCPU count as starting point** - then scale up/down based on results
5. **Test with representative queries** - TPC-H is good baseline, but use your queries
6. **Monitor in production** - capacity tests are estimates, real load may differ

### Next Steps

1. Run capacity test for your instance type
2. Review recommendation JSON
3. Set up connection pool with optimal settings
4. Deploy to staging environment
5. Load test with production-like traffic
6. Monitor and adjust based on real metrics

---

For questions or issues, refer to the main [README.md](README.md) or open an issue in the repository.
