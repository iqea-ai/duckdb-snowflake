#!/usr/bin/env python3
"""
DuckDB Concurrent Process Benchmark Script

Tests DuckDB performance with concurrent processes to determine optimal
process count for AWS EC2 instances. This simulates real production load
where multiple users/sessions query the database simultaneously.

Key metrics tracked:
- Total throughput (queries/second)
- Individual query latency
- System-wide memory usage
- CPU utilization
- I/O wait time
- Degradation point (when adding more processes hurts)

Usage:
    # Test with varying process counts
    python test_concurrent_processes.py --db-path tpch_sf16.duckdb --process-counts 4,8,16,32

    # Simulate i4g.4xlarge (128GB, 16 vCPUs)
    python test_concurrent_processes.py --db-path tpch_sf16.duckdb \
        --process-counts 8,16,24,32 \
        --total-memory 128GB \
        --output ec2_capacity_results.csv
"""

import argparse
import csv
import json
import multiprocessing
import os
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import List, Optional, Dict, Any, Tuple

try:
    import psutil
except ImportError:
    print("Error: psutil is required. Install it with: pip install psutil", file=sys.stderr)
    sys.exit(1)

try:
    import duckdb
except ImportError:
    print("Error: duckdb is required. Install it with: pip install duckdb", file=sys.stderr)
    sys.exit(1)


@dataclass
class ProcessResult:
    """Result from a single process execution"""
    process_id: int
    query_name: str
    execution_time_seconds: float
    success: bool
    error_message: Optional[str] = None
    memory_usage_mb: float = 0.0
    peak_memory_mb: float = 0.0


@dataclass
class ConcurrencyTestResult:
    """Result from a concurrency test run"""
    num_processes: int
    memory_per_process: str
    queries_executed: int
    successful_queries: int
    failed_queries: int
    total_time_seconds: float
    avg_query_time_seconds: float
    median_query_time_seconds: float
    p95_query_time_seconds: float
    p99_query_time_seconds: float
    throughput_queries_per_second: float
    peak_system_memory_mb: float
    avg_cpu_percent: float
    peak_cpu_percent: float
    avg_io_wait_percent: float
    degradation_factor: float = 1.0  # Compared to baseline


@dataclass
class SystemMetrics:
    """System-wide resource metrics"""
    timestamp: float
    memory_used_mb: float
    memory_percent: float
    cpu_percent: float
    io_wait_percent: float
    num_active_processes: int


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


def format_bytes(bytes_value: int) -> str:
    """Format bytes to human-readable string"""
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if bytes_value < 1024.0:
            return f"{bytes_value:.2f}{unit}"
        bytes_value /= 1024.0
    return f"{bytes_value:.2f}PB"


class SystemMonitor:
    """Monitors system resources during test execution"""

    def __init__(self, interval: float = 0.5):
        self.interval = interval
        self.metrics: List[SystemMetrics] = []
        self.monitoring = False
        self.monitor_thread = None
        self.process_ids: List[int] = []

    def start(self):
        """Start monitoring system resources"""
        self.monitoring = True
        self.metrics = []
        self.monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self.monitor_thread.start()

    def stop(self):
        """Stop monitoring"""
        self.monitoring = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=2.0)

    def _monitor_loop(self):
        """Monitor loop that collects metrics"""
        while self.monitoring:
            try:
                # Get system-wide metrics
                mem = psutil.virtual_memory()
                cpu_percent = psutil.cpu_percent(interval=None)

                # Get I/O wait (Linux only)
                io_wait = 0.0
                try:
                    io_counters = psutil.cpu_times_percent(interval=None)
                    if hasattr(io_counters, 'iowait'):
                        io_wait = io_counters.iowait
                except:
                    pass

                # Count active DuckDB processes
                active_processes = 0
                for proc in psutil.process_iter(['pid', 'name']):
                    try:
                        if 'duckdb' in proc.info['name'].lower() or proc.pid in self.process_ids:
                            active_processes += 1
                    except:
                        pass

                metric = SystemMetrics(
                    timestamp=time.time(),
                    memory_used_mb=mem.used / (1024 ** 2),
                    memory_percent=mem.percent,
                    cpu_percent=cpu_percent,
                    io_wait_percent=io_wait,
                    num_active_processes=active_processes
                )
                self.metrics.append(metric)

            except Exception:
                pass

            time.sleep(self.interval)

    def get_summary(self) -> Dict[str, float]:
        """Get summary statistics from collected metrics"""
        if not self.metrics:
            return {
                'peak_memory_mb': 0.0,
                'avg_memory_mb': 0.0,
                'peak_cpu_percent': 0.0,
                'avg_cpu_percent': 0.0,
                'avg_io_wait_percent': 0.0,
            }

        return {
            'peak_memory_mb': max(m.memory_used_mb for m in self.metrics),
            'avg_memory_mb': sum(m.memory_used_mb for m in self.metrics) / len(self.metrics),
            'peak_cpu_percent': max(m.cpu_percent for m in self.metrics),
            'avg_cpu_percent': sum(m.cpu_percent for m in self.metrics) / len(self.metrics),
            'avg_io_wait_percent': sum(m.io_wait_percent for m in self.metrics) / len(self.metrics),
            'max_active_processes': max(m.num_active_processes for m in self.metrics),
        }


def run_query_in_process(process_id: int, db_path: str, query: str, query_name: str,
                         memory_limit: Optional[str] = None,
                         temp_dir: Optional[Path] = None) -> ProcessResult:
    """Run a single query in a separate process"""
    start_time = time.time()
    peak_memory = 0.0

    try:
        # Track memory for this process
        current_process = psutil.Process()
        initial_memory = current_process.memory_info().rss / (1024 ** 2)

        # Connect to database with memory limit
        config = {}
        if memory_limit:
            config['max_memory'] = memory_limit

        conn = duckdb.connect(db_path, read_only=True, config=config)

        # Set temp directory if specified
        if temp_dir:
            conn.execute(f"SET temp_directory = '{temp_dir}'")

        # Execute query
        result = conn.execute(query)
        rows = result.fetchall()

        # Track peak memory
        final_memory = current_process.memory_info().rss / (1024 ** 2)
        peak_memory = max(initial_memory, final_memory)

        conn.close()

        execution_time = time.time() - start_time

        return ProcessResult(
            process_id=process_id,
            query_name=query_name,
            execution_time_seconds=execution_time,
            success=True,
            memory_usage_mb=final_memory - initial_memory,
            peak_memory_mb=peak_memory
        )

    except Exception as e:
        execution_time = time.time() - start_time
        return ProcessResult(
            process_id=process_id,
            query_name=query_name,
            execution_time_seconds=execution_time,
            success=False,
            error_message=str(e),
            peak_memory_mb=peak_memory
        )


def run_concurrent_test(db_path: str, query: str, query_name: str,
                       num_processes: int, memory_per_process: Optional[str] = None,
                       temp_dir: Optional[Path] = None) -> Tuple[List[ProcessResult], SystemMetrics]:
    """Run concurrent processes and collect results"""

    print(f"  Launching {num_processes} concurrent processes...", flush=True)

    # Start system monitoring
    monitor = SystemMonitor(interval=0.5)
    monitor.start()

    # Use multiprocessing pool to run queries concurrently
    start_time = time.time()

    with multiprocessing.Pool(processes=num_processes) as pool:
        # Create tasks for all processes
        tasks = [
            pool.apply_async(
                run_query_in_process,
                args=(i, db_path, query, query_name, memory_per_process, temp_dir)
            )
            for i in range(num_processes)
        ]

        # Wait for all tasks to complete
        results = [task.get() for task in tasks]

    total_time = time.time() - start_time

    # Stop monitoring
    monitor.stop()

    # Calculate metrics
    successful = [r for r in results if r.success]
    failed = [r for r in results if not r.success]

    if successful:
        exec_times = [r.execution_time_seconds for r in successful]
        exec_times_sorted = sorted(exec_times)

        avg_time = sum(exec_times) / len(exec_times)
        median_time = exec_times_sorted[len(exec_times_sorted) // 2]
        p95_idx = int(len(exec_times_sorted) * 0.95)
        p95_time = exec_times_sorted[p95_idx] if p95_idx < len(exec_times_sorted) else exec_times_sorted[-1]
        p99_idx = int(len(exec_times_sorted) * 0.99)
        p99_time = exec_times_sorted[p99_idx] if p99_idx < len(exec_times_sorted) else exec_times_sorted[-1]
    else:
        avg_time = median_time = p95_time = p99_time = 0.0

    # Get system metrics summary
    sys_summary = monitor.get_summary()

    # Calculate throughput
    throughput = len(successful) / total_time if total_time > 0 else 0.0

    result = ConcurrencyTestResult(
        num_processes=num_processes,
        memory_per_process=memory_per_process or "unlimited",
        queries_executed=num_processes,
        successful_queries=len(successful),
        failed_queries=len(failed),
        total_time_seconds=total_time,
        avg_query_time_seconds=avg_time,
        median_query_time_seconds=median_time,
        p95_query_time_seconds=p95_time,
        p99_query_time_seconds=p99_time,
        throughput_queries_per_second=throughput,
        peak_system_memory_mb=sys_summary['peak_memory_mb'],
        avg_cpu_percent=sys_summary['avg_cpu_percent'],
        peak_cpu_percent=sys_summary['peak_cpu_percent'],
        avg_io_wait_percent=sys_summary['avg_io_wait_percent']
    )

    print(f"    ✓ Completed in {total_time:.2f}s "
          f"(avg: {avg_time:.2f}s, p95: {p95_time:.2f}s, "
          f"throughput: {throughput:.2f} q/s)", flush=True)

    return results, result


def load_queries(query_files: List[str]) -> Dict[str, str]:
    """Load queries from files"""
    queries = {}
    for query_file in query_files:
        path = Path(query_file)
        if not path.exists():
            print(f"Warning: Query file not found: {query_file}", file=sys.stderr)
            continue

        query_name = path.stem
        with open(path, 'r') as f:
            queries[query_name] = f.read().strip()

    return queries


def run_benchmark(db_path: str, process_counts: List[int], queries: Dict[str, str],
                  total_memory: Optional[str] = None, temp_dir: Optional[str] = None) -> List[ConcurrencyTestResult]:
    """Run concurrent process benchmark"""

    results = []

    # Setup temp directory
    if temp_dir:
        temp_path = Path(temp_dir)
        temp_path.mkdir(parents=True, exist_ok=True)
    else:
        temp_path = Path.home() / ".tmp"
        temp_path.mkdir(parents=True, exist_ok=True)

    print(f"Database: {db_path}")
    print(f"Temp directory: {temp_path}")
    print(f"Process counts to test: {', '.join(map(str, process_counts))}")
    print(f"Queries to test: {', '.join(queries.keys())}")
    if total_memory:
        print(f"Total system memory: {total_memory}")
    print()

    baseline_throughput = None

    for query_name, query in queries.items():
        print(f"Testing query: {query_name}")
        print(f"{'='*80}")

        for num_processes in process_counts:
            # Calculate memory per process if total memory specified
            memory_per_process = None
            if total_memory:
                total_bytes = parse_memory_limit(total_memory)
                per_process_bytes = total_bytes // num_processes
                # Convert back to human-readable
                memory_per_process = format_bytes(per_process_bytes)

            print(f"\n  Testing with {num_processes} concurrent processes", end='')
            if memory_per_process:
                print(f" ({memory_per_process} per process)", end='')
            print()

            # Clear temp files
            try:
                for file in temp_path.glob("duckdb_temp_*.tmp"):
                    file.unlink()
            except Exception:
                pass

            # Run test
            process_results, test_result = run_concurrent_test(
                db_path=db_path,
                query=query,
                query_name=query_name,
                num_processes=num_processes,
                memory_per_process=memory_per_process,
                temp_dir=temp_path
            )

            # Calculate degradation factor
            if baseline_throughput is None:
                baseline_throughput = test_result.throughput_queries_per_second
                test_result.degradation_factor = 1.0
            else:
                test_result.degradation_factor = baseline_throughput / test_result.throughput_queries_per_second if test_result.throughput_queries_per_second > 0 else float('inf')

            results.append(test_result)

            # Print any failures
            failed = [r for r in process_results if not r.success]
            if failed:
                print(f"    ✗ {len(failed)} processes failed:")
                for fail in failed[:3]:  # Show first 3 failures
                    print(f"      - Process {fail.process_id}: {fail.error_message}")

        print()
        baseline_throughput = None  # Reset for next query

    return results


def save_csv_results(results: List[ConcurrencyTestResult], output_file: str):
    """Save results to CSV"""
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            'num_processes', 'memory_per_process', 'total_time_sec',
            'avg_query_time_sec', 'median_query_time_sec',
            'p95_query_time_sec', 'p99_query_time_sec',
            'throughput_qps', 'successful_queries', 'failed_queries',
            'peak_memory_mb', 'avg_cpu_percent', 'peak_cpu_percent',
            'avg_io_wait_percent', 'degradation_factor'
        ])

        for r in results:
            writer.writerow([
                r.num_processes,
                r.memory_per_process,
                f"{r.total_time_seconds:.3f}",
                f"{r.avg_query_time_seconds:.3f}",
                f"{r.median_query_time_seconds:.3f}",
                f"{r.p95_query_time_seconds:.3f}",
                f"{r.p99_query_time_seconds:.3f}",
                f"{r.throughput_queries_per_second:.3f}",
                r.successful_queries,
                r.failed_queries,
                f"{r.peak_system_memory_mb:.2f}",
                f"{r.avg_cpu_percent:.1f}",
                f"{r.peak_cpu_percent:.1f}",
                f"{r.avg_io_wait_percent:.1f}",
                f"{r.degradation_factor:.2f}"
            ])

    print(f"Results saved to CSV: {output_file}")


def print_summary(results: List[ConcurrencyTestResult]):
    """Print summary of results"""
    print(f"\n{'='*80}")
    print("CONCURRENT PROCESS BENCHMARK SUMMARY")
    print(f"{'='*80}\n")

    print(f"{'Processes':<12} {'Throughput':<15} {'Avg Time':<12} {'P95 Time':<12} "
          f"{'Peak Mem':<12} {'CPU':<10} {'Degradation':<12}")
    print("-" * 95)

    for result in results:
        degradation_str = f"{result.degradation_factor:.2f}x" if result.degradation_factor < 10 else ">>10x"

        print(f"{result.num_processes:<12} "
              f"{result.throughput_queries_per_second:<15.2f} "
              f"{result.avg_query_time_seconds:<12.2f} "
              f"{result.p95_query_time_seconds:<12.2f} "
              f"{result.peak_system_memory_mb/1024:<12.1f} "
              f"{result.avg_cpu_percent:<10.1f} "
              f"{degradation_str:<12}")

    # Find optimal process count
    if results:
        # Find the process count with best throughput before significant degradation
        optimal = max(
            [r for r in results if r.degradation_factor < 1.5],
            key=lambda r: r.throughput_queries_per_second,
            default=results[0]
        )

        print(f"\n{'='*80}")
        print(f"RECOMMENDATION:")
        print(f"  Optimal process count: {optimal.num_processes} processes")
        print(f"  Expected throughput: {optimal.throughput_queries_per_second:.2f} queries/second")
        print(f"  Avg query latency: {optimal.avg_query_time_seconds:.2f}s")
        print(f"  P95 query latency: {optimal.p95_query_time_seconds:.2f}s")
        print(f"  Peak memory usage: {optimal.peak_system_memory_mb/1024:.1f}GB")
        print(f"{'='*80}")


def main():
    parser = argparse.ArgumentParser(
        description='Test DuckDB with concurrent processes to determine optimal capacity',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test with different process counts
  python test_concurrent_processes.py --db-path tpch_sf16.duckdb --process-counts 4,8,16,32

  # Simulate AWS i4g.4xlarge (128GB memory, 16 vCPUs)
  python test_concurrent_processes.py \\
      --db-path tpch_sf16.duckdb \\
      --process-counts 8,16,24,32,48,64 \\
      --total-memory 128GB \\
      --output i4g_4xlarge_capacity.csv

  # Test with custom query
  python test_concurrent_processes.py \\
      --db-path tpch_sf16.duckdb \\
      --process-counts 4,8,16 \\
      --queries my_query.sql
        """
    )

    parser.add_argument('--db-path', type=str, required=True,
                       help='Path to DuckDB database file')
    parser.add_argument('--process-counts', type=str, default='4,8,16,32',
                       help='Comma-separated list of process counts to test (default: 4,8,16,32)')
    parser.add_argument('--queries', type=str,
                       help='Comma-separated list of SQL query files to execute')
    parser.add_argument('--total-memory', type=str,
                       help='Total system memory to simulate (e.g., "128GB" for i4g.4xlarge)')
    parser.add_argument('--temp-dir', type=str,
                       help='Temporary directory for spill files')
    parser.add_argument('--output', type=str,
                       help='Output CSV file')

    args = parser.parse_args()

    # Validate database
    db_path = Path(args.db_path)
    if not db_path.exists():
        print(f"Error: Database not found: {db_path}", file=sys.stderr)
        sys.exit(1)

    # Parse process counts
    process_counts = [int(p.strip()) for p in args.process_counts.split(',')]

    # Load queries
    if args.queries:
        query_files = [q.strip() for q in args.queries.split(',')]
        queries = load_queries(query_files)
    else:
        # Use default query
        example_dir = Path(__file__).parent / "example_queries"
        default_query = example_dir / "tpch_q1_aggregation.sql"
        if default_query.exists():
            queries = load_queries([str(default_query)])
        else:
            print("Error: No queries specified and no default query found", file=sys.stderr)
            sys.exit(1)

    if not queries:
        print("Error: No valid queries loaded", file=sys.stderr)
        sys.exit(1)

    print("DuckDB Concurrent Process Benchmark")
    print("=" * 80)
    print()

    # Run benchmark
    results = run_benchmark(
        db_path=str(db_path),
        process_counts=process_counts,
        queries=queries,
        total_memory=args.total_memory,
        temp_dir=args.temp_dir
    )

    # Print summary
    print_summary(results)

    # Save results
    if args.output:
        save_csv_results(results, args.output)
    else:
        save_csv_results(results, "concurrent_process_results.csv")


if __name__ == '__main__':
    main()
