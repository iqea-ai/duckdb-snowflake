#!/bin/bash

# Download pre-built ADBC Snowflake driver from official releases
# Version 1.7.0 from ADBC release 19

ADBC_VERSION="apache-arrow-adbc-19"
DRIVER_VERSION="1.7.0"
BASE_URL="https://github.com/apache/arrow-adbc/releases/download/${ADBC_VERSION}"

# Detect platform
OS="$(uname -s)"
ARCH="$(uname -m)"

echo "Detected platform: $OS $ARCH"

# Create directory for drivers
DRIVER_DIR="$(dirname "$0")/../adbc_drivers"
mkdir -p "$DRIVER_DIR"

# Determine which wheel to download based on platform
case "$OS" in
    Linux)
        if [ "$ARCH" = "x86_64" ]; then
            WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-manylinux1_x86_64.manylinux2014_x86_64.manylinux_2_17_x86_64.manylinux_2_5_x86_64.whl"
        elif [ "$ARCH" = "aarch64" ]; then
            WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-manylinux2014_aarch64.manylinux_2_17_aarch64.whl"
        else
            echo "Unsupported Linux architecture: $ARCH"
            exit 1
        fi
        ;;
    Darwin)
        if [ "$ARCH" = "x86_64" ]; then
            WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-macosx_10_15_x86_64.whl"
        elif [ "$ARCH" = "arm64" ]; then
            WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-macosx_11_0_arm64.whl"
        else
            echo "Unsupported macOS architecture: $ARCH"
            exit 1
        fi
        ;;
    MINGW*|CYGWIN*|MSYS*|Windows*)
        WHEEL_NAME="adbc_driver_snowflake-${DRIVER_VERSION}-py3-none-win_amd64.whl"
        ;;
    *)
        echo "Unsupported operating system: $OS"
        exit 1
        ;;
esac

WHEEL_URL="${BASE_URL}/${WHEEL_NAME}"
WHEEL_PATH="${DRIVER_DIR}/${WHEEL_NAME}"

# Download the wheel if it doesn't exist
if [ ! -f "${WHEEL_PATH}" ]; then
    echo "Downloading ${WHEEL_NAME}..."
    curl -L -o "${WHEEL_PATH}" "${WHEEL_URL}"
    if [ $? -ne 0 ]; then
        echo "Failed to download ADBC driver"
        exit 1
    fi
else
    echo "Wheel already exists: ${WHEEL_PATH}"
fi

# Extract the shared library from the wheel
echo "Extracting driver library..."
cd "${DRIVER_DIR}"
unzip -o "${WHEEL_NAME}" "adbc_driver_snowflake/libadbc_driver_snowflake.so" 2>/dev/null

if [ $? -ne 0 ]; then
    echo "Failed to extract driver library"
    exit 1
fi

# Move the library to the driver directory
if [ -f "adbc_driver_snowflake/libadbc_driver_snowflake.so" ]; then
    mv "adbc_driver_snowflake/libadbc_driver_snowflake.so" .
    rmdir "adbc_driver_snowflake" 2>/dev/null
    echo "ADBC Snowflake driver extracted successfully to: ${DRIVER_DIR}/libadbc_driver_snowflake.so"
else
    echo "Failed to find driver library in wheel"
    exit 1
fi

# Make the library executable (important for some platforms)
chmod +x "libadbc_driver_snowflake.so" 2>/dev/null || true

cd - > /dev/null
echo "ADBC driver setup complete!"
echo "Driver location: ${DRIVER_DIR}/libadbc_driver_snowflake.so"