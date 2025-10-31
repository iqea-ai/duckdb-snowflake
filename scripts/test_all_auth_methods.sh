#!/bin/bash

# Script to run comprehensive authentication method tests
# This script helps set up environment variables and run the authentication tests

set -e

echo "==============================================="
echo "Snowflake Authentication Methods Test Runner"
echo "==============================================="
echo ""

# Function to check if variable is set
check_env_var() {
    local var_name=$1
    local var_value="${!var_name}"
    if [ -z "$var_value" ]; then
        echo " Missing: $var_name"
        return 1
    else
        echo " Set: $var_name"
        return 0
    fi
}

# Common variables
echo "Checking common environment variables..."
check_env_var "SNOWFLAKE_ACCOUNT" || echo "   Please set: export SNOWFLAKE_ACCOUNT='your_account.snowflakecomputing.com'"
check_env_var "SNOWFLAKE_DATABASE" || echo "   Please set: export SNOWFLAKE_DATABASE='your_database'"

echo ""
echo "Checking Password authentication..."
check_env_var "SNOWFLAKE_USERNAME" || echo "   Please set: export SNOWFLAKE_USERNAME='your_username'"
check_env_var "SNOWFLAKE_PASSWORD" || echo "   Please set: export SNOWFLAKE_PASSWORD='your_password'"

echo ""
echo "Checking OAuth authentication..."
check_env_var "SNOWFLAKE_OAUTH_USER" || echo "   Please set: export SNOWFLAKE_OAUTH_USER='auth0|client_id'"
check_env_var "SNOWFLAKE_OAUTH_TOKEN" || echo "   Please set: export SNOWFLAKE_OAUTH_TOKEN='your_oauth_token'"

echo ""
echo "Checking Key Pair authentication..."
check_env_var "SNOWFLAKE_KEYPAIR_USER" || echo "   Please set: export SNOWFLAKE_KEYPAIR_USER='keypair_user'"
check_env_var "SNOWFLAKE_PRIVATE_KEY_PATH" || echo "   Please set: export SNOWFLAKE_PRIVATE_KEY_PATH='/path/to/rsa_key.p8'"
check_env_var "SNOWFLAKE_PRIVATE_KEY_PASSPHRASE" || echo "   Please set: export SNOWFLAKE_PRIVATE_KEY_PASSPHRASE='your_passphrase'"

echo ""
echo "Checking Okta authentication..."
check_env_var "SNOWFLAKE_OKTA_USER" || echo "   Please set: export SNOWFLAKE_OKTA_USER='okta_user@company.com'"
check_env_var "SNOWFLAKE_OKTA_URL" || echo "   Please set: export SNOWFLAKE_OKTA_URL='https://yourcompany.okta.com'"

echo ""
echo "Checking MFA authentication..."
check_env_var "SNOWFLAKE_MFA_USER" || echo "   Please set: export SNOWFLAKE_MFA_USER='mfa_user'"
check_env_var "SNOWFLAKE_MFA_PASSWORD" || echo "   Please set: export SNOWFLAKE_MFA_PASSWORD='your_password'"

echo ""
echo "==============================================="
echo ""

# Count missing variables
missing_count=0
for var in SNOWFLAKE_ACCOUNT SNOWFLAKE_DATABASE SNOWFLAKE_USERNAME SNOWFLAKE_PASSWORD \
           SNOWFLAKE_OAUTH_USER SNOWFLAKE_OAUTH_TOKEN \
           SNOWFLAKE_KEYPAIR_USER SNOWFLAKE_PRIVATE_KEY_PATH SNOWFLAKE_PRIVATE_KEY_PASSPHRASE \
           SNOWFLAKE_OKTA_USER SNOWFLAKE_OKTA_URL \
           SNOWFLAKE_MFA_USER SNOWFLAKE_MFA_PASSWORD; do
    if [ -z "${!var}" ]; then
        ((missing_count++))
    fi
done

if [ $missing_count -gt 0 ]; then
    echo "  Warning: $missing_count required environment variable(s) not set"
    echo ""
    echo "Please set all required variables before running tests."
    echo "See docs/AUTHENTICATION_SETUP.md for setup instructions."
    echo ""
    read -p "Do you want to continue anyway? (y/N) " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Test cancelled."
        exit 1
    fi
fi

echo "Running authentication tests..."
echo ""

# Determine the build directory
if [ -d "build/release" ]; then
    BUILD_DIR="build/release"
elif [ -d "build/debug" ]; then
    BUILD_DIR="build/debug"
else
    echo " Error: Build directory not found. Please run 'make' first."
    exit 1
fi

# Check if DuckDB binary exists
if [ ! -f "$BUILD_DIR/duckdb" ]; then
    echo " Error: DuckDB binary not found in $BUILD_DIR"
    echo "Please build the project first with: make"
    exit 1
fi

# Run the test
echo "Running test: test/sql/snowflake_all_auth_methods.test"
echo ""

cd "$(dirname "$0")/.."

# Run the test using DuckDB's test runner
if command -v python3 &> /dev/null; then
    python3 duckdb/scripts/run_tests_one_by_one.py $BUILD_DIR/test/unittest "test/sql/snowflake_all_auth_methods.test"
else
    # Alternative: run directly with DuckDB
    $BUILD_DIR/duckdb < test/sql/snowflake_all_auth_methods.test
fi

echo ""
echo "==============================================="
echo "Test completed!"
echo "==============================================="

