#!/usr/bin/env python3
"""
AWS EC2 Capacity Planning Test Suite for DuckDB

Comprehensive test suite that helps determine optimal DuckDB configuration
for specific AWS EC2 instance types. Tests various scenarios including:

1. Process count scaling (find optimal number of concurrent processes)
2. Memory allocation per process
3. Throughput vs latency tradeoffs
4. Resource saturation points
5. Cost-performance optimization

Supports common AWS instance types:
- i4g.4xlarge: 128GB RAM, 16 vCPUs, 3750GB NVMe
- i4g.8xlarge: 256GB RAM, 32 vCPUs, 7500GB NVMe
- r7g.4xlarge: 128GB RAM, 16 vCPUs, EBS only
- m7g.4xlarge: 64GB RAM, 16 vCPUs, EBS only

Usage:
    # Test for i4g.4xlarge
    python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb

    # Custom configuration
    python test_ec2_capacity.py --memory 128GB --vcpus 16 --db-path tpch_sf16.duckdb

    # Quick test (fewer scenarios)
    python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb --quick
"""

import argparse
import csv
import json
import os
import subprocess
import sys
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional, Tuple

try:
    import psutil
except ImportError:
    print("Error: psutil is required. Install it with: pip install psutil", file=sys.stderr)
    sys.exit(1)


# AWS EC2 instance type specifications
EC2_INSTANCE_TYPES = {
    'i4g.4xlarge': {
        'memory_gb': 128,
        'vcpus': 16,
        'nvme_storage_gb': 3750,
        'network_gbps': 18.75,
        'description': 'Storage optimized, AWS Graviton3'
    },
    'i4g.8xlarge': {
        'memory_gb': 256,
        'vcpus': 32,
        'nvme_storage_gb': 7500,
        'network_gbps': 37.5,
        'description': 'Storage optimized, AWS Graviton3'
    },
    'i4g.16xlarge': {
        'memory_gb': 512,
        'vcpus': 64,
        'nvme_storage_gb': 15000,
        'network_gbps': 75,
        'description': 'Storage optimized, AWS Graviton3'
    },
    'r7g.4xlarge': {
        'memory_gb': 128,
        'vcpus': 16,
        'nvme_storage_gb': 0,
        'network_gbps': 15,
        'description': 'Memory optimized, AWS Graviton3'
    },
    'r7g.8xlarge': {
        'memory_gb': 256,
        'vcpus': 32,
        'nvme_storage_gb': 0,
        'network_gbps': 30,
        'description': 'Memory optimized, AWS Graviton3'
    },
    'm7g.4xlarge': {
        'memory_gb': 64,
        'vcpus': 16,
        'nvme_storage_gb': 0,
        'network_gbps': 15,
        'description': 'General purpose, AWS Graviton3'
    },
    'm7g.8xlarge': {
        'memory_gb': 128,
        'vcpus': 32,
        'nvme_storage_gb': 0,
        'network_gbps': 30,
        'description': 'General purpose, AWS Graviton3'
    }
}


@dataclass
class TestScenario:
    """Test scenario configuration"""
    name: str
    description: str
    process_counts: List[int]
    memory_allocation_strategy: str  # 'equal', 'conservative', 'aggressive'
    memory_buffer_percent: float  # Reserve X% for OS


@dataclass
class CapacityRecommendation:
    """Capacity planning recommendation"""
    instance_type: str
    optimal_process_count: int
    memory_per_process_gb: float
    expected_throughput_qps: float
    expected_p95_latency_sec: float
    memory_utilization_percent: float
    cpu_utilization_percent: float
    cost_per_million_queries: Optional[float] = None
    notes: List[str] = None

    def __post_init__(self):
        if self.notes is None:
            self.notes = []


def parse_memory_limit(memory_str: str) -> int:
    """Parse memory limit string to bytes"""
    memory_str = memory_str.strip().upper()

    multipliers = [
        ('TB', 1024 ** 4),
        ('GB', 1024 ** 3),
        ('MB', 1024 ** 2),
        ('KB', 1024),
        ('B', 1),
    ]

    for unit, multiplier in multipliers:
        if memory_str.endswith(unit):
            value = float(memory_str[:-len(unit)])
            return int(value * multiplier)

    return int(float(memory_str))


def generate_test_scenarios(memory_gb: int, vcpus: int, mode: str = 'full') -> List[TestScenario]:
    """Generate test scenarios based on instance specs"""

    if mode == 'quick':
        # Quick test: fewer scenarios
        return [
            TestScenario(
                name='quick_scan',
                description='Quick capacity scan',
                process_counts=[vcpus // 2, vcpus, vcpus * 2],
                memory_allocation_strategy='equal',
                memory_buffer_percent=10.0
            )
        ]

    # Full test scenarios
    scenarios = []

    # Scenario 1: Conservative (low process count, high memory per process)
    scenarios.append(TestScenario(
        name='conservative',
        description='Conservative: High memory per process, low concurrency',
        process_counts=[4, 8, 12],
        memory_allocation_strategy='conservative',
        memory_buffer_percent=20.0
    ))

    # Scenario 2: Balanced (match vCPU count)
    scenarios.append(TestScenario(
        name='balanced',
        description='Balanced: Match vCPU count',
        process_counts=[vcpus // 2, vcpus, int(vcpus * 1.5)],
        memory_allocation_strategy='equal',
        memory_buffer_percent=15.0
    ))

    # Scenario 3: Aggressive (high concurrency, lower memory)
    max_processes = min(vcpus * 4, 128)  # Cap at 128 processes
    scenarios.append(TestScenario(
        name='aggressive',
        description='Aggressive: High concurrency, lower memory per process',
        process_counts=[vcpus * 2, vcpus * 3, max_processes],
        memory_allocation_strategy='aggressive',
        memory_buffer_percent=10.0
    ))

    # Scenario 4: Saturation test (find breaking point)
    scenarios.append(TestScenario(
        name='saturation',
        description='Saturation: Find resource limits',
        process_counts=list(range(vcpus, min(vcpus * 8, 256), vcpus)),
        memory_allocation_strategy='equal',
        memory_buffer_percent=10.0
    ))

    return scenarios


def calculate_memory_per_process(total_memory_gb: int, num_processes: int,
                                 strategy: str, buffer_percent: float) -> str:
    """Calculate memory allocation per process"""

    # Reserve buffer for OS
    available_memory_gb = total_memory_gb * (1 - buffer_percent / 100.0)

    if strategy == 'conservative':
        # Conservative: use 80% of available memory
        mem_per_process = (available_memory_gb * 0.8) / num_processes
    elif strategy == 'aggressive':
        # Aggressive: use 95% of available memory
        mem_per_process = (available_memory_gb * 0.95) / num_processes
    else:  # equal
        # Equal: divide evenly
        mem_per_process = available_memory_gb / num_processes

    return f"{mem_per_process:.1f}GB"


def run_capacity_test(db_path: str, scenario: TestScenario, memory_gb: int,
                     temp_dir: Optional[Path] = None,
                     queries: Optional[str] = None) -> Tuple[bool, Dict]:
    """Run a capacity test scenario"""

    print(f"\n{'='*80}")
    print(f"Scenario: {scenario.name}")
    print(f"Description: {scenario.description}")
    print(f"{'='*80}")

    # Build command for concurrent process test
    cmd = [
        sys.executable,
        str(Path(__file__).parent / "test_concurrent_processes.py"),
        "--db-path", db_path,
        "--process-counts", ",".join(map(str, scenario.process_counts)),
        "--total-memory", f"{memory_gb}GB",
    ]

    if temp_dir:
        cmd.extend(["--temp-dir", str(temp_dir)])

    if queries:
        cmd.extend(["--queries", queries])

    # Output to scenario-specific CSV
    output_file = f"capacity_{scenario.name}_results.csv"
    cmd.extend(["--output", output_file])

    print(f"Running: {' '.join(cmd)}")
    print()

    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        print(result.stdout)

        return True, {'output_file': output_file, 'stdout': result.stdout}

    except subprocess.CalledProcessError as e:
        print(f"Error running scenario {scenario.name}:")
        print(e.stderr)
        return False, {'error': str(e)}


def analyze_results(scenario_results: Dict[str, str], memory_gb: int) -> CapacityRecommendation:
    """Analyze test results and generate recommendations"""

    all_results = []

    # Load all scenario results
    for scenario_name, output_file in scenario_results.items():
        if not Path(output_file).exists():
            continue

        with open(output_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                row['scenario'] = scenario_name
                all_results.append(row)

    if not all_results:
        return CapacityRecommendation(
            instance_type='unknown',
            optimal_process_count=0,
            memory_per_process_gb=0.0,
            expected_throughput_qps=0.0,
            expected_p95_latency_sec=0.0,
            memory_utilization_percent=0.0,
            cpu_utilization_percent=0.0,
            notes=["No results available"]
        )

    # Find optimal configuration
    # Criteria: Max throughput with degradation < 2x and failures = 0
    valid_results = [
        r for r in all_results
        if int(r['failed_queries']) == 0 and float(r['degradation_factor']) < 2.0
    ]

    if not valid_results:
        # Fallback: just use any result without failures
        valid_results = [r for r in all_results if int(r['failed_queries']) == 0]

    if not valid_results:
        # Still nothing? Use all results
        valid_results = all_results

    # Find best throughput
    optimal = max(valid_results, key=lambda r: float(r['throughput_qps']))

    # Extract metrics
    num_processes = int(optimal['num_processes'])
    throughput = float(optimal['throughput_qps'])
    p95_latency = float(optimal['p95_query_time_sec'])
    peak_memory_mb = float(optimal['peak_memory_mb'])
    avg_cpu = float(optimal['avg_cpu_percent'])

    # Parse memory per process
    mem_str = optimal['memory_per_process']
    if 'GB' in mem_str:
        mem_per_proc_gb = float(mem_str.replace('GB', ''))
    elif 'MB' in mem_str:
        mem_per_proc_gb = float(mem_str.replace('MB', '')) / 1024
    else:
        mem_per_proc_gb = 0.0

    memory_util = (peak_memory_mb / 1024) / memory_gb * 100

    # Generate notes
    notes = []

    if float(optimal['degradation_factor']) > 1.5:
        notes.append(f"Warning: Performance degradation detected ({optimal['degradation_factor']}x)")

    if memory_util > 90:
        notes.append("Warning: High memory utilization (>90%)")
    elif memory_util > 75:
        notes.append("Moderate memory utilization (75-90%)")

    if avg_cpu > 90:
        notes.append("Warning: High CPU utilization (>90%)")
    elif avg_cpu < 50:
        notes.append("CPU underutilized (<50%) - consider more processes")

    if int(optimal['failed_queries']) > 0:
        notes.append(f"Warning: {optimal['failed_queries']} queries failed")

    return CapacityRecommendation(
        instance_type='custom',
        optimal_process_count=num_processes,
        memory_per_process_gb=mem_per_proc_gb,
        expected_throughput_qps=throughput,
        expected_p95_latency_sec=p95_latency,
        memory_utilization_percent=memory_util,
        cpu_utilization_percent=avg_cpu,
        notes=notes
    )


def save_recommendation(recommendation: CapacityRecommendation, output_file: str):
    """Save recommendation to JSON file"""
    with open(output_file, 'w') as f:
        json.dump(asdict(recommendation), f, indent=2)
    print(f"\nRecommendation saved to: {output_file}")


def print_recommendation(recommendation: CapacityRecommendation, instance_type: str):
    """Print capacity recommendation"""
    print(f"\n{'='*80}")
    print("EC2 CAPACITY PLANNING RECOMMENDATION")
    print(f"{'='*80}")

    if instance_type in EC2_INSTANCE_TYPES:
        spec = EC2_INSTANCE_TYPES[instance_type]
        print(f"\nInstance Type: {instance_type}")
        print(f"  Memory: {spec['memory_gb']}GB")
        print(f"  vCPUs: {spec['vcpus']}")
        print(f"  NVMe Storage: {spec['nvme_storage_gb']}GB")
        print(f"  Description: {spec['description']}")

    print(f"\nRecommended Configuration:")
    print(f"  Optimal Process Count: {recommendation.optimal_process_count}")
    print(f"  Memory per Process: {recommendation.memory_per_process_gb:.1f}GB")
    print(f"  Expected Throughput: {recommendation.expected_throughput_qps:.2f} queries/second")
    print(f"  Expected P95 Latency: {recommendation.expected_p95_latency_sec:.2f} seconds")

    print(f"\nResource Utilization:")
    print(f"  Memory: {recommendation.memory_utilization_percent:.1f}%")
    print(f"  CPU: {recommendation.cpu_utilization_percent:.1f}%")

    if recommendation.notes:
        print(f"\nNotes:")
        for note in recommendation.notes:
            print(f"  - {note}")

    print(f"\n{'='*80}")

    # Practical deployment guidance
    print("\nDeployment Guidance:")
    print(f"  1. Configure DuckDB connection pool with {recommendation.optimal_process_count} max connections")
    print(f"  2. Set max_memory = '{recommendation.memory_per_process_gb:.1f}GB' per session")
    print(f"  3. Monitor actual throughput and adjust based on query mix")
    print(f"  4. Consider load testing with production query patterns")

    if instance_type in EC2_INSTANCE_TYPES:
        spec = EC2_INSTANCE_TYPES[instance_type]
        if spec['nvme_storage_gb'] > 0:
            spill_per_process = spec['nvme_storage_gb'] / recommendation.optimal_process_count
            print(f"  5. NVMe spill storage available: ~{spill_per_process:.0f}GB per process")

    print(f"{'='*80}\n")


def main():
    parser = argparse.ArgumentParser(
        description='EC2 Capacity Planning Test Suite for DuckDB',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
Supported EC2 Instance Types:
{chr(10).join(f"  {name}: {spec['memory_gb']}GB RAM, {spec['vcpus']} vCPUs - {spec['description']}"
           for name, spec in EC2_INSTANCE_TYPES.items())}

Examples:
  # Test for AWS i4g.4xlarge
  python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb

  # Quick test (fewer scenarios)
  python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb --quick

  # Custom configuration
  python test_ec2_capacity.py --memory 128GB --vcpus 16 --db-path tpch_sf16.duckdb

  # With custom queries
  python test_ec2_capacity.py --instance-type i4g.4xlarge --db-path tpch_sf16.duckdb \\
      --queries query1.sql,query2.sql
        """
    )

    parser.add_argument('--instance-type', type=str, choices=list(EC2_INSTANCE_TYPES.keys()),
                       help='AWS EC2 instance type to simulate')
    parser.add_argument('--memory', type=str,
                       help='Total memory (e.g., "128GB") - use instead of --instance-type')
    parser.add_argument('--vcpus', type=int,
                       help='Number of vCPUs - use instead of --instance-type')
    parser.add_argument('--db-path', type=str, required=True,
                       help='Path to DuckDB database file')
    parser.add_argument('--queries', type=str,
                       help='Comma-separated list of SQL query files to test')
    parser.add_argument('--temp-dir', type=str,
                       help='Temporary directory for spill files')
    parser.add_argument('--quick', action='store_true',
                       help='Run quick test (fewer scenarios)')
    parser.add_argument('--output', type=str,
                       help='Output file for recommendation (JSON)')

    args = parser.parse_args()

    # Determine configuration
    if args.instance_type:
        spec = EC2_INSTANCE_TYPES[args.instance_type]
        memory_gb = spec['memory_gb']
        vcpus = spec['vcpus']
        instance_type = args.instance_type
    elif args.memory and args.vcpus:
        memory_gb = parse_memory_limit(args.memory) // (1024 ** 3)
        vcpus = args.vcpus
        instance_type = 'custom'
    else:
        print("Error: Must specify either --instance-type OR both --memory and --vcpus", file=sys.stderr)
        sys.exit(1)

    # Validate database
    db_path = Path(args.db_path)
    if not db_path.exists():
        print(f"Error: Database not found: {db_path}", file=sys.stderr)
        sys.exit(1)

    print("EC2 Capacity Planning Test Suite")
    print("=" * 80)
    print(f"Instance Configuration: {memory_gb}GB RAM, {vcpus} vCPUs")
    print(f"Database: {db_path}")
    print(f"Test Mode: {'Quick' if args.quick else 'Full'}")
    print()

    # Generate test scenarios
    mode = 'quick' if args.quick else 'full'
    scenarios = generate_test_scenarios(memory_gb, vcpus, mode)

    print(f"Generated {len(scenarios)} test scenario(s):")
    for scenario in scenarios:
        print(f"  - {scenario.name}: {scenario.description}")
    print()

    # Setup temp directory
    if args.temp_dir:
        temp_path = Path(args.temp_dir)
        temp_path.mkdir(parents=True, exist_ok=True)
    else:
        temp_path = Path.home() / ".tmp" / "ec2_capacity_test"
        temp_path.mkdir(parents=True, exist_ok=True)

    # Run all scenarios
    scenario_results = {}
    for scenario in scenarios:
        success, result = run_capacity_test(
            db_path=str(db_path),
            scenario=scenario,
            memory_gb=memory_gb,
            temp_dir=temp_path,
            queries=args.queries
        )

        if success:
            scenario_results[scenario.name] = result['output_file']
        else:
            print(f"Warning: Scenario {scenario.name} failed")

    if not scenario_results:
        print("Error: All scenarios failed", file=sys.stderr)
        sys.exit(1)

    # Analyze results and generate recommendation
    print("\nAnalyzing results...")
    recommendation = analyze_results(scenario_results, memory_gb)
    recommendation.instance_type = instance_type

    # Print recommendation
    print_recommendation(recommendation, instance_type)

    # Save recommendation
    output_file = args.output or f"ec2_capacity_recommendation_{instance_type}.json"
    save_recommendation(recommendation, output_file)

    print("\nCapacity planning complete!")
    print(f"Results saved to: {output_file}")
    print(f"Individual scenario results: capacity_*_results.csv")


if __name__ == '__main__':
    main()
