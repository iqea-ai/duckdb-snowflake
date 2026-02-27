#!/bin/bash

# Setup Key-Pair Authentication for Snowflake
# This script helps you generate RSA keys and configure Snowflake

set -e

echo "================================================"
echo "Snowflake Key-Pair Authentication Setup"
echo "================================================"
echo ""

KEY_DIR="$(pwd)"
PRIVATE_KEY="$KEY_DIR/snowflake_key.p8"
PUBLIC_KEY="$KEY_DIR/snowflake_key.pub"

# Check if keys already exist
if [ -f "$PRIVATE_KEY" ]; then
    echo "⚠️  Warning: Private key already exists at $PRIVATE_KEY"
    read -p "Do you want to overwrite it? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Keeping existing key. Exiting."
        exit 0
    fi
fi

echo "Step 1: Generating RSA Key Pair..."
echo "  - Generating private key (unencrypted for automation)..."
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out "$PRIVATE_KEY" -nocrypt

echo "  - Generating public key..."
openssl rsa -in "$PRIVATE_KEY" -pubout -out "$PUBLIC_KEY"

echo "✓ Keys generated successfully!"
echo ""
echo "  Private key: $PRIVATE_KEY"
echo "  Public key:  $PUBLIC_KEY"
echo ""

# Extract public key content (without header/footer)
PUBLIC_KEY_CONTENT=$(grep -v "BEGIN PUBLIC KEY" "$PUBLIC_KEY" | grep -v "END PUBLIC KEY" | tr -d '\n')

echo "Step 2: Add Public Key to Snowflake"
echo "================================================"
echo ""
echo "Run this SQL command in Snowsight (or snowsql):"
echo ""
echo "ALTER USER ${SNOWFLAKE_USER:-YOUR_USERNAME} SET RSA_PUBLIC_KEY='$PUBLIC_KEY_CONTENT';"
echo ""
echo "================================================"
echo ""

# Create .env file for key-pair auth
ENV_FILE=".env.keypair"
echo "Step 3: Creating environment file: $ENV_FILE"
cat > "$ENV_FILE" <<EOF
# Snowflake Key-Pair Authentication Configuration
# Source this file before running analysis: source .env.keypair

export SNOWFLAKE_ACCOUNT=${SNOWFLAKE_ACCOUNT:-YFEQLOK-BD05926}
export SNOWFLAKE_USER=${SNOWFLAKE_USER:-iqeadev}
export SNOWFLAKE_PRIVATE_KEY_PATH=$PRIVATE_KEY
export SNOWFLAKE_WAREHOUSE=${SNOWFLAKE_WAREHOUSE:-COMPUTE_WH}
EOF

echo "✓ Environment file created: $ENV_FILE"
echo ""

echo "Step 4: Update .gitignore to protect keys"
if ! grep -q "snowflake_key" .gitignore 2>/dev/null; then
    echo "snowflake_key.p8" >> .gitignore
    echo "snowflake_key.pub" >> .gitignore
    echo ".env.keypair" >> .gitignore
    echo "✓ Added keys to .gitignore"
else
    echo "✓ Keys already in .gitignore"
fi
echo ""

echo "================================================"
echo "Setup Complete! Next Steps:"
echo "================================================"
echo ""
echo "1. Run the SQL command shown above in Snowsight to add your public key"
echo ""
echo "2. Source the environment file:"
echo "   source .env.keypair"
echo ""
echo "3. Test the connection:"
echo "   duckdb -c \"INSTALL snowflake FROM community; LOAD snowflake; CREATE SECRET test (TYPE snowflake, ACCOUNT getenv('SNOWFLAKE_ACCOUNT'), USER getenv('SNOWFLAKE_USER'), PRIVATE_KEY_FILE getenv('SNOWFLAKE_PRIVATE_KEY_PATH'), WAREHOUSE getenv('SNOWFLAKE_WAREHOUSE')); ATTACH '' AS sf (TYPE snowflake, SECRET test, READ_ONLY); SHOW DATABASES;\""
echo ""
echo "4. Run the analysis using key-pair auth:"
echo "   duckdb query_analysis.db < 01_extract_query_history_keypair.sql"
echo ""
echo "5. Continue with the rest of the analysis:"
echo "   duckdb query_analysis.db < 02_analyze_queries.sql"
echo "   duckdb query_analysis.db < 03_classify_caching_strategy.sql"
echo ""
