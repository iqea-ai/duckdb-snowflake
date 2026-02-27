#!/usr/bin/env python3
"""
Generate TPC-H Data for Disk Spill Benchmark

This script generates TPC-H data for local memory testing before production deployment.

Usage:
    python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16
"""

import argparse
import sys
import time
from pathlib import Path

try:
    import duckdb
except ImportError:
    print("Error: duckdb is required. Install it with: pip install duckdb", file=sys.stderr)
    sys.exit(1)


def generate_tpch_data(output_path: str, scale_factor: float, overwrite: bool = False):
    """Generate TPC-H data in a DuckDB database"""
    output_file = Path(output_path)
    
    # Check if file exists
    if output_file.exists() and not overwrite:
        print(f"Error: Database file already exists: {output_path}")
        print("Use --overwrite to replace it, or choose a different output path.")
        sys.exit(1)
    
    print(f"Generating TPC-H data (SF={scale_factor})...")
    print(f"Output database: {output_path}")
    print()
    
    # Connect to database (creates if doesn't exist)
    conn = duckdb.connect(output_path)
    
    try:
        # Install and load TPC-H extension
        print("Installing TPC-H extension...")
        conn.execute("INSTALL tpch;")
        conn.execute("LOAD tpch;")
        print("✓ TPC-H extension loaded")
        
        # Generate data
        print(f"Generating TPC-H data with scale factor {scale_factor}...")
        print("This may take a while for large scale factors...")
        start_time = time.time()
        
        # Use dbgen function to generate data
        # For large scale factors, use parallel generation
        if scale_factor >= 10:
            # Use parallel generation for SF >= 10
            children = 4  # Number of parallel workers
            print(f"Using parallel generation with {children} workers...")
            
            for step in range(1, children + 1):
                print(f"  Step {step}/{children}...", end=' ', flush=True)
                step_start = time.time()
                conn.execute(f"CALL dbgen(sf={scale_factor}, children={children}, step={step});")
                step_time = time.time() - step_start
                print(f"✓ ({step_time:.1f}s)")
        else:
            # Single-threaded for smaller scale factors
            conn.execute(f"CALL dbgen(sf={scale_factor});")
        
        generation_time = time.time() - start_time
        print(f"✓ Data generation completed in {generation_time:.1f} seconds")
        
        # Verify table sizes
        print()
        print("Verifying table sizes...")
        result = conn.execute("""
            SELECT 
                table_name,
                printf('%.2f GB', estimated_size/1024.0/1024.0/1024.0) AS size
            FROM duckdb_tables()
            WHERE schema_name = 'main'
            ORDER BY estimated_size DESC
        """).fetchall()
        
        print(f"{'Table':<15} {'Size':<15}")
        print("-" * 30)
        total_size = 0
        for row in result:
            table_name, size_str = row
            print(f"{table_name:<15} {size_str:<15}")
            # Extract size in GB for total
            try:
                size_gb = float(size_str.replace(' GB', ''))
                total_size += size_gb
            except:
                pass
        
        print("-" * 30)
        print(f"{'Total':<15} {total_size:.2f} GB")
        
        # Show row counts
        print()
        print("Row counts:")
        tables = ['customer', 'lineitem', 'nation', 'orders', 'part', 'partsupp', 'region', 'supplier']
        for table in tables:
            try:
                count = conn.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                print(f"  {table}: {count:,} rows")
            except Exception as e:
                print(f"  {table}: Error - {e}")
        
        print()
        print(f"✓ TPC-H database created successfully: {output_path}")
        print()
        print("You can now run the benchmark:")
        print(f"  python test_resource_usage.py --db-path {output_path}")
        
    except Exception as e:
        print(f"Error generating TPC-H data: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser(
        description='Generate TPC-H data for disk spill benchmark',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate SF16 data (~16GB, recommended for local testing)
  python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16

  # Generate SF1 data (~1GB) for quick testing
  python generate_tpch_data.py --output tpch_sf1.duckdb --scale-factor 1

  # Overwrite existing database
  python generate_tpch_data.py --output tpch_sf16.duckdb --scale-factor 16 --overwrite

Scale Factor Guidelines:
  - SF1:    ~1GB   (quick testing)
  - SF10:   ~10GB  (medium testing)
  - SF16:   ~16GB  (recommended for local memory testing)
  - SF100:  ~100GB (comprehensive testing, takes longer)
  - SF1000: ~1TB   (very large, takes hours)
        """
    )
    
    parser.add_argument(
        '--output',
        type=str,
        default='tpch_sf16.duckdb',
        help='Output database file path (default: tpch_sf16.duckdb)'
    )
    
    parser.add_argument(
        '--scale-factor',
        type=float,
        default=16.0,
        help='TPC-H scale factor (default: 16, recommended for local testing)'
    )
    
    parser.add_argument(
        '--overwrite',
        action='store_true',
        help='Overwrite existing database file'
    )
    
    parser.add_argument(
        '--yes',
        '-y',
        action='store_true',
        help='Skip confirmation prompt for large scale factors'
    )
    
    args = parser.parse_args()
    
    # Validate scale factor
    if args.scale_factor <= 0:
        print("Error: Scale factor must be greater than 0", file=sys.stderr)
        sys.exit(1)
    
    # Warn for very large scale factors
    if args.scale_factor >= 50 and not args.yes:
        print(f"Warning: Scale factor {args.scale_factor} will generate a very large database.")
        print(f"Expected size: ~{args.scale_factor}GB")
        print("This may take a long time and require significant disk space.")
        try:
            response = input("Continue? (y/N): ")
            if response.lower() != 'y':
                print("Cancelled.")
                sys.exit(0)
        except EOFError:
            # Non-interactive mode, require --yes flag
            print("Error: Cannot prompt for confirmation in non-interactive mode.", file=sys.stderr)
            print("Use --yes flag to skip confirmation.", file=sys.stderr)
            sys.exit(1)
    
    generate_tpch_data(
        output_path=args.output,
        scale_factor=args.scale_factor,
        overwrite=args.overwrite
    )


if __name__ == '__main__':
    main()









