#!/usr/bin/env python3
"""
Generate TPC-H SF100 data and run queries 1-22

This script:
1. Generates TPC-H data with scale factor 100 locally
2. Runs TPC-H queries 1-22 on the generated data

All files (database, temp files) are stored in a single directory for easy cleanup.

Usage:
    python run_tpch_sf100.py --data-dir tpch_sf100_data
    python run_tpch_sf100.py --data-dir tpch_sf100_data --overwrite
"""

import argparse
import sys
import time
import os
from pathlib import Path

# Fix import path to avoid shadowing duckdb package by local duckdb/ directory
# Insert site-packages at the beginning to prioritize installed packages
import site
site_packages = site.getsitepackages()
if site_packages:
    # Insert site-packages at the beginning (after index 0 which is the script dir)
    for sp in reversed(site_packages):
        if sp not in sys.path:
            sys.path.insert(1, sp)

try:
    import duckdb
except ImportError:
    print("Error: duckdb is required. Install it with: pip install duckdb", file=sys.stderr)
    sys.exit(1)


def setup_temp_directory(conn, temp_dir: Path):
    """Set DuckDB temp directory explicitly"""
    temp_dir_str = str(temp_dir.resolve())
    # Escape single quotes in path for SQL
    temp_dir_str_escaped = temp_dir_str.replace("'", "''")
    try:
        conn.execute(f"SET temp_directory = '{temp_dir_str_escaped}'")
        print(f"✓ Temp directory set to: {temp_dir_str}")
    except Exception as e:
        print(f"Warning: Could not set temp directory: {e}")
        print(f"Temp files may be stored in default location")


def generate_tpch_data(conn, scale_factor: float):
    """Generate TPC-H data in the DuckDB connection"""
    print(f"Generating TPC-H data with scale factor {scale_factor}...")
    print("This may take a while for large scale factors...")
    start_time = time.time()
    
    # Install and load TPC-H extension if not already loaded
    try:
        conn.execute("INSTALL tpch;")
    except:
        pass  # Extension may already be installed
    conn.execute("LOAD tpch;")
    
    # Use dbgen function to generate data
    # For large scale factors, use parallel generation
    if scale_factor >= 10:
        # Use parallel generation for SF >= 10
        children = 4  # Number of parallel workers
        step_names = [
            "Partition 1/4",
            "Partition 2/4",
            "Partition 3/4",
            "Partition 4/4"
        ]
        print(f"Using parallel generation with {children} workers...")
        print()
        
        for step in range(1, children + 1):
            step_name = step_names[step - 1] if step <= len(step_names) else f"Generating partition {step}/{children}"
            print(f"  {step_name}...", end=' ', flush=True)
            step_start = time.time()
            conn.execute(f"CALL dbgen(sf={scale_factor}, children={children}, step={step});")
            step_time = time.time() - step_start
            print(f"✓ ({step_time:.1f}s)")
    else:
        # Single-threaded for smaller scale factors
        print("Generating all tables (single-threaded)...", end=' ', flush=True)
        gen_start = time.time()
        conn.execute(f"CALL dbgen(sf={scale_factor});")
        gen_time = time.time() - gen_start
        print(f"✓ ({gen_time:.1f}s)")
        print()
    
    generation_time = time.time() - start_time
    print(f"✓ Data generation completed in {generation_time:.1f} seconds ({generation_time/60:.1f} minutes)")
    print()


def run_queries(conn, query_numbers=None):
    """Run TPC-H queries and return results"""
    # Load TPC-H extension if not already loaded
    try:
        conn.execute("LOAD tpch;")
    except:
        pass
    
    # Get queries from tpch_queries() function
    print("Loading TPC-H queries...")
    query_results = conn.execute("SELECT query_nr, query FROM tpch_queries() ORDER BY query_nr").fetchall()
    
    queries = {nr: query for nr, query in query_results}
    
    # Filter to requested query numbers (default: 1-22)
    if query_numbers is None:
        query_numbers = list(range(1, 23))
    
    query_numbers = [q for q in query_numbers if q in queries]
    
    if not query_numbers:
        print("Error: No valid queries found", file=sys.stderr)
        return
    
    print(f"Running {len(query_numbers)} queries: {query_numbers}")
    print("=" * 80)
    print()
    
    results = []
    total_start = time.time()
    
    for query_nr in sorted(query_numbers):
        query = queries[query_nr]
        print(f"Query {query_nr}:")
        print("-" * 80)
        
        query_start = time.time()
        try:
            result = conn.execute(query).fetchall()
            query_time = time.time() - query_start
            
            row_count = len(result)
            print(f"  ✓ Completed in {query_time:.2f} seconds")
            print(f"  Rows returned: {row_count:,}")
            
            # Show first few rows if any
            if row_count > 0:
                print(f"  First row: {result[0]}")
            
            results.append({
                'query_nr': query_nr,
                'time': query_time,
                'rows': row_count,
                'success': True
            })
        except Exception as e:
            query_time = time.time() - query_start
            print(f"  ✗ Failed after {query_time:.2f} seconds")
            print(f"  Error: {e}")
            results.append({
                'query_nr': query_nr,
                'time': query_time,
                'rows': 0,
                'success': False,
                'error': str(e)
            })
        
        print()
    
    total_time = time.time() - total_start
    
    # Print summary
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"Total time: {total_time:.2f} seconds ({total_time/60:.2f} minutes)")
    print(f"Queries run: {len(results)}")
    print(f"Successful: {sum(1 for r in results if r['success'])}")
    print(f"Failed: {sum(1 for r in results if not r['success'])}")
    print()
    print(f"{'Query':<8} {'Time (s)':<12} {'Rows':<15} {'Status':<10}")
    print("-" * 50)
    for r in results:
        status = "✓ Success" if r['success'] else "✗ Failed"
        print(f"{r['query_nr']:<8} {r['time']:<12.2f} {r['rows']:<15,} {status:<10}")
    
    return results


def main():
    parser = argparse.ArgumentParser(
        description='Generate TPC-H SF100 data and run queries 1-22',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate SF100 and run all queries 1-22 (all files in tpch_sf100_data/)
  python run_tpch_sf100.py --data-dir tpch_sf100_data

  # Overwrite existing database
  python run_tpch_sf100.py --data-dir tpch_sf100_data --overwrite

  # Run only specific queries
  python run_tpch_sf100.py --data-dir tpch_sf100_data --queries 1,5,10

  # Skip data generation if database already exists
  python run_tpch_sf100.py --data-dir tpch_sf100_data --skip-generation

  # Use custom database path (temp directory will still be in data_dir/temp)
  python run_tpch_sf100.py --data-dir tpch_sf100_data --db-path custom.duckdb

  # Remove all files
  rm -rf tpch_sf100_data

Note: SF100 generates approximately 100GB of data and may take several hours to generate.
All files (database, temp files) are stored in the data directory for easy cleanup.
        """
    )
    
    parser.add_argument(
        '--data-dir',
        type=str,
        default='tpch_sf100_data',
        help='Directory to store all files (database, temp files). Default: tpch_sf100_data'
    )
    
    parser.add_argument(
        '--db-path',
        type=str,
        default=None,
        help='Database file path (overrides --data-dir/db.duckdb). If not set, uses {data-dir}/tpch_sf100.duckdb'
    )
    
    parser.add_argument(
        '--scale-factor',
        type=float,
        default=100.0,
        help='TPC-H scale factor (default: 100)'
    )
    
    parser.add_argument(
        '--overwrite',
        action='store_true',
        help='Overwrite existing database file'
    )
    
    parser.add_argument(
        '--skip-generation',
        action='store_true',
        help='Skip data generation if database already exists'
    )
    
    parser.add_argument(
        '--queries',
        type=str,
        default=None,
        help='Comma-separated list of query numbers to run (e.g., "1,5,10" or "1-22"). Default: all queries 1-22'
    )
    
    parser.add_argument(
        '--yes',
        '-y',
        action='store_true',
        help='Skip confirmation prompt for large scale factors'
    )
    
    args = parser.parse_args()
    
    # Parse query numbers
    query_numbers = None
    if args.queries:
        query_numbers = []
        for part in args.queries.split(','):
            part = part.strip()
            if '-' in part:
                start, end = part.split('-')
                query_numbers.extend(range(int(start), int(end) + 1))
            else:
                query_numbers.append(int(part))
        query_numbers = sorted(set(query_numbers))
    
    # Validate scale factor
    if args.scale_factor <= 0:
        print("Error: Scale factor must be greater than 0", file=sys.stderr)
        sys.exit(1)
    
    # Set up directory structure - everything goes in data_dir
    # If --db-path is provided and is outside --data-dir, use db_path's parent as base
    if args.db_path:
        db_path = Path(args.db_path).resolve()
        specified_data_dir = Path(args.data_dir).resolve()
        
        # Check if db_path is inside the specified data_dir
        try:
            db_path.relative_to(specified_data_dir)
            # db_path is inside data_dir, use data_dir as base
            data_dir = specified_data_dir
        except ValueError:
            # db_path is outside data_dir, use db_path's parent as base
            data_dir = db_path.parent
            print(f"Note: Database path is outside specified --data-dir")
            print(f"Using database location's parent directory as base: {data_dir}")
    else:
        # Use data_dir structure
        data_dir = Path(args.data_dir).resolve()
        db_path = data_dir / "tpch_sf100.duckdb"
    
    # Temp directory always goes in data_dir for easy cleanup
    temp_dir = data_dir / "temp"
    
    # Create directories
    data_dir.mkdir(parents=True, exist_ok=True)
    temp_dir.mkdir(parents=True, exist_ok=True)
    
    # Ensure db_path parent exists
    db_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"Data directory: {data_dir.resolve()}")
    print(f"Database file: {db_path.resolve()}")
    print(f"Temp directory: {temp_dir.resolve()}")
    print()
    
    # Check if database exists
    db_exists = db_path.exists()
    
    if db_exists and not args.skip_generation and not args.overwrite:
        print(f"Database already exists: {db_path}")
        print("Use --overwrite to replace it, or --skip-generation to use existing database.")
        print(f"\nTo remove all files, run: rm -rf {data_dir}")
        sys.exit(1)
    
    # Warn for large scale factors
    if args.scale_factor >= 50 and not args.yes and not args.skip_generation:
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
    
    # Connect to database (creates if doesn't exist)
    print(f"Connecting to database: {db_path}")
    conn = duckdb.connect(str(db_path))
    
    try:
        # Set temp directory explicitly
        setup_temp_directory(conn, temp_dir)
        print()
        
        # Generate data if needed
        if not args.skip_generation:
            if db_exists and args.overwrite:
                print(f"Overwriting existing database: {db_path}")
                # Close connection and delete the database file, then recreate
                conn.close()
                try:
                    db_path.unlink()
                    # Recreate connection with new (empty) database
                    conn = duckdb.connect(str(db_path))
                    setup_temp_directory(conn, temp_dir)
                    print()
                except Exception as e:
                    print(f"  Error: Could not delete and recreate database: {e}")
                    # Fallback: try to drop all tables
                    conn = duckdb.connect(str(db_path))
                    setup_temp_directory(conn, temp_dir)
                    try:
                        tables = conn.execute("""
                            SELECT table_name 
                            FROM information_schema.tables 
                            WHERE table_schema = 'main'
                        """).fetchall()
                        for (table_name,) in tables:
                            conn.execute(f"DROP TABLE IF EXISTS {table_name} CASCADE")
                    except Exception as e2:
                        print(f"  Error: Could not clean database: {e2}")
                        raise
            
            generate_tpch_data(conn, args.scale_factor)
            
            # Force checkpoint to ensure all data is written to disk
            try:
                conn.execute("CHECKPOINT")
            except:
                pass  # CHECKPOINT may not be available in all DuckDB versions
            
            # Verify table information and database file size
            print("Verifying table information...")
            
            # Get row counts for all tables
            tables = ['customer', 'lineitem', 'nation', 'orders', 'part', 'partsupp', 'region', 'supplier']
            print(f"{'Table':<15} {'Rows':<20}")
            print("-" * 40)
            for table in tables:
                try:
                    count = conn.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                    print(f"{table:<15} {count:>19,}")
                except Exception as e:
                    print(f"{table:<15} Error: {e}")
            
            print()
            
            # Get actual database file size on disk
            db_file_size = db_path.stat().st_size
            db_file_size_gb = db_file_size / (1024.0 * 1024.0 * 1024.0)
            print(f"Database file size: {db_file_size_gb:.2f} GB ({db_file_size:,} bytes)")
            print()
        else:
            print("Skipping data generation (using existing database)")
            print()
        
        # Run queries
        run_queries(conn, query_numbers)
        
        print()
        print("=" * 80)
        print(f"All files stored in: {data_dir.resolve()}")
        print(f"To remove all files: rm -rf {data_dir}")
        print("=" * 80)
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
    finally:
        conn.close()


if __name__ == '__main__':
    main()

