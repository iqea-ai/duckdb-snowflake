#!/bin/bash

# Test OIDC Integration Script
# This script tests the complete OIDC flow from Keycloak to DuckDB to Snowflake

echo "🚀 Testing OIDC Integration Flow"
echo "================================="

# Configuration
KEYCLOAK_URL="http://localhost:8080"
CLIENT_ID="duckdb-snowflake-client"
CLIENT_SECRET="lIRSI8idmj3UMvzjmWmFV9HdnJ6BhaKP"
DUCKDB_PATH="./build/release/duckdb"
EXTENSION_PATH="./build/release/snowflake.duckdb_extension"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Step 1: Get OIDC token from Keycloak
echo "Step 1: Getting OIDC token from Keycloak..."
TOKEN_RESPONSE=$(curl -s -X POST "$KEYCLOAK_URL/realms/master/protocol/openid-connect/token" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "grant_type=client_credentials&client_id=$CLIENT_ID&client_secret=$CLIENT_SECRET")

if echo "$TOKEN_RESPONSE" | jq -e '.access_token' > /dev/null; then
    ACCESS_TOKEN=$(echo "$TOKEN_RESPONSE" | jq -r .access_token)
    EXPIRES_IN=$(echo "$TOKEN_RESPONSE" | jq -r .expires_in)
    print_status "Token obtained successfully (expires in $EXPIRES_IN seconds)"
    
    # Save token to file
    echo "$ACCESS_TOKEN" > /tmp/keycloak_token.txt
    print_status "Token saved to /tmp/keycloak_token.txt"
else
    print_error "Failed to get token from Keycloak"
    echo "$TOKEN_RESPONSE"
    exit 1
fi

# Step 2: Test DuckDB extension with OIDC token
echo ""
echo "Step 2: Testing DuckDB extension with OIDC token..."

# Test file-based token
echo "Testing file-based token..."
FILE_RESULT=$($DUCKDB_PATH -cmd "LOAD '$EXTENSION_PATH'; CREATE SECRET IF NOT EXISTS snowflake_oidc_test (TYPE snowflake, account 'test_account', database 'test_db', token_file_path '/tmp/keycloak_token.txt');" 2>&1)

if [[ $FILE_RESULT == *"Success"* ]]; then
    print_status "File-based OIDC secret created successfully"
else
    print_error "Failed to create file-based OIDC secret"
    echo "$FILE_RESULT"
fi

# Test direct token
echo "Testing direct token..."
DIRECT_RESULT=$($DUCKDB_PATH -cmd "LOAD '$EXTENSION_PATH'; CREATE SECRET IF NOT EXISTS snowflake_oidc_direct_test (TYPE snowflake, account 'test_account', database 'test_db', oidc_token '$ACCESS_TOKEN');" 2>&1)

if [[ $DIRECT_RESULT == *"Success"* ]]; then
    print_status "Direct OIDC secret created successfully"
else
    print_error "Failed to create direct OIDC secret"
    echo "$DIRECT_RESULT"
fi

# Step 3: List secrets
echo ""
echo "Step 3: Listing Snowflake secrets..."
SECRETS=$($DUCKDB_PATH -cmd "SELECT name, type FROM duckdb_secrets() WHERE type = 'snowflake';" 2>&1)
echo "$SECRETS"

# Step 4: Test token validation
echo ""
echo "Step 4: Validating token structure..."
TOKEN_PAYLOAD=$(echo "$ACCESS_TOKEN" | cut -d'.' -f2 | base64 -d 2>/dev/null)
if echo "$TOKEN_PAYLOAD" | jq -e '.sub' > /dev/null; then
    SUBJECT=$(echo "$TOKEN_PAYLOAD" | jq -r .sub)
    ISSUER=$(echo "$TOKEN_PAYLOAD" | jq -r .iss)
    AUDIENCE=$(echo "$TOKEN_PAYLOAD" | jq -r .aud)
    
    print_status "Token validation successful:"
    echo "  Subject: $SUBJECT"
    echo "  Issuer: $ISSUER"
    echo "  Audience: $AUDIENCE"
else
    print_error "Failed to decode token payload"
fi

echo ""
echo "🎉 OIDC Integration Test Complete!"
echo "================================="
echo ""
echo "Next steps:"
echo "1. Run the Snowflake SQL commands in snowflake_oidc_setup.sql"
echo "2. Update the account/database names in the secrets"
echo "3. Test actual Snowflake connectivity"
echo ""
echo "Token expires in: $EXPIRES_IN seconds"
echo "Token file: /tmp/keycloak_token.txt"
