#!/usr/bin/env python3
"""
Automatically package DuckDB extension with platform-specific ADBC driver.

This script creates platform-specific zip archives containing:
- The DuckDB extension (.duckdb_extension)
- The ADBC Snowflake driver (.so file)

Usage:
    python3 scripts/package_extension_with_driver.py [--build-dir BUILD_DIR] [--output-dir OUTPUT_DIR]
"""

import argparse
import os
import platform
import shutil
import sys
import zipfile
from pathlib import Path


def detect_platform():
    """Detect the current platform and return the platform identifier."""
    system = platform.system().lower()
    arch = platform.machine().lower()
    
    if system == "linux":
        if arch in ["x86_64", "amd64"]:
            return "linux_x86_64"
        elif arch == "aarch64":
            return "linux_aarch64"
        else:
            raise ValueError(f"Unsupported Linux architecture: {arch}")
    elif system == "darwin":
        if arch == "arm64":
            return "darwin_arm64"
        elif arch == "x86_64":
            return "darwin_x86_64"
        else:
            raise ValueError(f"Unsupported macOS architecture: {arch}")
    elif system == "windows":
        if arch in ["amd64", "x86_64"]:
            return "windows_x86_64"
        else:
            raise ValueError(f"Unsupported Windows architecture: {arch}")
    else:
        raise ValueError(f"Unsupported operating system: {system}")


def find_extension_file(build_dir):
    """Find the built extension file."""
    possible_paths = [
        f"{build_dir}/extension/snowflake/snowflake.duckdb_extension",
        f"{build_dir}/snowflake.duckdb_extension",
        f"{build_dir}/release/extension/snowflake/snowflake.duckdb_extension",
        f"{build_dir}/debug/extension/snowflake/snowflake.duckdb_extension",
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            return path
    
    raise FileNotFoundError(f"Extension file not found in build directory: {build_dir}")


def find_adbc_driver():
    """Find the ADBC driver file."""
    driver_path = "adbc_drivers/libadbc_driver_snowflake.so"
    
    if not os.path.exists(driver_path):
        raise FileNotFoundError(
            f"ADBC driver not found at {driver_path}. "
            "Run 'make download-adbc' to download it."
        )
    
    return driver_path


def create_package(extension_path, driver_path, platform_name, output_dir):
    """Create a platform-specific zip package."""
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Package name
    package_name = f"snowflake-extension-{platform_name}.zip"
    package_path = os.path.join(output_dir, package_name)
    
    print(f"Creating package: {package_path}")
    
    # Create zip file
    with zipfile.ZipFile(package_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        # Add extension file
        extension_filename = "snowflake/snowflake.duckdb_extension"
        zipf.write(extension_path, extension_filename)
        print(f"  Added: {extension_filename}")
        
        # Add driver file
        driver_filename = "snowflake/libadbc_driver_snowflake.so"
        zipf.write(driver_path, driver_filename)
        print(f"  Added: {driver_filename}")
    
    # Get file sizes for verification
    extension_size = os.path.getsize(extension_path)
    driver_size = os.path.getsize(driver_path)
    package_size = os.path.getsize(package_path)
    
    print(f"\nPackage created successfully!")
    print(f"  Extension size: {extension_size:,} bytes")
    print(f"  Driver size: {driver_size:,} bytes")
    print(f"  Package size: {package_size:,} bytes")
    print(f"  Compression ratio: {((extension_size + driver_size) / package_size):.2f}x")
    
    return package_path


def validate_package(package_path):
    """Validate the package contents."""
    print(f"\nValidating package: {package_path}")
    
    with zipfile.ZipFile(package_path, 'r') as zipf:
        files = zipf.namelist()
        
        expected_files = [
            "snowflake/snowflake.duckdb_extension",
            "snowflake/libadbc_driver_snowflake.so"
        ]
        
        for expected_file in expected_files:
            if expected_file not in files:
                raise ValueError(f"Missing file in package: {expected_file}")
            print(f"  Found: {expected_file}")
    
    print("  Package validation passed!")


def main():
    parser = argparse.ArgumentParser(
        description="Package DuckDB extension with ADBC driver",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Package with default settings
  python3 scripts/package_extension_with_driver.py
  
  # Package from specific build directory
  python3 scripts/package_extension_with_driver.py --build-dir build/release
  
  # Package to specific output directory
  python3 scripts/package_extension_with_driver.py --output-dir dist/
        """
    )
    
    parser.add_argument(
        '--build-dir',
        default='build/release',
        help='Build directory containing the extension (default: build/release)'
    )
    
    parser.add_argument(
        '--output-dir',
        default='build/packages',
        help='Output directory for packages (default: build/packages)'
    )
    
    parser.add_argument(
        '--validate-only',
        action='store_true',
        help='Only validate existing packages, do not create new ones'
    )
    
    args = parser.parse_args()
    
    try:
        # Detect platform
        platform_name = detect_platform()
        print(f"Detected platform: {platform_name}")
        
        if args.validate_only:
            # Validate existing packages
            package_path = os.path.join(args.output_dir, f"snowflake-extension-{platform_name}.zip")
            if os.path.exists(package_path):
                validate_package(package_path)
            else:
                print(f"No existing package found: {package_path}")
                return 1
        else:
            # Find extension file
            extension_path = find_extension_file(args.build_dir)
            print(f"Found extension: {extension_path}")
            
            # Find ADBC driver
            driver_path = find_adbc_driver()
            print(f"Found ADBC driver: {driver_path}")
            
            # Create package
            package_path = create_package(extension_path, driver_path, platform_name, args.output_dir)
            
            # Validate package
            validate_package(package_path)
            
            print(f"\nSuccessfully created package: {package_path}")
            print(f"\nTo use this package:")
            print(f"1. Extract the zip file")
            print(f"2. Copy the 'snowflake' directory to your DuckDB extensions location")
            print(f"3. Load the extension: LOAD 'snowflake/snowflake.duckdb_extension'")
        
        return 0
        
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
