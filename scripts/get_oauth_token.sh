#!/bin/bash

# Script to obtain OAuth token from Auth0 or Okta
# Usage: ./get_oauth_token.sh [auth0|okta]

set -e

IDP_TYPE="${1:-auth0}"

if [ "$IDP_TYPE" = "auth0" ]; then
    echo "=== Getting OAuth Token from Auth0 ==="
    echo ""
    
    # Prompt for Auth0 credentials
    read -p "Enter Auth0 Domain (e.g., dev-abc123.us.auth0.com): " AUTH0_DOMAIN
    read -p "Enter Client ID: " CLIENT_ID
    read -s -p "Enter Client Secret: " CLIENT_SECRET
    echo ""
    read -p "Enter Snowflake Account URL (e.g., https://myaccount.snowflakecomputing.com): " AUDIENCE
    
    echo ""
    echo "Requesting token..."
    
    RESPONSE=$(curl -s --request POST \
      --url "https://${AUTH0_DOMAIN}/oauth/token" \
      --header 'content-type: application/json' \
      --data "{
        \"client_id\": \"${CLIENT_ID}\",
        \"client_secret\": \"${CLIENT_SECRET}\",
        \"audience\": \"${AUDIENCE}\",
        \"grant_type\": \"client_credentials\"
      }")
    
    # Check for errors
    if echo "$RESPONSE" | grep -q "error"; then
        echo "Error obtaining token:"
        echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"
        exit 1
    fi
    
    # Extract token
    TOKEN=$(echo "$RESPONSE" | grep -o '"access_token":"[^"]*' | cut -d'"' -f4)
    EXPIRES_IN=$(echo "$RESPONSE" | grep -o '"expires_in":[0-9]*' | cut -d':' -f2)
    
    echo ""
    echo "✅ Token obtained successfully!"
    echo ""
    echo "Access Token:"
    echo "$TOKEN"
    echo ""
    echo "Expires in: ${EXPIRES_IN} seconds ($(($EXPIRES_IN / 3600)) hours)"
    echo ""
    echo "To use in DuckDB:"
    echo "CREATE SECRET snowflake_oauth ("
    echo "    TYPE snowflake,"
    echo "    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',"
    echo "    USER 'auth0|${CLIENT_ID}',"
    echo "    AUTH_TYPE 'oauth',"
    echo "    TOKEN '${TOKEN}',"
    echo "    DATABASE 'YOUR_DATABASE',"
    echo "    WAREHOUSE 'COMPUTE_WH'"
    echo ");"
    
elif [ "$IDP_TYPE" = "okta" ]; then
    echo "=== Getting OAuth Token from Okta ==="
    echo ""
    
    # Prompt for Okta credentials
    read -p "Enter Okta Domain (e.g., yourcompany.okta.com): " OKTA_DOMAIN
    read -p "Enter Authorization Server (e.g., default): " AUTH_SERVER
    read -p "Enter Client ID: " CLIENT_ID
    read -s -p "Enter Client Secret: " CLIENT_SECRET
    echo ""
    
    echo ""
    echo "Requesting token..."
    
    RESPONSE=$(curl -s --request POST \
      --url "https://${OKTA_DOMAIN}/oauth2/${AUTH_SERVER}/v1/token" \
      --header 'content-type: application/x-www-form-urlencoded' \
      --data "grant_type=client_credentials&client_id=${CLIENT_ID}&client_secret=${CLIENT_SECRET}&scope=openid")
    
    # Check for errors
    if echo "$RESPONSE" | grep -q "error"; then
        echo "Error obtaining token:"
        echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"
        exit 1
    fi
    
    # Extract token
    TOKEN=$(echo "$RESPONSE" | grep -o '"access_token":"[^"]*' | cut -d'"' -f4)
    EXPIRES_IN=$(echo "$RESPONSE" | grep -o '"expires_in":[0-9]*' | cut -d':' -f2)
    
    echo ""
    echo "✅ Token obtained successfully!"
    echo ""
    echo "Access Token:"
    echo "$TOKEN"
    echo ""
    echo "Expires in: ${EXPIRES_IN} seconds ($(($EXPIRES_IN / 3600)) hours)"
    echo ""
    echo "To use in DuckDB:"
    echo "CREATE SECRET snowflake_oauth_okta ("
    echo "    TYPE snowflake,"
    echo "    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',"
    echo "    USER 'okta_user@yourcompany.com',"
    echo "    AUTH_TYPE 'oauth',"
    echo "    TOKEN '${TOKEN}',"
    echo "    DATABASE 'YOUR_DATABASE',"
    echo "    WAREHOUSE 'COMPUTE_WH'"
    echo ");"
    
else
    echo "Usage: $0 [auth0|okta]"
    echo ""
    echo "Examples:"
    echo "  $0 auth0    # Get token from Auth0"
    echo "  $0 okta     # Get token from Okta"
    exit 1
fi










