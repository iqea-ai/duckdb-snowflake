#!/bin/bash

# Test script for Snowflake extension
# Usage: ./scripts/test_snowflake.sh

set -e

echo "=== Snowflake Extension Test Script ==="

# Check if environment variables are set
if [ -z "$SNOWFLAKE_ACCOUNT" ] || [ -z "$SNOWFLAKE_USERNAME" ] || [ -z "$SNOWFLAKE_PASSWORD" ] || [ -z "$SNOWFLAKE_DATABASE" ]; then
    echo "Error: Snowflake environment variables not set."
    echo "Please set the following environment variables:"
    echo "  export SNOWFLAKE_ACCOUNT='your_account'"
    echo "  export SNOWFLAKE_USERNAME='your_username'"
    echo "  export SNOWFLAKE_PASSWORD='your_password'"
    echo "  export SNOWFLAKE_DATABASE='your_database'"
    echo ""
    echo "Example:"
    echo "  export SNOWFLAKE_ACCOUNT='myaccount.snowflakecomputing.com'"
    echo "  export SNOWFLAKE_USERNAME='myuser'"
    echo "  export SNOWFLAKE_PASSWORD='mypass'"
    echo "  export SNOWFLAKE_DATABASE='SNOWFLAKE_SAMPLE_DATA'"
    exit 1
fi

echo "Environment variables set:"
echo "  SNOWFLAKE_ACCOUNT: $SNOWFLAKE_ACCOUNT"
echo "  SNOWFLAKE_USERNAME: $SNOWFLAKE_USERNAME"
echo "  SNOWFLAKE_DATABASE: $SNOWFLAKE_DATABASE"
echo ""

# Build the extension if not already built
if [ ! -f "build/debug/test/unittest" ]; then
    echo "Building extension..."
    make debug-build
fi

echo "Running Snowflake tests..."
make test-snowflake

echo "=== All tests completed successfully! ==="
