#!/usr/bin/env python3
"""
DuckDB Disk Spill Benchmark Script

This script tests DuckDB's performance and spill behavior at different memory limits
to understand performance characteristics before deploying to production instances.

Recommended for local testing with SF16 (~16GB) database before deploying to AWS i4g.4xlarge.

Usage:
    python test_resource_usage.py --db-path tpch_sf16.duckdb --memory-limits 16GB,8GB,4GB,2GB,1GB
"""

import argparse
import csv
import json
import os
import platform
import signal
import sys
import threading
import time
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import List, Optional, Dict, Any

# Check for required dependencies
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
class MemoryTagStats:
    """Memory statistics for a specific tag"""
    tag: str
    memory_usage_bytes: int = 0
    temporary_storage_bytes: int = 0
    
    @property
    def memory_usage_gb(self) -> float:
        return self.memory_usage_bytes / (1024 ** 3)
    
    @property
    def temporary_storage_gb(self) -> float:
        return self.temporary_storage_bytes / (1024 ** 3)


@dataclass
class MemoryStats:
    """Memory and spill statistics from DuckDB"""
    memory_usage_bytes: int = 0
    temporary_storage_bytes: int = 0  # Logical (uncompressed) spill size
    disk_usage_bytes: int = 0  # Actual disk usage (compressed)
    compression_ratio: float = 0.0
    tag_breakdown: List[MemoryTagStats] = None
    
    def __post_init__(self):
        if self.tag_breakdown is None:
            self.tag_breakdown = []
    
    @property
    def memory_usage_gb(self) -> float:
        return self.memory_usage_bytes / (1024 ** 3)
    
    @property
    def temporary_storage_gb(self) -> float:
        return self.temporary_storage_bytes / (1024 ** 3)
    
    @property
    def disk_usage_gb(self) -> float:
        return self.disk_usage_bytes / (1024 ** 3)


@dataclass
class QueryResult:
    """Result of a single query execution"""
    memory_limit: str
    query_name: str
    execution_time_seconds: float
    success: bool
    error_message: Optional[str] = None
    rows_returned: Optional[int] = None
    memory_stats: Optional[MemoryStats] = None
    temp_storage_limit: Optional[str] = None  # For tracking temp storage limit in matrix tests


def parse_memory_limit(memory_str: str) -> int:
    """Parse memory limit string (e.g., '4GB', '8GB') to bytes"""
    memory_str = memory_str.strip().upper()
    
    # Check longer units first to avoid matching "B" in "GB"
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
    
    # If no unit, assume bytes
    return int(float(memory_str))


def format_bytes(bytes_value: int) -> str:
    """Format bytes to human-readable string"""
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if bytes_value < 1024.0:
            return f"{bytes_value:.2f} {unit}"
        bytes_value /= 1024.0
    return f"{bytes_value:.2f} PB"


def get_memory_stats(conn: duckdb.DuckDBPyConnection, temp_dir: Optional[Path] = None, 
                     include_tag_breakdown: bool = True) -> MemoryStats:
    """Get memory and spill statistics from DuckDB"""
    try:
        # Get memory usage from duckdb_memory() with tag breakdown
        if include_tag_breakdown:
            tag_results = conn.execute("""
                SELECT 
                    tag,
                    SUM(memory_usage_bytes) as memory_bytes,
                    SUM(temporary_storage_bytes) as spill_bytes
                FROM duckdb_memory()
                WHERE memory_usage_bytes > 0 OR temporary_storage_bytes > 0
                GROUP BY tag
                ORDER BY memory_bytes DESC, spill_bytes DESC
            """).fetchall()
            
            tag_breakdown = [
                MemoryTagStats(
                    tag=str(row[0]),
                    memory_usage_bytes=int(row[1] or 0),
                    temporary_storage_bytes=int(row[2] or 0)
                )
                for row in tag_results
            ]
        else:
            tag_breakdown = []
        
        # Get totals
        result = conn.execute("""
            SELECT 
                SUM(memory_usage_bytes) as total_memory,
                SUM(temporary_storage_bytes) as total_spill
            FROM duckdb_memory()
        """).fetchone()
        
        memory_bytes = int(result[0] or 0)
        spill_bytes = int(result[1] or 0)
        
        # Get actual disk usage from temp directory
        disk_bytes = 0
        if temp_dir and temp_dir.exists():
            for file in temp_dir.glob("duckdb_temp_*.tmp"):
                try:
                    disk_bytes += file.stat().st_size
                except OSError:
                    pass
        
        # Calculate compression ratio
        compression_ratio = 0.0
        if spill_bytes > 0 and disk_bytes > 0:
            compression_ratio = spill_bytes / disk_bytes
        
        return MemoryStats(
            memory_usage_bytes=memory_bytes,
            temporary_storage_bytes=spill_bytes,
            disk_usage_bytes=disk_bytes,
            compression_ratio=compression_ratio,
            tag_breakdown=tag_breakdown
        )
    except Exception:
        # If duckdb_memory() fails, return empty stats
        return MemoryStats()


def execute_query(conn: duckdb.DuckDBPyConnection, query: str, query_name: str, 
                  temp_dir: Optional[Path] = None, timeout: Optional[float] = None) -> QueryResult:
    """Execute a single query and measure execution time with spill tracking"""
    start_time = time.time()
    memory_stats = None
    query_timed_out = False
    
    def timeout_handler():
        nonlocal query_timed_out
        query_timed_out = True
        # Note: DuckDB doesn't have a direct way to cancel queries from Python
        # This flag will be checked after execution
    
    timer = None
    if timeout:
        timer = threading.Timer(timeout, timeout_handler)
        timer.start()
    
    try:
        result = conn.execute(query)
        rows = result.fetchall()
        execution_time = time.time() - start_time
        
        if timer:
            timer.cancel()
        
        if query_timed_out:
            return QueryResult(
                memory_limit="",
                query_name=query_name,
                execution_time_seconds=timeout or execution_time,
                success=False,
                error_message=f"Query timed out after {timeout}s",
                memory_stats=None
            )
        
        # Get memory stats after query execution
        memory_stats = get_memory_stats(conn, temp_dir, include_tag_breakdown=True)
        
        return QueryResult(
            memory_limit="",  # Will be set by caller
            query_name=query_name,
            execution_time_seconds=execution_time,
            success=True,
            rows_returned=len(rows) if rows else 0,
            memory_stats=memory_stats
        )
    except Exception as e:
        execution_time = time.time() - start_time
        # Try to get memory stats even on failure
        try:
            memory_stats = get_memory_stats(conn, temp_dir, include_tag_breakdown=True)
        except Exception:
            pass
        
        return QueryResult(
            memory_limit="",  # Will be set by caller
            query_name=query_name,
            execution_time_seconds=execution_time,
            success=False,
            error_message=str(e),
            memory_stats=memory_stats
        )


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


def run_benchmark(db_path: str, memory_limits: List[str], queries: Dict[str, str],
                  temp_dir: Optional[str] = None, temp_storage_limits: Optional[List[str]] = None,
                  threads: Optional[int] = None, query_timeout: Optional[float] = None) -> List[QueryResult]:
    """
    Run benchmark with memory and temp storage limits.
    
    If temp_storage_limits is provided, runs full matrix: each memory limit × each temp storage limit.
    Otherwise, uses single temp_dir for all tests.
    """
    results = []
    
    # Determine temp directories to test
    if temp_storage_limits:
        # Full matrix: test each memory limit with each temp storage limit
        temp_dirs = [Path(t.strip()) if t.strip().lower() != 'unlimited' else None 
                     for t in temp_storage_limits]
        for td in temp_dirs:
            if td:
                td.mkdir(parents=True, exist_ok=True)
    else:
        # Single temp directory for all tests
        if temp_dir:
            temp_path = Path(temp_dir)
            temp_path.mkdir(parents=True, exist_ok=True)
        else:
            temp_path = Path.home() / ".tmp"
            temp_path.mkdir(parents=True, exist_ok=True)
        temp_dirs = [temp_path]
    
    print(f"Database: {db_path}")
    if temp_storage_limits:
        print(f"Temp storage limits: {', '.join(temp_storage_limits)}")
        print(f"Full test matrix: {len(memory_limits)} memory limits × {len(temp_storage_limits)} temp storage limits = {len(memory_limits) * len(temp_storage_limits)} tests")
    else:
        print(f"Temp directory: {temp_dirs[0]}")
    if threads:
        print(f"Threads: {threads}")
    print(f"Memory limits to test: {', '.join(memory_limits)}")
    print(f"Queries to test: {', '.join(queries.keys())}")
    print()
    
    test_count = 0
    total_tests = len(memory_limits) * len(temp_dirs) * len(queries)
    
    for memory_limit in memory_limits:
        for temp_dir_idx, temp_path in enumerate(temp_dirs):
            if temp_storage_limits:
                temp_label = temp_storage_limits[temp_dir_idx]
                print(f"{'='*80}")
                print(f"Testing with memory_limit = {memory_limit}, temp_storage = {temp_label}")
                print(f"{'='*80}")
            else:
                print(f"{'='*80}")
                print(f"Testing with memory_limit = {memory_limit}")
                print(f"{'='*80}")
        print(f"{'='*80}")
        print(f"Testing with memory_limit = {memory_limit}")
        print(f"{'='*80}")
        
        # Clear temp files before each test
        try:
            for file in temp_path.glob("duckdb_temp_*.tmp"):
                file.unlink()
        except Exception:
            pass
        
        # Connect to database with memory limit
        config = {
            'max_memory': memory_limit,
        }
        if threads:
            config['threads'] = threads
        
        conn = duckdb.connect(db_path, config=config)
        
        # Set temp directory
        try:
            if temp_path:
                conn.execute(f"SET temp_directory = '{temp_path}'")
        except Exception:
            pass
        
        # Run each query
        for query_name, query in queries.items():
            test_count += 1
            print(f"  [{test_count}/{total_tests}] Executing query: {query_name}...", end=' ', flush=True)
            
            result = execute_query(conn, query, query_name, temp_path, timeout=query_timeout)
            result.memory_limit = memory_limit
            # Add temp storage info to result for tracking
            if temp_storage_limits:
                result.temp_storage_limit = temp_storage_limits[temp_dir_idx]
            results.append(result)
            
            if result.success:
                spill_info = ""
                if result.memory_stats and result.memory_stats.temporary_storage_gb > 0:
                    spill_info = f", spill: {result.memory_stats.temporary_storage_gb:.2f}GB"
                    if result.memory_stats.compression_ratio > 0:
                        spill_info += f" (disk: {result.memory_stats.disk_usage_gb:.2f}GB, "
                        spill_info += f"ratio: {result.memory_stats.compression_ratio:.2f}x)"
                print(f"✓ ({result.execution_time_seconds:.3f}s{spill_info})")
            else:
                print(f"✗ Failed: {result.error_message}")
        
        conn.close()
        print()
    
    return results


def save_csv_results(results: List[QueryResult], output_file: str):
    """Save results to CSV matching the format in duckdb-disk-spill-analysis.md"""
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        # Header - include temp_storage_limit if any results have it
        has_temp_storage = any(r.temp_storage_limit for r in results)
        header = ['memory_limit', 'query', 'exec_time_sec', 'mem_usage_gb',
                  'temp_storage_gb', 'disk_usage_gb', 'compression_ratio']
        if has_temp_storage:
            header.insert(2, 'temp_storage_limit')
        writer.writerow(header)
        
        # Data rows
        for result in results:
            if result.memory_stats:
                mem_gb = result.memory_stats.memory_usage_gb
                spill_gb = result.memory_stats.temporary_storage_gb
                disk_gb = result.memory_stats.disk_usage_gb
                comp_ratio = result.memory_stats.compression_ratio if result.memory_stats.compression_ratio > 0 else "N/A"
            else:
                mem_gb = 0.0
                spill_gb = 0.0
                disk_gb = 0.0
                comp_ratio = "N/A"
            
            row = [
                result.memory_limit,
                result.query_name,
            ]
            if has_temp_storage:
                row.append(result.temp_storage_limit or "unlimited")
            row.extend([
                f"{result.execution_time_seconds:.3f}",
                f"{mem_gb:.2f}",
                f"{spill_gb:.2f}",
                f"{disk_gb:.2f}",
                f"{comp_ratio:.2f}" if isinstance(comp_ratio, float) else comp_ratio
            ])
            writer.writerow(row)
    
    print(f"Results saved to CSV: {output_file}")


def save_json_results(results: List[QueryResult], output_file: str):
    """Save results to JSON for detailed analysis"""
    output_data = {
        'timestamp': datetime.now().isoformat(),
        'system': {
            'platform': platform.platform(),
            'python_version': sys.version,
        },
        'results': [asdict(r) for r in results]
    }
    
    with open(output_file, 'w') as f:
        json.dump(output_data, f, indent=2, default=str)
    
    print(f"Results saved to JSON: {output_file}")


def print_summary(results: List[QueryResult]):
    """Print summary of results"""
    print(f"\n{'='*80}")
    print("BENCHMARK SUMMARY")
    print(f"{'='*80}")
    
    # Group by query
    queries = {}
    for result in results:
        if result.query_name not in queries:
            queries[result.query_name] = []
        queries[result.query_name].append(result)
    
    for query_name, query_results in queries.items():
        print(f"\nQuery: {query_name}")
        print(f"{'Memory Limit':<15} {'Time (s)':<12} {'Memory (GB)':<15} {'Spill (GB)':<15} {'Disk (GB)':<15} {'Comp Ratio':<12}")
        print("-" * 100)
        
        for result in sorted(query_results, key=lambda r: parse_memory_limit(r.memory_limit)):
            if result.memory_stats:
                mem_gb = result.memory_stats.memory_usage_gb
                spill_gb = result.memory_stats.temporary_storage_gb
                disk_gb = result.memory_stats.disk_usage_gb
                comp_ratio = f"{result.memory_stats.compression_ratio:.2f}x" if result.memory_stats.compression_ratio > 0 else "N/A"
            else:
                mem_gb = 0.0
                spill_gb = 0.0
                disk_gb = 0.0
                comp_ratio = "N/A"
            
            status = "✓" if result.success else "✗"
            print(f"{result.memory_limit:<15} {result.execution_time_seconds:<12.3f} "
                  f"{mem_gb:<15.2f} {spill_gb:<15.2f} {disk_gb:<15.2f} {comp_ratio:<12}")
        
        # Calculate slowdown (compare to fastest, not lowest memory)
        if len(query_results) > 1:
            successful_results = [r for r in query_results if r.success]
            if successful_results:
                baseline = min(successful_results, key=lambda r: r.execution_time_seconds)
                if baseline:
                    print(f"\n  Slowdown from {baseline.memory_limit} baseline ({baseline.execution_time_seconds:.3f}s):")
                    for result in sorted(query_results, key=lambda r: parse_memory_limit(r.memory_limit)):
                        if result.success:
                            if result == baseline:
                                print(f"    {result.memory_limit}: 1.00x (baseline)")
                            else:
                                slowdown = result.execution_time_seconds / baseline.execution_time_seconds
                                if slowdown > 1.0:
                                    print(f"    {result.memory_limit}: {slowdown:.2f}x slower")
                                else:
                                    print(f"    {result.memory_limit}: {1.0/slowdown:.2f}x faster")


def main():
    parser = argparse.ArgumentParser(
        description='DuckDB Disk Spill Benchmark - Test performance at different memory limits',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test with default memory limits and queries (recommended for local testing)
  python test_resource_usage.py --db-path tpch_sf16.duckdb

  # Test with specific memory limits
  python test_resource_usage.py --db-path tpch_sf16.duckdb --memory-limits 16GB,8GB,4GB,2GB,1GB

  # Test with specific queries
  python test_resource_usage.py --db-path tpch_sf16.duckdb --queries example_queries/tpch_q1_aggregation.sql,example_queries/tpch_q3_join.sql

  # Save to CSV
  python test_resource_usage.py --db-path tpch_sf16.duckdb --output local_memory_results.csv

  # Full example with temp directory
  python test_resource_usage.py \\
      --db-path tpch_sf16.duckdb \\
      --memory-limits 16GB,8GB,4GB,2GB,1GB \\
      --queries example_queries/tpch_q1_aggregation.sql,example_queries/tpch_q3_join.sql \\
      --output local_memory_results.csv \\
      --temp-dir ~/duckdb_temp \\
      --query-timeout 600

  # Full test matrix (5 memory limits × 4 temp storage limits = 20 tests per query)
  # Requires pre-configured temp directories with size limits (see README)
  python test_resource_usage.py \\
      --db-path tpch_sf16.duckdb \\
      --memory-limits 16GB,8GB,4GB,2GB,1GB \\
      --temp-storage-limits unlimited,16GB,8GB,4GB \\
      --output full_matrix_results.csv \\
      --query-timeout 600
        """
    )
    
    parser.add_argument(
        '--db-path',
        type=str,
        required=True,
        help='Path to DuckDB database file'
    )
    
    parser.add_argument(
        '--memory-limits',
        type=str,
        default='16GB,8GB,4GB,2GB,1GB',
        help='Comma-separated list of memory limits to test (default: 16GB,8GB,4GB,2GB,1GB)'
    )
    
    parser.add_argument(
        '--queries',
        type=str,
        help='Comma-separated list of SQL query files to execute'
    )
    
    parser.add_argument(
        '--output',
        type=str,
        help='Output file (CSV or JSON based on extension, or use --format)'
    )
    
    parser.add_argument(
        '--format',
        type=str,
        choices=['csv', 'json', 'both'],
        default='csv',
        help='Output format: csv (default, matches document), json, or both'
    )
    
    parser.add_argument(
        '--temp-dir',
        type=str,
        help='Temporary directory for spill files (default: ~/.tmp). Use with --temp-storage-limits for full matrix testing.'
    )
    
    parser.add_argument(
        '--temp-storage-limits',
        type=str,
        help='Comma-separated list of temp storage limits to test (e.g., "unlimited,16GB,8GB,4GB"). '
             'Each value should be a directory path or "unlimited". '
             'When specified, runs full matrix: each memory limit × each temp storage limit. '
             'Directories must be pre-configured with size limits (see README).'
    )
    
    parser.add_argument(
        '--threads',
        type=int,
        help='Number of threads to use (default: all available)'
    )
    
    parser.add_argument(
        '--query-timeout',
        type=float,
        help='Timeout in seconds for each query (default: no timeout)'
    )
    
    args = parser.parse_args()
    
    # Validate database path
    db_path = Path(args.db_path)
    if not db_path.exists():
        print(f"Error: Database file not found: {db_path}", file=sys.stderr)
        sys.exit(1)
    
    # Parse memory limits
    memory_limits = [m.strip() for m in args.memory_limits.split(',')]
    
    # Parse temp storage limits if provided
    temp_storage_limits = None
    if args.temp_storage_limits:
        temp_storage_limits = [t.strip() for t in args.temp_storage_limits.split(',')]
        print(f"Note: Running full test matrix with temp storage limits. Ensure directories are pre-configured.")
        print(f"See README for setup instructions.")
        print()
    
    # Load queries
    if args.queries:
        query_files = [q.strip() for q in args.queries.split(',')]
        queries = load_queries(query_files)
        if not queries:
            print("Error: No valid query files found", file=sys.stderr)
            sys.exit(1)
    else:
        # Default queries from example_queries directory
        example_dir = Path(__file__).parent / "example_queries"
        default_queries = [
            example_dir / "tpch_q1_aggregation.sql",
            example_dir / "tpch_q2_sort.sql",
            example_dir / "tpch_q3_join.sql",
        ]
        queries = load_queries([str(q) for q in default_queries if q.exists()])
        if not queries:
            print("Warning: No default queries found. Use --queries to specify query files.", file=sys.stderr)
            sys.exit(1)
    
    print("DuckDB Disk Spill Benchmark")
    print("=" * 80)
    
    # Run benchmark
    results = run_benchmark(
        db_path=str(db_path),
        memory_limits=memory_limits,
        queries=queries,
        temp_dir=args.temp_dir,
        temp_storage_limits=temp_storage_limits,
        threads=args.threads,
        query_timeout=args.query_timeout
    )
    
    # Print summary
    print_summary(results)
    
    # Save results
    if args.output:
        if args.format in ['csv', 'both']:
            csv_file = args.output if args.output.endswith('.csv') else args.output + '.csv'
            save_csv_results(results, csv_file)
        if args.format in ['json', 'both']:
            json_file = args.output if args.output.endswith('.json') else args.output + '.json'
            save_json_results(results, json_file)
    elif args.format == 'csv':
        # Default CSV output if no file specified
        default_csv = "local_memory_results.csv"
        save_csv_results(results, default_csv)


if __name__ == '__main__':
    main()
