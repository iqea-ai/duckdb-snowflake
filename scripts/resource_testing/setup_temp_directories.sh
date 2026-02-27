#!/bin/bash
# Setup Temp Directories for Disk Limit Testing
#
# Creates tmpfs mounts with size limits for testing DuckDB spill behavior
# with different disk storage constraints.
#
# Usage:
#   sudo ./setup_temp_directories.sh
#
# This creates:
#   /mnt/temp_unlimited  - Regular directory (unlimited)
#   /mnt/temp_16gb       - tmpfs with 16GB limit
#   /mnt/temp_8gb        - tmpfs with 8GB limit
#   /mnt/temp_4gb        - tmpfs with 4GB limit

set -e

BASE_DIR="/mnt"
DIRS=("temp_unlimited" "temp_16gb" "temp_8gb" "temp_4gb")

echo "Setting up temp directories for disk limit testing..."
echo ""

# Create all directories
echo "Creating directories..."
for dir in "${DIRS[@]}"; do
    mkdir -p "${BASE_DIR}/${dir}"
    echo "  ✓ Created ${BASE_DIR}/${dir}"
done

echo ""

# Mount tmpfs with size limits (skip unlimited - it's a regular directory)
echo "Mounting tmpfs filesystems..."
mount -t tmpfs -o size=16G tmpfs "${BASE_DIR}/temp_16gb" && echo "  ✓ Mounted 16GB tmpfs at ${BASE_DIR}/temp_16gb"
mount -t tmpfs -o size=8G tmpfs "${BASE_DIR}/temp_8gb" && echo "  ✓ Mounted 8GB tmpfs at ${BASE_DIR}/temp_8gb"
mount -t tmpfs -o size=4G tmpfs "${BASE_DIR}/temp_4gb" && echo "  ✓ Mounted 4GB tmpfs at ${BASE_DIR}/temp_4gb"

echo ""
echo "Verifying mounts..."
df -h | grep -E "temp_|Filesystem" | head -5

echo ""
echo "Setup complete! You can now use these directories with:"
echo "  --temp-storage-limits unlimited,/mnt/temp_16gb,/mnt/temp_8gb,/mnt/temp_4gb"



