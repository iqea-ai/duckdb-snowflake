# Solving the MFA Issue

## The Problem

Your Snowflake account has MFA enabled. Password authentication can't provide the MFA token automatically, so automated scripts fail.

## The Solution: Key-Pair Authentication ✅

Key-pair authentication works with MFA-enabled accounts because the private key proves your identity without needing the MFA token for each connection.

## Quick Setup (5 minutes)

### Option A: Automated Setup (Recommended)

```bash
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/query_logs

# Run the setup script
./setup_keypair_auth.sh
```

This will:
1. ✓ Generate RSA key pair
2. ✓ Show you the SQL command to run in Snowsight
3. ✓ Create `.env.keypair` with your configuration
4. ✓ Update `.gitignore` to protect your keys

Then:
1. Copy the `ALTER USER` SQL command from the output
2. Run it in Snowsight (https://app.snowflake.com/)
3. Come back and test the connection

### Option B: Manual Setup

```bash
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/query_logs

# 1. Generate private key
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out snowflake_key.p8 -nocrypt

# 2. Generate public key
openssl rsa -in snowflake_key.p8 -pubout -out snowflake_key.pub

# 3. Display public key (copy this)
cat snowflake_key.pub
```

Then in Snowsight, run:
```sql
-- Remove the BEGIN/END lines and newlines from the public key, then:
ALTER USER iqeadev SET RSA_PUBLIC_KEY='MIIBIjANBg...your_key_here...IDAQAB';
```

## Testing the Connection

After adding the public key to Snowflake:

```bash
# Source the environment (using the existing .env with your account details)
source /Users/venkata/ddbsf-ext/duckdb-snowflake/examples/notebooks/.env

# Add the private key path
export SNOWFLAKE_PRIVATE_KEY_PATH=/Users/venkata/ddbsf-ext/duckdb-snowflake/query_logs/snowflake_key.p8

# Test connection
duckdb -c "
INSTALL snowflake FROM community;
LOAD snowflake;

CREATE SECRET test (
    TYPE snowflake,
    ACCOUNT getenv('SNOWFLAKE_ACCOUNT'),
    USER getenv('SNOWFLAKE_USER'),
    PRIVATE_KEY_FILE getenv('SNOWFLAKE_PRIVATE_KEY_PATH'),
    WAREHOUSE getenv('SNOWFLAKE_WAREHOUSE')
);

ATTACH '' AS sf (TYPE snowflake, SECRET test, READ_ONLY);
SHOW DATABASES;
"
```

If this shows your databases, you're good to go! ✓

## Running the Analysis with Key-Pair Auth

```bash
cd /Users/venkata/ddbsf-ext/duckdb-snowflake/query_logs

# Set environment variables
source /Users/venkata/ddbsf-ext/duckdb-snowflake/examples/notebooks/.env
export SNOWFLAKE_PRIVATE_KEY_PATH=$(pwd)/snowflake_key.p8

# Run the analysis (using the key-pair version)
duckdb query_analysis.db < 01_extract_query_history_keypair.sql
duckdb query_analysis.db < 02_analyze_queries.sql
duckdb query_analysis.db < 03_classify_caching_strategy.sql

# Generate visualizations
python visualize_results.py
```

## Alternative: Disable MFA for This User

If you have admin access and prefer password auth:

```sql
-- In Snowsight, as ACCOUNTADMIN
ALTER USER iqeadev SET DISABLE_MFA = TRUE;
```

Then you can use the original `01_extract_query_history.sql` with password auth.

⚠️ **Security Note**: This reduces security. Key-pair auth is more secure and recommended.

## Files Created

- `setup_keypair_auth.sh` - Automated setup script
- `01_extract_query_history_keypair.sql` - Key-pair version of extraction script
- `.env.keypair` - Environment file (created by setup script)
- `snowflake_key.p8` - Your private key (keep secure!)
- `snowflake_key.pub` - Your public key

## Which Approach?

| Approach | Pros | Cons |
|----------|------|------|
| **Key-Pair Auth** ✅ | Works with MFA, more secure, industry standard | Requires initial setup |
| **Disable MFA** | Simpler, uses password | Less secure, not recommended |

**Recommendation**: Use key-pair authentication. It's the standard approach for automated Snowflake access.

## Next Steps

1. Run `./setup_keypair_auth.sh`
2. Add the public key to Snowflake (copy the SQL from script output)
3. Test the connection
4. Run the analysis!

## Troubleshooting

### "Permission denied" on private key
```bash
chmod 600 snowflake_key.p8
```

### "JWT token is invalid"
The public key wasn't added correctly. Make sure to:
- Remove the `-----BEGIN PUBLIC KEY-----` and `-----END PUBLIC KEY-----` lines
- Remove all newlines (should be one continuous string)

### "User not found"
Make sure you're using the correct username. Check with:
```sql
SELECT CURRENT_USER();
```

## Summary

**No, just completing MFA enrollment won't fix password-based automation.**

**Yes, setting up key-pair authentication will fix it** and is the recommended solution for automated access with MFA-enabled accounts.

Run `./setup_keypair_auth.sh` to get started!
