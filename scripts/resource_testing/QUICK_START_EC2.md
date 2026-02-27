# Quick Start: EC2 Capacity Testing

**Goal**: Determine how many DuckDB processes your EC2 instance can handle.

## TL;DR

```bash
# For AWS i4g.4xlarge (128GB RAM, 16 vCPUs)
python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb

# Get recommendation
cat ec2_capacity_recommendation_i4g.4xlarge.json
```

**Result**: You'll get optimal process count, memory per process, and expected throughput.

## What Each Script Does

### 1. `generate_tpch_data.py` - Create Test Database

```bash
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes
```

**Purpose**: Creates a realistic test database for benchmarking.

**You only need to run this once.**

### 2. `test_resource_usage.py` - Single Process Memory Testing

```bash
python test_resource_usage.py --db-path tpch_sf16.duckdb --memory-limits 16GB,8GB,4GB
```

**Purpose**: Tests how a **single** query performs with different memory limits.

**Use when**: Understanding query memory behavior and spill characteristics.

**Answers**:
- "How much memory does my query need?"
- "What happens if I limit memory to 4GB?"

### 3. `test_concurrent_processes.py` - Multi-Process Testing (NEW!)

```bash
python test_concurrent_processes.py \
    --db-path tpch_sf16.duckdb \
    --process-counts 8,16,24,32 \
    --total-memory 128GB
```

**Purpose**: Tests **concurrent** DuckDB processes to find capacity limits.

**Use when**: Planning production deployment with multiple users/sessions.

**Answers**:
- "How many concurrent processes can I run?"
- "What throughput can I expect?"
- "Where's the breaking point?"

### 4. `test_ec2_capacity.py` - Complete EC2 Planning Suite (NEW!)

```bash
python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb
```

**Purpose**: Comprehensive capacity planning for specific EC2 instance types.

**Use when**: Deciding on EC2 instance type and configuration.

**Answers**:
- "What's the optimal configuration for i4g.4xlarge?"
- "How many connections should my connection pool have?"
- "What throughput can I expect in production?"

### 5. `analyze_capacity_results.py` - Results Analysis (NEW!)

```bash
python analyze_capacity_results.py --input capacity_*_results.csv --chart all
```

**Purpose**: Analyzes test results and generates reports with visualizations.

**Use when**: Interpreting test results and making decisions.

**Outputs**:
- Performance trends
- Saturation points
- Scaling efficiency
- Recommendations

## When to Use What

### Scenario 1: "I want to understand my query's memory needs"

```bash
# Use single-process memory testing
python test_resource_usage.py \
    --db-path tpch_sf16.duckdb \
    --queries my_query.sql \
    --memory-limits 16GB,8GB,4GB,2GB
```

**Result**: You'll see how your query performs at different memory limits and when spilling occurs.

### Scenario 2: "I want to know how many processes my EC2 instance can handle"

```bash
# Use EC2 capacity planning
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb \
    --queries my_query.sql
```

**Result**: You'll get optimal process count and expected throughput.

### Scenario 3: "I want to test a specific configuration"

```bash
# Use concurrent process testing directly
python test_concurrent_processes.py \
    --db-path tpch_sf16.duckdb \
    --process-counts 24 \
    --total-memory 128GB \
    --queries my_query.sql
```

**Result**: You'll see how 24 concurrent processes perform on your system.

### Scenario 4: "I want to compare different instance types"

```bash
# Test each instance type
for instance in i4g.4xlarge i4g.8xlarge r7g.4xlarge m7g.4xlarge; do
    python test_ec2_capacity.py \
        --instance-type $instance \
        --db-path tpch_sf16.duckdb \
        --quick
done

# Compare results
python analyze_capacity_results.py --input capacity_*_results.csv
```

**Result**: You'll see which instance type gives best price/performance for your workload.

## Reading Results

### Concurrent Process Test Output

```
Processes  Throughput   Avg Time   P95 Time   Peak Mem   CPU    Degradation
12         52.3 q/s     2.1s       2.3s       89.2GB     67%    1.0x
24         98.7 q/s     2.2s       2.4s       115.4GB    82%    1.06x       ← Optimal
32         112.3 q/s    2.8s       3.1s       118.9GB    94%    1.42x
48         98.1 q/s     4.5s       5.2s       120.1GB    98%    2.13x       ← Degraded
```

**How to read**:
- **Throughput**: Higher is better (queries/second)
- **P95 Time**: 95% of queries complete in this time or less
- **Degradation**: How much slower vs baseline (< 1.5x is good)
- **Optimal**: Best throughput before significant degradation (24 processes)

### EC2 Recommendation JSON

```json
{
  "optimal_process_count": 24,
  "memory_per_process_gb": 4.8,
  "expected_throughput_qps": 98.7,
  "expected_p95_latency_sec": 2.4
}
```

**Deploy with**:
- Connection pool max size: **24**
- DuckDB max_memory per connection: **4.8GB**
- Expected throughput: **~99 queries/second**
- Expected P95 latency: **~2.4 seconds**

## Common Issues

### "Tests complete too quickly / no meaningful results"

Your database might be too small. Check:

```bash
duckdb tpch_sf16.duckdb -c "SELECT COUNT(*) FROM lineitem;"
```

Should return ~72 million rows for SF16.

If less, regenerate:

```bash
rm tpch_sf16.duckdb
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes
```

### "Tests show all processes succeed with no degradation"

Your system has excess capacity. This is good! But to find limits:

```bash
# Test with more processes
python test_concurrent_processes.py \
    --db-path tpch_sf16.duckdb \
    --process-counts 32,48,64,96,128 \
    --total-memory 128GB
```

### "Tests fail with OOM (Out of Memory) errors"

You're exceeding system memory. Solutions:

1. Reduce process count:
   ```bash
   --process-counts 4,8,12,16
   ```

2. Reduce memory per process:
   ```bash
   # EC2 test uses conservative allocation
   python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb
   ```

3. Use smaller database:
   ```bash
   python generate_tpch_data.py --output tpch_sf1.duckdb --scale-factor 1 --yes
   ```

## Example Workflow

### Step 1: Generate Database (One-Time)

```bash
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/scripts/resource_testing
python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --yes
```

### Step 2: Run EC2 Capacity Test

```bash
python test_ec2_capacity.py \
    --instance-type i4g.4xlarge \
    --db-path tpch_sf16.duckdb
```

**Time**: 30-60 minutes (use `--quick` for 5-10 minutes)

### Step 3: Review Results

```bash
# View recommendation
cat ec2_capacity_recommendation_i4g.4xlarge.json

# Detailed analysis
python analyze_capacity_results.py \
    --input capacity_*_results.csv \
    --output report.txt \
    --chart all

cat report.txt
```

### Step 4: Deploy with Recommended Settings

Example for Python application:

```python
import duckdb
from queue import Queue

# From recommendation
MAX_CONNECTIONS = 24
MEMORY_PER_CONNECTION = "4.8GB"

# Connection pool
pool = Queue(maxsize=MAX_CONNECTIONS)
for _ in range(MAX_CONNECTIONS):
    conn = duckdb.connect(
        "production.duckdb",
        read_only=True,
        config={'max_memory': MEMORY_PER_CONNECTION}
    )
    pool.put(conn)

# Usage
conn = pool.get()
try:
    result = conn.execute("SELECT ...").fetchall()
finally:
    pool.put(conn)
```

## Supported EC2 Instance Types

Pre-configured instance types (use with `--instance-type`):

| Instance Type | Memory | vCPUs | Storage | Description |
|---------------|--------|-------|---------|-------------|
| i4g.4xlarge   | 128GB  | 16    | 3750GB NVMe | Storage optimized |
| i4g.8xlarge   | 256GB  | 32    | 7500GB NVMe | Storage optimized |
| i4g.16xlarge  | 512GB  | 64    | 15000GB NVMe | Storage optimized |
| r7g.4xlarge   | 128GB  | 16    | EBS only | Memory optimized |
| r7g.8xlarge   | 256GB  | 32    | EBS only | Memory optimized |
| m7g.4xlarge   | 64GB   | 16    | EBS only | General purpose |
| m7g.8xlarge   | 128GB  | 32    | EBS only | General purpose |

For custom configurations:

```bash
python test_ec2_capacity.py --memory 128GB --vcpus 16 --db-path tpch_sf16.duckdb
```

## What Files Do I Get?

After running tests:

```
tpch_sf16.duckdb                              # Test database (reusable)
capacity_conservative_results.csv              # Conservative scenario results
capacity_balanced_results.csv                  # Balanced scenario results
capacity_aggressive_results.csv                # Aggressive scenario results
capacity_saturation_results.csv                # Saturation test results
ec2_capacity_recommendation_i4g.4xlarge.json   # Final recommendation
```

Keep these files for:
- Reference when deploying
- Comparison when retesting
- Sharing with team
- Documentation

## Next Steps

1. **Read full guide**: [EC2_CAPACITY_PLANNING_GUIDE.md](EC2_CAPACITY_PLANNING_GUIDE.md)
2. **Run your first test**: Use commands above
3. **Deploy to staging**: Use recommended settings
4. **Monitor production**: Track actual vs expected performance
5. **Iterate**: Retest with production query patterns

## Help

- Full documentation: [EC2_CAPACITY_PLANNING_GUIDE.md](EC2_CAPACITY_PLANNING_GUIDE.md)
- Original memory testing guide: [README.md](README.md)
- Step-by-step tutorial: [HOW_TO_RUN_TESTS.md](HOW_TO_RUN_TESTS.md)
