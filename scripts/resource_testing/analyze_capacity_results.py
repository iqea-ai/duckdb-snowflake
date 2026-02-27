#!/usr/bin/env python3
"""
DuckDB Capacity Test Results Analyzer

Analyzes results from capacity testing and generates reports, visualizations,
and actionable recommendations for EC2 deployment.

Usage:
    # Analyze concurrent process test results
    python analyze_capacity_results.py --input concurrent_process_results.csv

    # Analyze EC2 capacity test results
    python analyze_capacity_results.py --input capacity_*_results.csv --output report.txt

    # Generate comparison chart
    python analyze_capacity_results.py --input capacity_*_results.csv --chart capacity_chart.txt
"""

import argparse
import csv
import glob
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict, Optional, Tuple


@dataclass
class TestResult:
    """Single test result"""
    num_processes: int
    memory_per_process: str
    total_time_sec: float
    avg_query_time_sec: float
    median_query_time_sec: float
    p95_query_time_sec: float
    p99_query_time_sec: float
    throughput_qps: float
    successful_queries: int
    failed_queries: int
    peak_memory_mb: float
    avg_cpu_percent: float
    peak_cpu_percent: float
    avg_io_wait_percent: float
    degradation_factor: float


def load_results(file_pattern: str) -> List[TestResult]:
    """Load results from CSV file(s)"""
    results = []

    # Handle glob patterns
    files = glob.glob(file_pattern)
    if not files:
        files = [file_pattern]

    for file_path in files:
        path = Path(file_path)
        if not path.exists():
            continue

        with open(path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    result = TestResult(
                        num_processes=int(row['num_processes']),
                        memory_per_process=row['memory_per_process'],
                        total_time_sec=float(row['total_time_sec']),
                        avg_query_time_sec=float(row['avg_query_time_sec']),
                        median_query_time_sec=float(row['median_query_time_sec']),
                        p95_query_time_sec=float(row['p95_query_time_sec']),
                        p99_query_time_sec=float(row['p99_query_time_sec']),
                        throughput_qps=float(row['throughput_qps']),
                        successful_queries=int(row['successful_queries']),
                        failed_queries=int(row['failed_queries']),
                        peak_memory_mb=float(row['peak_memory_mb']),
                        avg_cpu_percent=float(row['avg_cpu_percent']),
                        peak_cpu_percent=float(row['peak_cpu_percent']),
                        avg_io_wait_percent=float(row['avg_io_wait_percent']),
                        degradation_factor=float(row['degradation_factor'])
                    )
                    results.append(result)
                except (KeyError, ValueError) as e:
                    print(f"Warning: Skipping invalid row in {file_path}: {e}", file=sys.stderr)
                    continue

    return results


def find_optimal_configuration(results: List[TestResult]) -> Optional[TestResult]:
    """Find optimal configuration based on throughput and degradation"""
    # Filter: no failures, degradation < 2x
    valid_results = [
        r for r in results
        if r.failed_queries == 0 and r.degradation_factor < 2.0
    ]

    if not valid_results:
        # Fallback: no failures
        valid_results = [r for r in results if r.failed_queries == 0]

    if not valid_results:
        # Last resort: use all
        valid_results = results

    if not valid_results:
        return None

    # Find best throughput
    return max(valid_results, key=lambda r: r.throughput_qps)


def find_saturation_point(results: List[TestResult]) -> Optional[int]:
    """Find the process count where performance starts degrading significantly"""
    if len(results) < 2:
        return None

    # Sort by process count
    sorted_results = sorted(results, key=lambda r: r.num_processes)

    # Find where degradation factor > 1.5 or failures start
    for i, result in enumerate(sorted_results):
        if result.degradation_factor > 1.5 or result.failed_queries > 0:
            # Return the previous process count (before degradation)
            if i > 0:
                return sorted_results[i - 1].num_processes
            else:
                return result.num_processes

    # No saturation found - return max tested
    return sorted_results[-1].num_processes


def generate_ascii_chart(results: List[TestResult], metric: str = 'throughput') -> str:
    """Generate ASCII chart for visualization"""
    if not results:
        return "No data to chart"

    # Sort by process count
    sorted_results = sorted(results, key=lambda r: r.num_processes)

    # Select metric to chart
    if metric == 'throughput':
        values = [r.throughput_qps for r in sorted_results]
        title = "Throughput (queries/second)"
    elif metric == 'latency':
        values = [r.p95_query_time_sec for r in sorted_results]
        title = "P95 Latency (seconds)"
    elif metric == 'memory':
        values = [r.peak_memory_mb / 1024 for r in sorted_results]
        title = "Peak Memory (GB)"
    elif metric == 'cpu':
        values = [r.avg_cpu_percent for r in sorted_results]
        title = "Avg CPU (%)"
    else:
        values = [r.throughput_qps for r in sorted_results]
        title = "Throughput (queries/second)"

    process_counts = [r.num_processes for r in sorted_results]

    # Chart dimensions
    chart_width = 60
    chart_height = 15

    # Normalize values to chart height
    max_value = max(values) if values else 1
    min_value = min(values) if values else 0
    value_range = max_value - min_value if max_value != min_value else 1

    # Build chart
    chart_lines = []
    chart_lines.append(f"\n{title}")
    chart_lines.append("=" * chart_width)

    # Y-axis labels and bars
    for i in range(chart_height, -1, -1):
        # Y-axis label
        y_value = min_value + (value_range * i / chart_height)
        label = f"{y_value:6.1f} |"

        # Plot points
        line = label
        for j, value in enumerate(values):
            normalized = (value - min_value) / value_range * chart_height
            if abs(normalized - i) < 0.5:
                line += " ●"
            else:
                line += "  "

        chart_lines.append(line)

    # X-axis
    x_axis = " " * 8 + "+" + "-" * (chart_width - 8)
    chart_lines.append(x_axis)

    # X-axis labels
    x_labels = " " * 8
    for count in process_counts:
        x_labels += f" {count}"
    chart_lines.append(x_labels)
    chart_lines.append(" " * 8 + "Process Count")

    return "\n".join(chart_lines)


def generate_report(results: List[TestResult], output_file: Optional[str] = None):
    """Generate comprehensive analysis report"""
    if not results:
        print("No results to analyze")
        return

    report_lines = []
    report_lines.append("=" * 80)
    report_lines.append("DUCKDB CAPACITY TEST ANALYSIS REPORT")
    report_lines.append("=" * 80)
    report_lines.append("")

    # Summary statistics
    report_lines.append("SUMMARY")
    report_lines.append("-" * 80)
    report_lines.append(f"Total tests: {len(results)}")
    report_lines.append(f"Process counts tested: {sorted(set(r.num_processes for r in results))}")
    total_failures = sum(r.failed_queries for r in results)
    report_lines.append(f"Total failures: {total_failures}")
    report_lines.append("")

    # Find optimal configuration
    optimal = find_optimal_configuration(results)
    if optimal:
        report_lines.append("OPTIMAL CONFIGURATION")
        report_lines.append("-" * 80)
        report_lines.append(f"Process count: {optimal.num_processes}")
        report_lines.append(f"Memory per process: {optimal.memory_per_process}")
        report_lines.append(f"Throughput: {optimal.throughput_qps:.2f} queries/second")
        report_lines.append(f"Avg latency: {optimal.avg_query_time_sec:.2f}s")
        report_lines.append(f"P95 latency: {optimal.p95_query_time_sec:.2f}s")
        report_lines.append(f"P99 latency: {optimal.p99_query_time_sec:.2f}s")
        report_lines.append(f"Peak memory: {optimal.peak_memory_mb/1024:.1f}GB")
        report_lines.append(f"Avg CPU: {optimal.avg_cpu_percent:.1f}%")
        report_lines.append(f"Degradation factor: {optimal.degradation_factor:.2f}x")
        report_lines.append("")

    # Find saturation point
    saturation = find_saturation_point(results)
    if saturation:
        report_lines.append("SATURATION ANALYSIS")
        report_lines.append("-" * 80)
        report_lines.append(f"Performance starts degrading at: {saturation} processes")
        report_lines.append(f"Recommendation: Stay at or below {saturation} concurrent processes")
        report_lines.append("")

    # Detailed results table
    report_lines.append("DETAILED RESULTS")
    report_lines.append("-" * 80)
    report_lines.append(f"{'Proc':<6} {'Memory':<10} {'Throughput':<12} {'P95 Lat':<10} "
                       f"{'Memory':<10} {'CPU':<8} {'Degrad':<8} {'Fail':<6}")
    report_lines.append(f"{'Count':<6} {'Per Proc':<10} {'(q/s)':<12} {'(sec)':<10} "
                       f"{'(GB)':<10} {'(%)':<8} {'Factor':<8} {'#':<6}")
    report_lines.append("-" * 80)

    for result in sorted(results, key=lambda r: r.num_processes):
        memory_gb = result.peak_memory_mb / 1024
        degrad_str = f"{result.degradation_factor:.2f}x" if result.degradation_factor < 10 else ">>10x"
        fail_indicator = "✗" if result.failed_queries > 0 else ""

        report_lines.append(
            f"{result.num_processes:<6} "
            f"{result.memory_per_process:<10} "
            f"{result.throughput_qps:<12.2f} "
            f"{result.p95_query_time_sec:<10.2f} "
            f"{memory_gb:<10.1f} "
            f"{result.avg_cpu_percent:<8.1f} "
            f"{degrad_str:<8} "
            f"{fail_indicator:<6}"
        )

    report_lines.append("")

    # Performance patterns
    report_lines.append("PERFORMANCE PATTERNS")
    report_lines.append("-" * 80)

    sorted_results = sorted(results, key=lambda r: r.num_processes)
    if len(sorted_results) >= 2:
        # Check for linear scaling
        first = sorted_results[0]
        last = sorted_results[-1]

        theoretical_speedup = last.num_processes / first.num_processes
        actual_speedup = last.throughput_qps / first.throughput_qps if first.throughput_qps > 0 else 0
        scaling_efficiency = (actual_speedup / theoretical_speedup * 100) if theoretical_speedup > 0 else 0

        report_lines.append(f"Theoretical speedup ({first.num_processes}→{last.num_processes} proc): {theoretical_speedup:.2f}x")
        report_lines.append(f"Actual speedup: {actual_speedup:.2f}x")
        report_lines.append(f"Scaling efficiency: {scaling_efficiency:.1f}%")

        if scaling_efficiency > 80:
            report_lines.append("  → Excellent scaling - nearly linear")
        elif scaling_efficiency > 60:
            report_lines.append("  → Good scaling - some overhead")
        elif scaling_efficiency > 40:
            report_lines.append("  → Moderate scaling - significant overhead")
        else:
            report_lines.append("  → Poor scaling - severe bottleneck")

        report_lines.append("")

    # Resource utilization analysis
    report_lines.append("RESOURCE UTILIZATION")
    report_lines.append("-" * 80)

    avg_cpu = sum(r.avg_cpu_percent for r in results) / len(results)
    max_memory_gb = max(r.peak_memory_mb for r in results) / 1024

    report_lines.append(f"Average CPU utilization: {avg_cpu:.1f}%")
    report_lines.append(f"Peak memory usage: {max_memory_gb:.1f}GB")

    if avg_cpu < 50:
        report_lines.append("  → CPU is underutilized - consider increasing process count")
    elif avg_cpu > 90:
        report_lines.append("  → CPU is saturated - consider decreasing process count")
    else:
        report_lines.append("  → CPU utilization is healthy")

    report_lines.append("")

    # Recommendations
    report_lines.append("RECOMMENDATIONS")
    report_lines.append("-" * 80)

    if optimal:
        report_lines.append(f"1. Deploy with {optimal.num_processes} concurrent DuckDB processes")
        report_lines.append(f"2. Allocate {optimal.memory_per_process} memory per process")
        report_lines.append(f"3. Expect ~{optimal.throughput_qps:.0f} queries/second throughput")
        report_lines.append(f"4. Expect ~{optimal.p95_query_time_sec:.2f}s P95 latency")

        # Additional recommendations based on patterns
        if optimal.avg_cpu_percent < 50:
            report_lines.append(f"5. CPU is underutilized - test with more processes ({optimal.num_processes * 2}+)")
        elif optimal.degradation_factor > 1.3:
            report_lines.append(f"5. Some degradation detected - consider testing with fewer processes")

        if total_failures > 0:
            report_lines.append(f"6. ⚠ Failures detected - review query patterns and memory limits")

    report_lines.append("")
    report_lines.append("=" * 80)

    report = "\n".join(report_lines)

    # Print report
    print(report)

    # Save to file if specified
    if output_file:
        with open(output_file, 'w') as f:
            f.write(report)
        print(f"\nReport saved to: {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze DuckDB capacity test results',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Analyze single result file
  python analyze_capacity_results.py --input concurrent_process_results.csv

  # Analyze multiple files
  python analyze_capacity_results.py --input "capacity_*_results.csv"

  # Generate report and charts
  python analyze_capacity_results.py --input concurrent_process_results.csv \\
      --output report.txt --chart all
        """
    )

    parser.add_argument('--input', type=str, required=True,
                       help='Input CSV file(s) - supports glob patterns')
    parser.add_argument('--output', type=str,
                       help='Output file for report (default: print to stdout)')
    parser.add_argument('--chart', type=str, choices=['throughput', 'latency', 'memory', 'cpu', 'all'],
                       help='Generate ASCII chart for specified metric')

    args = parser.parse_args()

    # Load results
    print(f"Loading results from: {args.input}")
    results = load_results(args.input)

    if not results:
        print("Error: No results loaded", file=sys.stderr)
        sys.exit(1)

    print(f"Loaded {len(results)} test result(s)\n")

    # Generate report
    generate_report(results, args.output)

    # Generate charts if requested
    if args.chart:
        print("\n")
        if args.chart == 'all':
            for metric in ['throughput', 'latency', 'memory', 'cpu']:
                print(generate_ascii_chart(results, metric))
                print()
        else:
            print(generate_ascii_chart(results, args.chart))
            print()


if __name__ == '__main__':
    main()
