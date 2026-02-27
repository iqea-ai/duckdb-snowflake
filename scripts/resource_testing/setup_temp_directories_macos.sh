#!/bin/bash
# Setup Temp Directories for Disk Limit Testing (macOS)
#
# Creates disk images with size limits for testing DuckDB spill behavior
# with different disk storage constraints on macOS.
#
# Usage:
#   ./setup_temp_directories_macos.sh
#
# This creates in ~/duckdb_temp_limits/:
#   temp_unlimited  - Regular directory (unlimited)
#   temp_16gb       - Disk image with 16GB limit
#   temp_8gb        - Disk image with 8GB limit
#   temp_4gb        - Disk image with 4GB limit

set -e

BASE_DIR="$HOME/duckdb_temp_limits"
MOUNT_BASE="$HOME/duckdb_temp_mounts"

echo "Setting up temp directories for disk limit testing (macOS)..."
echo ""

# Create base directory
mkdir -p "$BASE_DIR"
mkdir -p "$MOUNT_BASE"

# Create unlimited directory (regular directory)
mkdir -p "$BASE_DIR/temp_unlimited"
echo "  ✓ Created $BASE_DIR/temp_unlimited (unlimited)"

# Function to create and mount disk image
create_disk_image() {
    local size=$1
    local name=$2
    local image_path="$BASE_DIR/${name}.dmg"
    local mount_point="$MOUNT_BASE/${name}"
    
    # Check if already exists
    if [ -f "$image_path" ]; then
        echo "  ⚠ Disk image $image_path already exists, skipping..."
        return
    fi
    
    # Create disk image
    echo "  Creating ${size} disk image: $name..."
    hdiutil create -size ${size} -fs HFS+ -volname "${name}" "$image_path" > /dev/null 2>&1
    
    # Mount it
    hdiutil attach "$image_path" -mountpoint "$mount_point" > /dev/null 2>&1
    
    echo "  ✓ Created and mounted ${size} disk image at $mount_point"
}

# Create limited disk images
create_disk_image 16g temp_16gb
create_disk_image 8g temp_8gb
create_disk_image 4g temp_4gb

echo ""
echo "Verifying mounts..."
df -h | grep -E "temp_|Filesystem" | head -5

echo ""
echo "Setup complete! You can now use these directories with:"
echo "  --temp-storage-limits $BASE_DIR/temp_unlimited,$MOUNT_BASE/temp_16gb,$MOUNT_BASE/temp_8gb,$MOUNT_BASE/temp_4gb"
echo ""
echo "To unmount later:"
echo "  hdiutil detach $MOUNT_BASE/temp_16gb"
echo "  hdiutil detach $MOUNT_BASE/temp_8gb"
echo "  hdiutil detach $MOUNT_BASE/temp_4gb"



