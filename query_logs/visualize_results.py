#!/usr/bin/env python3
"""
Visualize Snowflake Query Log Analysis Results

Creates charts and reports from the query classification data to help
understand caching opportunities and cost savings potential.

Usage:
    python visualize_results.py [--db query_analysis.db] [--output-dir reports]

Requirements:
    pip install duckdb pandas matplotlib seaborn
"""

import argparse
import os
from pathlib import Path

import duckdb
import pandas as pd

# Try to import visualization libraries
try:
    import matplotlib.pyplot as plt
    import seaborn as sns
    HAS_VIZ = True
except ImportError:
    HAS_VIZ = False
    print("Warning: matplotlib/seaborn not installed. Charts will be skipped.")
    print("Install with: pip install matplotlib seaborn")


def load_classification_data(db_path: str) -> pd.DataFrame:
    """Load query classification results from DuckDB."""
    conn = duckdb.connect(db_path, read_only=True)
    df = conn.execute("""
        SELECT
            query_signature,
            query_pattern,
            caching_tier,
            classification_reason,
            execution_count,
            last_seen,
            days_since_last_seen,
            avg_execution_seconds,
            median_execution_seconds,
            stddev_execution_seconds,
            runtime_coefficient_of_variation,
            avg_rows_produced,
            estimated_result_mb,
            max_estimated_result_mb,
            avg_mb_scanned,
            total_gb_scanned,
            avg_warehouse_cost_credits,
            total_warehouse_cost_credits,
            potential_cache_savings_credits,
            cache_priority_score,
            most_common_warehouse,
            most_common_warehouse_size,
            unique_users
        FROM query_classification
    """).fetchdf()
    conn.close()
    return df


def generate_summary_report(df: pd.DataFrame, output_dir: Path):
    """Generate text summary report."""
    report_path = output_dir / "summary_report.txt"

    with open(report_path, 'w') as f:
        f.write("=" * 80 + "\n")
        f.write("SNOWFLAKE QUERY LOG ANALYSIS - SUMMARY REPORT\n")
        f.write("=" * 80 + "\n\n")

        # Overall statistics
        f.write("OVERALL STATISTICS\n")
        f.write("-" * 80 + "\n")
        f.write(f"Total unique query patterns: {len(df):,}\n")
        f.write(f"Total query executions: {df['execution_count'].sum():,}\n")
        f.write(f"Total Snowflake credits consumed: {df['total_warehouse_cost_credits'].sum():,.2f}\n")
        f.write(f"Potential savings from caching: {df['potential_cache_savings_credits'].sum():,.2f} credits\n")
        f.write(f"Average query runtime: {df['avg_execution_seconds'].mean():.2f} seconds\n")
        f.write(f"Total data scanned: {df['total_gb_scanned'].sum():,.2f} GB\n\n")

        # By tier
        f.write("BREAKDOWN BY CACHING TIER\n")
        f.write("-" * 80 + "\n")
        tier_summary = df.groupby('caching_tier').agg({
            'query_signature': 'count',
            'execution_count': 'sum',
            'total_warehouse_cost_credits': 'sum',
            'potential_cache_savings_credits': 'sum',
            'avg_execution_seconds': 'mean',
            'estimated_result_mb': 'mean',
            'total_gb_scanned': 'sum'
        }).round(2)

        for tier in ['CACHE', 'ARROW_EXTENSION', 'BATCH_TRANSPORT']:
            if tier in tier_summary.index:
                row = tier_summary.loc[tier]
                f.write(f"\n{tier}:\n")
                f.write(f"  Query patterns: {int(row['query_signature']):,}\n")
                f.write(f"  Total executions: {int(row['execution_count']):,}\n")
                f.write(f"  Total cost: {row['total_warehouse_cost_credits']:,.2f} credits\n")
                f.write(f"  Potential savings: {row['potential_cache_savings_credits']:,.2f} credits\n")
                f.write(f"  Avg runtime: {row['avg_execution_seconds']:.2f} seconds\n")
                f.write(f"  Avg result size: {row['estimated_result_mb']:.2f} MB\n")
                f.write(f"  Total scanned: {row['total_gb_scanned']:.2f} GB\n")

        # Top cache candidates
        f.write("\n" + "=" * 80 + "\n")
        f.write("TOP 10 CACHE CANDIDATES (by priority score)\n")
        f.write("=" * 80 + "\n")
        cache_df = df[df['caching_tier'] == 'CACHE'].nlargest(10, 'cache_priority_score')

        for idx, row in cache_df.iterrows():
            f.write(f"\n{row.name + 1}. Priority Score: {row['cache_priority_score']:.2f}\n")
            f.write(f"   Executions: {row['execution_count']}\n")
            f.write(f"   Avg Runtime: {row['avg_execution_seconds']:.2f}s\n")
            f.write(f"   Result Size: {row['estimated_result_mb']:.2f} MB\n")
            f.write(f"   Total Cost: {row['total_warehouse_cost_credits']:.2f} credits\n")
            f.write(f"   Potential Savings: {row['potential_cache_savings_credits']:.2f} credits\n")
            f.write(f"   Query: {row['query_pattern'][:120]}...\n")

        # Top cost drivers
        f.write("\n" + "=" * 80 + "\n")
        f.write("TOP 10 QUERIES BY TOTAL COST\n")
        f.write("=" * 80 + "\n")
        cost_df = df.nlargest(10, 'total_warehouse_cost_credits')

        for idx, row in cost_df.iterrows():
            f.write(f"\n{row.name + 1}. Total Cost: {row['total_warehouse_cost_credits']:.2f} credits\n")
            f.write(f"   Tier: {row['caching_tier']}\n")
            f.write(f"   Executions: {row['execution_count']}\n")
            f.write(f"   Avg Runtime: {row['avg_execution_seconds']:.2f}s\n")
            f.write(f"   Query: {row['query_pattern'][:120]}...\n")

    print(f"✓ Summary report saved to: {report_path}")


def create_visualizations(df: pd.DataFrame, output_dir: Path):
    """Create visualization charts."""
    if not HAS_VIZ:
        print("Skipping visualizations (matplotlib/seaborn not installed)")
        return

    # Set style
    sns.set_style("whitegrid")
    plt.rcParams['figure.figsize'] = (12, 8)

    # 1. Tier distribution pie chart
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    tier_counts = df['caching_tier'].value_counts()
    axes[0, 0].pie(tier_counts.values, labels=tier_counts.index, autopct='%1.1f%%', startangle=90)
    axes[0, 0].set_title('Query Distribution by Caching Tier')

    # 2. Cost by tier
    tier_costs = df.groupby('caching_tier')['total_warehouse_cost_credits'].sum().sort_values(ascending=False)
    tier_costs.plot(kind='bar', ax=axes[0, 1], color=['#2ecc71', '#3498db', '#e74c3c'])
    axes[0, 1].set_title('Total Cost by Caching Tier (Credits)')
    axes[0, 1].set_ylabel('Credits')
    axes[0, 1].tick_params(axis='x', rotation=45)

    # 3. Execution frequency distribution
    df['execution_bucket'] = pd.cut(df['execution_count'],
                                     bins=[0, 1, 5, 10, 50, 100, float('inf')],
                                     labels=['1', '2-5', '6-10', '11-50', '51-100', '100+'])
    freq_dist = df['execution_bucket'].value_counts().sort_index()
    freq_dist.plot(kind='bar', ax=axes[1, 0], color='#9b59b6')
    axes[1, 0].set_title('Query Frequency Distribution')
    axes[1, 0].set_xlabel('Execution Count')
    axes[1, 0].set_ylabel('Number of Query Patterns')
    axes[1, 0].tick_params(axis='x', rotation=45)

    # 4. Runtime vs Result Size scatter (cache candidates)
    cache_df = df[df['caching_tier'] == 'CACHE'].nlargest(50, 'cache_priority_score')
    scatter = axes[1, 1].scatter(cache_df['avg_execution_seconds'],
                                 cache_df['estimated_result_mb'],
                                 s=cache_df['execution_count'] * 10,
                                 c=cache_df['cache_priority_score'],
                                 cmap='viridis',
                                 alpha=0.6)
    axes[1, 1].set_title('Top 50 Cache Candidates: Runtime vs Result Size')
    axes[1, 1].set_xlabel('Avg Execution Time (seconds)')
    axes[1, 1].set_ylabel('Estimated Result Size (MB)')
    plt.colorbar(scatter, ax=axes[1, 1], label='Cache Priority Score')

    plt.tight_layout()
    chart_path = output_dir / "overview_charts.png"
    plt.savefig(chart_path, dpi=300, bbox_inches='tight')
    print(f"✓ Overview charts saved to: {chart_path}")
    plt.close()

    # 5. Cost savings opportunity
    fig, ax = plt.subplots(figsize=(10, 6))
    cache_savings = df[df['caching_tier'] == 'CACHE'].nlargest(20, 'potential_cache_savings_credits')
    ax.barh(range(len(cache_savings)), cache_savings['potential_cache_savings_credits'])
    ax.set_yticks(range(len(cache_savings)))
    ax.set_yticklabels([f"{p[:50]}..." for p in cache_savings['query_pattern']], fontsize=8)
    ax.set_xlabel('Potential Savings (Credits)')
    ax.set_title('Top 20 Cache Opportunities by Potential Savings')
    ax.invert_yaxis()
    plt.tight_layout()
    savings_path = output_dir / "savings_opportunities.png"
    plt.savefig(savings_path, dpi=300, bbox_inches='tight')
    print(f"✓ Savings chart saved to: {savings_path}")
    plt.close()


def export_detailed_csvs(df: pd.DataFrame, output_dir: Path):
    """Export detailed CSVs for each tier."""
    for tier in ['CACHE', 'ARROW_EXTENSION', 'BATCH_TRANSPORT']:
        tier_df = df[df['caching_tier'] == tier].copy()

        if tier == 'CACHE':
            tier_df = tier_df.sort_values('cache_priority_score', ascending=False)
        else:
            tier_df = tier_df.sort_values('total_warehouse_cost_credits', ascending=False)

        csv_path = output_dir / f"{tier.lower()}_queries.csv"
        tier_df.to_csv(csv_path, index=False)
        print(f"✓ {tier} queries exported to: {csv_path}")


def main():
    parser = argparse.ArgumentParser(description="Visualize Snowflake query analysis results")
    parser.add_argument("--db", default="query_analysis.db", help="DuckDB database file")
    parser.add_argument("--output-dir", default="reports", help="Output directory for reports")
    args = parser.parse_args()

    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(exist_ok=True)

    print(f"Loading data from {args.db}...")
    df = load_classification_data(args.db)
    print(f"Loaded {len(df)} query patterns")

    print("\nGenerating reports...")
    generate_summary_report(df, output_dir)
    create_visualizations(df, output_dir)
    export_detailed_csvs(df, output_dir)

    print(f"\n✓ All reports generated in: {output_dir.absolute()}")
    print("\nNext steps:")
    print("1. Review the summary report")
    print("2. Examine the cache_queries.csv for implementation priorities")
    print("3. Consider adjusting classification thresholds if needed")
    print("4. Implement caching for top priority queries")


if __name__ == "__main__":
    main()
