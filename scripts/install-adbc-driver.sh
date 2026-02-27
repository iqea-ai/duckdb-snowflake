#!/bin/bash

# DuckDB Snowflake ADBC Driver Installer
# Usage: curl -sSL https://raw.githubusercontent.com/iqea-ai/duckdb-snowflake/main/scripts/install-adbc-driver.sh | sh
#    or: wget -qO- https://raw.githubusercontent.com/iqea-ai/duckdb-snowflake/main/scripts/install-adbc-driver.sh | sh

set -e

# Configuration
ADBC_VERSION="apache-arrow-adbc-20"
DRIVER_VERSION="1.8.0"
BASE_URL="https://github.com/apache/arrow-adbc/releases/download/${ADBC_VERSION}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Detect platform
detect_platform() {
    OS="$(uname -s)"
    ARCH="$(uname -m)"

    case "$OS" in
        Linux)
            if [ "$ARCH" = "x86_64" ]; then
                WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-manylinux1_x86_64.manylinux2014_x86_64.manylinux_2_17_x86_64.manylinux_2_5_x86_64.whl"
                PLATFORM="linux_amd64"
            elif [ "$ARCH" = "aarch64" ]; then
                WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-manylinux2014_aarch64.manylinux_2_17_aarch64.whl"
                PLATFORM="linux_arm64"
            else
                print_error "Unsupported Linux architecture: $ARCH"
                exit 1
            fi
            ;;
        Darwin)
            if [ "$ARCH" = "x86_64" ]; then
                WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-macosx_10_15_x86_64.whl"
                PLATFORM="osx_amd64"
            elif [ "$ARCH" = "arm64" ]; then
                WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-macosx_11_0_arm64.whl"
                PLATFORM="osx_arm64"
            else
                print_error "Unsupported macOS architecture: $ARCH"
                exit 1
            fi
            ;;
        MINGW*|CYGWIN*|MSYS*|Windows*)
            WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-win_amd64.whl"
            PLATFORM="windows_amd64"
            ;;
        *)
            print_error "Unsupported operating system: $OS"
            exit 1
            ;;
    esac

    print_info "Detected platform: $OS $ARCH ($PLATFORM)"
}

# Detect DuckDB version
get_duckdb_version() {
    # Check if DuckDB is installed
    if ! command -v duckdb >/dev/null 2>&1; then
        print_error "DuckDB is not installed!"
        echo ""
        echo "Please install DuckDB first:"
        echo "  • macOS:  brew install duckdb"
        echo "  • Ubuntu: sudo apt-get install duckdb"
        echo "  • Other:  https://duckdb.org/docs/installation/"
        echo ""
        exit 1
    fi

    # Get DuckDB version
    DUCKDB_VERSION=$(duckdb -c "SELECT version()" 2>/dev/null | grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    if [ -z "$DUCKDB_VERSION" ]; then
        print_error "Could not detect DuckDB version"
        echo "Please ensure DuckDB is properly installed and accessible"
        exit 1
    fi

    print_info "Detected DuckDB version: $DUCKDB_VERSION"
}

# Get platform string for DuckDB directory structure
get_duckdb_platform() {
    case "$PLATFORM" in
        linux_amd64)
            DUCKDB_PLATFORM="linux_amd64"
            ;;
        linux_arm64)
            DUCKDB_PLATFORM="linux_arm64"
            ;;
        osx_amd64)
            DUCKDB_PLATFORM="osx_amd64"
            ;;
        osx_arm64)
            DUCKDB_PLATFORM="osx_arm64"
            ;;
        windows_amd64)
            DUCKDB_PLATFORM="windows_amd64"
            ;;
        *)
            DUCKDB_PLATFORM="$PLATFORM"
            ;;
    esac
}

# Check for required tools
check_dependencies() {
    # Check for curl or wget
    if command -v curl >/dev/null 2>&1; then
        DOWNLOAD_CMD="curl -L -o"
    elif command -v wget >/dev/null 2>&1; then
        DOWNLOAD_CMD="wget -O"
    else
        print_error "Neither curl nor wget found. Please install one of them."
        exit 1
    fi

    # Check for unzip
    if ! command -v unzip >/dev/null 2>&1; then
        print_error "unzip is required but not installed."
        print_info "Install it with:"
        case "$OS" in
            Linux)
                echo "  sudo apt-get install unzip  # Debian/Ubuntu"
                echo "  sudo yum install unzip      # RHEL/CentOS"
                ;;
            Darwin)
                echo "  brew install unzip"
                ;;
        esac
        exit 1
    fi
}

# Download the wheel file
download_driver() {
    WHEEL_URL="${BASE_URL}/${WHEEL_NAME}"
    WHEEL_PATH="${INSTALL_DIR}/${WHEEL_NAME}"

    if [ -f "${WHEEL_PATH}" ]; then
        print_warning "Driver wheel already exists, checking if extraction is needed..."
    else
        print_info "Downloading ADBC Snowflake driver..."
        print_info "URL: ${WHEEL_URL}"

        $DOWNLOAD_CMD "${WHEEL_PATH}" "${WHEEL_URL}"

        if [ $? -ne 0 ]; then
            print_error "Failed to download ADBC driver"
            print_info "Please check your internet connection and try again"
            exit 1
        fi

        print_success "Downloaded ${WHEEL_NAME}"
    fi
}

# Global variable for driver filename
DRIVER_FILE=""

# Extract the driver from wheel
extract_driver() {
    print_info "Extracting driver library..."

    cd "${INSTALL_DIR}"

    # Extract the shared library from the wheel
    unzip -o "${WHEEL_NAME}" "adbc_driver_snowflake/libadbc_driver_snowflake.so" 2>/dev/null || {
        # Try alternative path for Windows
        unzip -o "${WHEEL_NAME}" "adbc_driver_snowflake/adbc_driver_snowflake.dll" 2>/dev/null || {
            print_error "Failed to extract driver library from wheel"
            exit 1
        }
    }

    # Move the library to the install directory
    if [ -f "adbc_driver_snowflake/libadbc_driver_snowflake.so" ]; then
        mv -f "adbc_driver_snowflake/libadbc_driver_snowflake.so" .
        DRIVER_FILE="libadbc_driver_snowflake.so"
    elif [ -f "adbc_driver_snowflake/adbc_driver_snowflake.dll" ]; then
        mv -f "adbc_driver_snowflake/adbc_driver_snowflake.dll" .
        DRIVER_FILE="adbc_driver_snowflake.dll"
    else
        print_error "Driver library not found in wheel"
        exit 1
    fi

    # Clean up
    rmdir "adbc_driver_snowflake" 2>/dev/null || true

    # Make the library executable (important for some platforms)
    chmod +x "${DRIVER_FILE}" 2>/dev/null || true

    print_success "Extracted ${DRIVER_FILE}"
}


# Verify installation
verify_installation() {
    print_info "Verifying installation..."

    FINAL_PATH="${INSTALL_DIR}/${DRIVER_FILE}"
    if [ -f "${FINAL_PATH}" ]; then
        FILE_SIZE=$(ls -lh "${FINAL_PATH}" | awk '{print $5}')
        print_success "ADBC Snowflake driver installed successfully!"
        echo ""
        echo "Installation details:"
        echo "  • Driver: ${DRIVER_FILE}"
        echo "  • Location: ${FINAL_PATH}"
        echo "  • Size: ${FILE_SIZE}"
        echo "  • Version: ${DRIVER_VERSION}"
        echo ""

        # Platform-specific instructions
        case "$PLATFORM" in
            linux*)
                echo "To use with DuckDB Snowflake extension:"
                echo "  1. The extension will automatically find the driver at:"
                echo "     ${FINAL_PATH}"
                echo "  2. If you have issues, set the environment variable:"
                echo "     export SNOWFLAKE_ADBC_DRIVER_PATH=\"${FINAL_PATH}\""
                ;;
            osx*)
                echo "To use with DuckDB Snowflake extension:"
                echo "  1. The extension will automatically find the driver at:"
                echo "     ${FINAL_PATH}"
                echo "  2. If you have issues, set the environment variable:"
                echo "     export SNOWFLAKE_ADBC_DRIVER_PATH=\"${FINAL_PATH}\""
                ;;
            windows*)
                echo "To use with DuckDB Snowflake extension:"
                echo "  1. The extension will automatically find the driver at:"
                echo "     ${FINAL_PATH}"
                echo "  2. If you have issues, set the environment variable:"
                echo "     set SNOWFLAKE_ADBC_DRIVER_PATH=${FINAL_PATH}"
                ;;
        esac

        echo ""
        print_success "Installation complete! You can now use the Snowflake extension."
    else
        print_error "Installation verification failed"
        exit 1
    fi
}

# Main installation flow
main() {
    detect_platform
    check_dependencies
    get_duckdb_version

    echo ""
    echo "════════════════════════════════════════════════════════════════"
    echo "     DuckDB Snowflake ADBC Driver Installer"
    echo "     DuckDB Version: ${DUCKDB_VERSION}"
    echo "════════════════════════════════════════════════════════════════"
    echo ""

    get_duckdb_platform

    # Determine installation directory
    if [ -n "$DUCKDB_EXTENSION_DIR" ]; then
        INSTALL_DIR="$DUCKDB_EXTENSION_DIR"
    else
        # Get home directory
        if [ -n "$HOME" ]; then
            HOME_DIR="$HOME"
        else
            HOME_DIR="$(cd ~ && pwd)"
        fi

        # Use the same directory structure as DuckDB extensions
        # ~/.duckdb/extensions/<version>/<platform>/
        INSTALL_DIR="${HOME_DIR}/.duckdb/extensions/${DUCKDB_VERSION}/${DUCKDB_PLATFORM}"
    fi

    # Create directory if it doesn't exist
    mkdir -p "$INSTALL_DIR"
    print_info "Installation directory: $INSTALL_DIR"

    download_driver
    extract_driver
    verify_installation
}

# Run main function
main "$@"