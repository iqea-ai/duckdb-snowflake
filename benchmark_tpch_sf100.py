#!/usr/bin/env python3
"""
Benchmark TPC-H SF100 generation and query execution

Runs the script multiple times, deleting the database before each run,
and collects statistics on execution time.

Usage:
    python benchmark_tpch_sf100.py --runs 10 --data-dir tpch_sf100_data
"""

import argparse
import os
import subprocess
import sys
import time
import shutil
from pathlib import Path
import statistics
from typing import List, Tuple
import time as time_module  # For cleanup function


def parse_time_from_output(output: str) -> Tuple[float, float]:
    """Extract generation time and total query time from script output"""
    generation_time = None
    total_query_time = None
    
    lines = output.split('\n')
    for i, line in enumerate(lines):
        # Look for generation time
        if "Data generation completed in" in line:
            # Extract seconds from line like "✓ Data generation completed in 588.3 seconds (9.8 minutes)"
            try:
                parts = line.split("Data generation completed in")[1].strip().split()[0]
                generation_time = float(parts)
            except (ValueError, IndexError):
                pass
        
        # Look for total query time
        if "Total time:" in line and "seconds" in line:
            # Extract seconds from line like "Total time: 53.76 seconds (0.90 minutes)"
            try:
                parts = line.split("Total time:")[1].strip().split()[0]
                total_query_time = float(parts)
            except (ValueError, IndexError):
                pass
    
    return generation_time, total_query_time


def run_single_benchmark(data_dir: str, scale_factor: float, skip_queries: bool = False) -> Tuple[float, float, float]:
    """
    Run a single benchmark iteration
    Returns: (wall_clock_time, generation_time, query_time)
    """
    script_path = Path(__file__).parent / "run_tpch_sf100.py"
    
    cmd = [
        sys.executable,
        str(script_path),
        "--data-dir", data_dir,
        "--scale-factor", str(scale_factor),
        "--overwrite",  # Overwrite existing database
        "--yes"  # Skip prompts
    ]
    
    if skip_queries:
        # We'll need to add a --skip-queries option or just time generation
        pass
    
    start_time = time.time()
    script_dir = Path(__file__).parent
    
    # Set up environment - remove current directory from Python path to avoid shadowing duckdb package
    env = dict(os.environ)
    # Use absolute path for script and run from parent directory to avoid duckdb directory shadowing
    original_pythonpath = env.get('PYTHONPATH', '')
    # Don't add current directory to path if it would shadow duckdb
    env['PYTHONPATH'] = original_pythonpath if original_pythonpath else ''
    
    # Calculate timeout: scale_factor * 2 minutes (generous timeout for SF100 = 200 minutes)
    # Add 30 minutes buffer for query execution
    timeout_seconds = int(scale_factor * 2 * 60 + 30 * 60)  # Convert to seconds
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=str(script_dir.parent),  # Run from parent directory to avoid duckdb shadowing
            timeout=timeout_seconds
        )
    except subprocess.TimeoutExpired:
        print(f"  ⚠ Benchmark timed out after {timeout_seconds/60:.1f} minutes")
        return None, None, None
    
    wall_clock_time = time.time() - start_time
    
    if result.returncode != 0:
        print(f"Error running benchmark: {result.stderr}", file=sys.stderr)
        print(f"Stdout: {result.stdout}", file=sys.stderr)
        return None, None, None
    
    generation_time, query_time = parse_time_from_output(result.stdout)
    
    return wall_clock_time, generation_time, query_time


def cleanup_data_dir(data_dir: Path):
    """Remove the data directory if it exists"""
    if data_dir.exists():
        print(f"  Cleaning up {data_dir}...")
        try:
            shutil.rmtree(data_dir)
            # Wait a moment to ensure filesystem operations complete
            import time
            time.sleep(0.5)
            print(f"  ✓ Cleaned up")
        except Exception as e:
            print(f"  ⚠ Cleanup warning: {e}")
            # Try again
            try:
                shutil.rmtree(data_dir, ignore_errors=True)
                time.sleep(0.5)
                print(f"  ✓ Cleaned up (with ignore_errors)")
            except:
                pass


def main():
    parser = argparse.ArgumentParser(
        description='Benchmark TPC-H SF100 generation and query execution',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run 10 iterations
  python benchmark_tpch_sf100.py --runs 10

  # Run 30 iterations (recommended for statistical significance)
  python benchmark_tpch_sf100.py --runs 30 --data-dir tpch_sf100_data

  # Quick test with 3 runs
  python benchmark_tpch_sf100.py --runs 3

Note: Each run will delete and regenerate the database, so this will take a while.
For SF100, expect ~10 minutes per run.
        """
    )
    
    parser.add_argument(
        '--runs',
        type=int,
        default=10,
        help='Number of runs to perform (default: 10, recommended: 30+ for statistical significance)'
    )
    
    parser.add_argument(
        '--data-dir',
        type=str,
        default='tpch_sf100_data',
        help='Data directory to use (default: tpch_sf100_data)'
    )
    
    parser.add_argument(
        '--scale-factor',
        type=float,
        default=100.0,
        help='TPC-H scale factor (default: 100)'
    )
    
    parser.add_argument(
        '--min-runs',
        type=int,
        default=5,
        help='Minimum number of runs for meaningful statistics (default: 5)'
    )
    
    parser.add_argument(
        '--yes',
        '-y',
        action='store_true',
        help='Skip confirmation prompts'
    )
    
    args = parser.parse_args()
    
    if args.runs < args.min_runs:
        print(f"Warning: {args.runs} runs may not be sufficient for reliable statistics.", file=sys.stderr)
        print(f"Recommended minimum: {args.min_runs} runs", file=sys.stderr)
        print(f"For normal distribution: 30+ runs recommended", file=sys.stderr)
        if not args.yes:
            try:
                response = input(f"Continue with {args.runs} runs? (y/N): ")
                if response.lower() != 'y':
                    sys.exit(0)
            except EOFError:
                print("Error: Cannot prompt for confirmation in non-interactive mode.", file=sys.stderr)
                print("Use --yes flag to skip confirmation.", file=sys.stderr)
                sys.exit(1)
    
    data_dir = Path(args.data_dir)
    
    # Clean up before starting (in case of previous incomplete run)
    if data_dir.exists():
        print("Initial cleanup of existing data directory...")
        cleanup_data_dir(data_dir)
        print()
    
    print("=" * 80)
    print("TPC-H SF100 Benchmark")
    print("=" * 80)
    print(f"Scale factor: {args.scale_factor}")
    print(f"Data directory: {data_dir.resolve()}")
    print(f"Number of runs: {args.runs}")
    print(f"Estimated time per run: ~10 minutes (for SF100)")
    print(f"Total estimated time: ~{args.runs * 10} minutes")
    print("=" * 80)
    print()
    
    wall_clock_times = []
    generation_times = []
    query_times = []
    
    for run_num in range(1, args.runs + 1):
        print(f"Run {run_num}/{args.runs}")
        print("-" * 80)
        
        # Clean up before each run
        cleanup_data_dir(data_dir)
        
        # Verify cleanup worked and wait a bit
        time_module.sleep(0.2)
        if data_dir.exists():
            print(f"  ⚠ Warning: Directory still exists after cleanup, forcing removal...")
            try:
                # Try to remove database file specifically first
                db_file = data_dir / "tpch_sf100.duckdb"
                if db_file.exists():
                    db_file.unlink(missing_ok=True)
                # Then remove directory
                shutil.rmtree(data_dir, ignore_errors=True)
                time_module.sleep(0.3)
            except Exception as e:
                print(f"  ⚠ Could not force cleanup: {e}")
                # Continue anyway - --overwrite flag will handle it
        
        # Run benchmark
        print(f"  Starting benchmark...")
        wall_time, gen_time, query_time = run_single_benchmark(
            str(data_dir),
            args.scale_factor
        )
        
        if wall_time is None:
            print(f"  ✗ Run {run_num} failed")
            continue
        
        wall_clock_times.append(wall_time)
        if gen_time is not None:
            generation_times.append(gen_time)
        if query_time is not None:
            query_times.append(query_time)
        
        print(f"  ✓ Run {run_num} completed")
        print(f"    Wall clock time: {wall_time:.1f}s ({wall_time/60:.1f} min)")
        if gen_time:
            print(f"    Generation time: {gen_time:.1f}s ({gen_time/60:.1f} min)")
        if query_time:
            print(f"    Query time: {query_time:.1f}s ({query_time/60:.2f} min)")
        print()
    
    # Calculate statistics
    print("=" * 80)
    print("STATISTICS")
    print("=" * 80)
    
    if len(wall_clock_times) == 0:
        print("Error: No successful runs")
        sys.exit(1)
    
    print(f"\nSuccessful runs: {len(wall_clock_times)}/{args.runs}")
    print()
    
    # Wall clock time statistics
    print("Wall Clock Time (Total Execution Time):")
    print(f"  Mean:   {statistics.mean(wall_clock_times):.2f}s ({statistics.mean(wall_clock_times)/60:.2f} min)")
    if len(wall_clock_times) > 1:
        print(f"  StdDev: {statistics.stdev(wall_clock_times):.2f}s ({statistics.stdev(wall_clock_times)/60:.2f} min)")
        print(f"  Min:    {min(wall_clock_times):.2f}s ({min(wall_clock_times)/60:.2f} min)")
        print(f"  Max:    {max(wall_clock_times):.2f}s ({max(wall_clock_times)/60:.2f} min)")
        # Coefficient of variation (CV) as percentage
        cv = (statistics.stdev(wall_clock_times) / statistics.mean(wall_clock_times)) * 100
        print(f"  CV:     {cv:.2f}% (lower is more consistent)")
    print()
    
    # Generation time statistics
    if generation_times:
        print("Data Generation Time:")
        print(f"  Mean:   {statistics.mean(generation_times):.2f}s ({statistics.mean(generation_times)/60:.2f} min)")
        if len(generation_times) > 1:
            print(f"  StdDev: {statistics.stdev(generation_times):.2f}s ({statistics.stdev(generation_times)/60:.2f} min)")
            print(f"  Min:    {min(generation_times):.2f}s ({min(generation_times)/60:.2f} min)")
            print(f"  Max:    {max(generation_times):.2f}s ({max(generation_times)/60:.2f} min)")
            cv = (statistics.stdev(generation_times) / statistics.mean(generation_times)) * 100
            print(f"  CV:     {cv:.2f}%")
        print()
    
    # Query time statistics
    if query_times:
        print("Query Execution Time:")
        print(f"  Mean:   {statistics.mean(query_times):.2f}s ({statistics.mean(query_times)/60:.2f} min)")
        if len(query_times) > 1:
            print(f"  StdDev: {statistics.stdev(query_times):.2f}s ({statistics.stdev(query_times)/60:.2f} min)")
            print(f"  Min:    {min(query_times):.2f}s ({min(query_times)/60:.2f} min)")
            print(f"  Max:    {max(query_times):.2f}s ({max(query_times)/60:.2f} min)")
            cv = (statistics.stdev(query_times) / statistics.mean(query_times)) * 100
            print(f"  CV:     {cv:.2f}%")
        print()
    
    # Recommendations
    print("=" * 80)
    print("RECOMMENDATIONS")
    print("=" * 80)
    
    if len(wall_clock_times) < 30:
        print(f"\nFor more reliable statistics (normal distribution), consider running 30+ iterations.")
        print(f"Current: {len(wall_clock_times)} runs")
        print(f"Recommended: 30-50 runs for statistical significance")
        print()
        print("Statistical guidelines:")
        print("  - Minimum: 5 runs (basic statistics)")
        print("  - Good: 10-20 runs (better estimates)")
        print("  - Excellent: 30+ runs (normal distribution, confidence intervals)")
    else:
        print(f"\n✓ Sufficient runs ({len(wall_clock_times)}) for reliable statistics")
    
    print()
    print("=" * 80)
    
    # Final cleanup (optional - comment out if you want to keep the last run)
    # cleanup_data_dir(data_dir)


if __name__ == '__main__':
    main()

