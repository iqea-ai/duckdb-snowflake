# Authentication Methods Setup Guide

This guide provides step-by-step instructions for setting up all authentication methods supported by the DuckDB Snowflake extension.

## Prerequisites

- Snowflake account with ACCOUNTADMIN role access
- DuckDB with Snowflake extension installed
- Access to your Identity Provider (Auth0, Okta, etc.)

---

Note on account values:
- For all authentication methods (Password, Key Pair, OAuth, MFA, EXT_BROWSER, OKTA), use your Snowflake account identifier (e.g., `myaccount` or `xy12345.us-east-1`) as the `ACCOUNT` value in secrets and connection strings.
- Use the full Snowflake URL (`https://<account>.snowflakecomputing.com`) only in IdP configuration such as OAuth/SAML audience values.

## 1. OAuth 2.0 Authentication with Auth0

### Step 1: Configure Auth0

1. **Log in to Auth0 Dashboard**
   - Go to https://manage.auth0.com

2. **Create an API**
   - Navigate to **Applications → APIs**
   - Click **Create API**
   - **Name**: `Snowflake OAuth Integration`
   - **Identifier**: `https://YOUR_ACCOUNT.snowflakecomputing.com`
   - **Signing Algorithm**: `RS256`
   - Click **Create**

3. **Create Machine-to-Machine Application**
   - Navigate to **Applications → Applications**
   - Click **Create Application**
   - **Name**: `Snowflake M2M Client`
   - **Type**: Machine to Machine Applications
   - Select your Snowflake API
   - Click **Authorize**
   - Note down:
     - **Client ID**
     - **Client Secret**
     - **Domain** (e.g., `dev-abc123.us.auth0.com`)

### Step 2: Configure Snowflake Security Integration

```sql
USE ROLE ACCOUNTADMIN;

CREATE OR REPLACE SECURITY INTEGRATION auth0_oauth_integration
  TYPE = EXTERNAL_OAUTH
  ENABLED = TRUE
  EXTERNAL_OAUTH_TYPE = CUSTOM
  EXTERNAL_OAUTH_ISSUER = 'https://YOUR_AUTH0_DOMAIN/'  -- e.g., 'https://dev-abc123.us.auth0.com/'
  EXTERNAL_OAUTH_JWS_KEYS_URL = 'https://YOUR_AUTH0_DOMAIN/.well-known/jwks.json'
  EXTERNAL_OAUTH_AUDIENCE_LIST = ('https://YOUR_ACCOUNT.snowflakecomputing.com')
  EXTERNAL_OAUTH_TOKEN_USER_MAPPING_CLAIM = 'sub'
  EXTERNAL_OAUTH_SNOWFLAKE_USER_MAPPING_ATTRIBUTE = 'LOGIN_NAME'
  EXTERNAL_OAUTH_ANY_ROLE_MODE = 'ENABLE';

-- Grant usage to appropriate role
GRANT USAGE ON INTEGRATION auth0_oauth_integration TO ROLE PUBLIC;

-- Verify integration
DESCRIBE INTEGRATION auth0_oauth_integration;
```

### Step 3: Create Snowflake User for OAuth

The OAuth token's `sub` claim must match a Snowflake user's LOGIN_NAME:

```sql
-- Get the 'sub' claim from your OAuth token first (see step 4)
-- Then create user with matching LOGIN_NAME

CREATE USER IF NOT EXISTS "auth0|YOUR_CLIENT_ID"
  LOGIN_NAME = 'auth0|YOUR_CLIENT_ID'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "auth0|YOUR_CLIENT_ID";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
```

### Step 4: Obtain OAuth Token

Using curl:

```bash
curl --request POST \
  --url 'https://YOUR_AUTH0_DOMAIN/oauth/token' \
  --header 'content-type: application/json' \
  --data '{
    "client_id": "YOUR_CLIENT_ID",
    "client_secret": "YOUR_CLIENT_SECRET",
    "audience": "https://YOUR_ACCOUNT.snowflakecomputing.com",
    "grant_type": "client_credentials"
  }'
```

Response:
```json
{
  "access_token": "eyJhbGc...",
  "token_type": "Bearer",
  "expires_in": 86400
}
```

### Step 5: Test in DuckDB

```sql
CREATE SECRET snowflake_oauth (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'auth0|YOUR_CLIENT_ID',
    AUTH_TYPE 'oauth',
    TOKEN 'YOUR_ACCESS_TOKEN',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_oauth (TYPE snowflake, SECRET snowflake_oauth, READ_ONLY);
SELECT * FROM sf_oauth.information_schema.tables LIMIT 5;
```

### Environment Variables for Testing

```bash
export SNOWFLAKE_OAUTH_TOKEN="eyJhbGc..."
export SNOWFLAKE_OAUTH_USER="auth0|YOUR_CLIENT_ID"
```

---

## 2. Key Pair Authentication

### Step 1: Generate RSA Key Pair

**Option A: Encrypted Private Key (Recommended)**

```bash
# Generate encrypted private key (PKCS#8 format)
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out rsa_key.p8 -passout pass:YOUR_PASSPHRASE

# Generate public key
openssl rsa -in rsa_key.p8 -pubout -out rsa_key.pub -passin pass:YOUR_PASSPHRASE

# Extract public key content (without headers/footers) for Snowflake
grep -v "BEGIN PUBLIC KEY" rsa_key.pub | grep -v "END PUBLIC KEY" | tr -d '\n' > rsa_key_oneline.txt
```

**Option B: Unencrypted Private Key (Less Secure)**

```bash
# Generate unencrypted private key
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out rsa_key_unencrypted.p8 -nocrypt

# Generate public key
openssl rsa -in rsa_key_unencrypted.p8 -pubout -out rsa_key.pub

# Extract public key content
grep -v "BEGIN PUBLIC KEY" rsa_key.pub | grep -v "END PUBLIC KEY" | tr -d '\n' > rsa_key_oneline.txt
```

### Step 2: Register Public Key in Snowflake

```sql
USE ROLE ACCOUNTADMIN;

-- Assign public key to existing user
ALTER USER YOUR_USERNAME SET RSA_PUBLIC_KEY='MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...';

-- Or create new user with public key
CREATE USER IF NOT EXISTS keypair_user
  LOGIN_NAME = 'keypair_user'
  RSA_PUBLIC_KEY='MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC';

GRANT ROLE PUBLIC TO USER keypair_user;
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;

-- Verify the key is set
DESC USER YOUR_USERNAME;
```

### Step 3: Test in DuckDB

**With Encrypted Private Key:**

```sql
CREATE SECRET snowflake_keypair (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'YOUR_USERNAME',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/path/to/rsa_key.p8',
    PRIVATE_KEY_PASSPHRASE 'YOUR_PASSPHRASE',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_keypair (TYPE snowflake, SECRET snowflake_keypair, READ_ONLY);
SELECT * FROM sf_keypair.information_schema.tables LIMIT 5;
```

**With Unencrypted Private Key:**

```sql
CREATE SECRET snowflake_keypair_unencrypted (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'YOUR_USERNAME',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/path/to/rsa_key_unencrypted.p8',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'COMPUTE_WH'
);
```

### Environment Variables for Testing

```bash
export SNOWFLAKE_KEYPAIR_USER="YOUR_USERNAME"
export SNOWFLAKE_PRIVATE_KEY_PATH="/absolute/path/to/rsa_key.p8"
export SNOWFLAKE_PRIVATE_KEY_PASSPHRASE="YOUR_PASSPHRASE"
```

---

## 3. Native Okta Authentication

### Step 1: Set Up Okta for Snowflake

1. **Configure Snowflake for Okta**
   - In Snowflake, navigate to Admin → Security → Network Policy
   - Note your Okta domain (e.g., `https://yourcompany.okta.com`)

2. **Verify Okta Configuration**
   - Ensure users are provisioned in Okta
   - Verify Okta SSO is enabled for Snowflake

### Step 2: Configure Snowflake

```sql
USE ROLE ACCOUNTADMIN;

-- Verify user exists and can authenticate via Okta
CREATE USER IF NOT EXISTS okta_user
  LOGIN_NAME = 'okta_user@yourcompany.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC';

GRANT ROLE PUBLIC TO USER okta_user;
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
```

### Step 3: Test in DuckDB

**Note**: Native Okta authentication requires browser interaction for SSO.

```sql
CREATE SECRET snowflake_okta (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'okta_user@yourcompany.com',
    AUTH_TYPE 'okta',
    OKTA_URL 'https://yourcompany.okta.com',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'COMPUTE_WH'
);

-- This will open a browser window for Okta authentication
ATTACH '' AS sf_okta (TYPE snowflake, SECRET snowflake_okta, READ_ONLY);
SELECT * FROM sf_okta.information_schema.tables LIMIT 5;
```

### Environment Variables for Testing

```bash
export SNOWFLAKE_OKTA_USER="okta_user@yourcompany.com"
export SNOWFLAKE_OKTA_URL="https://yourcompany.okta.com"
```

---

## 4. Multi-Factor Authentication (MFA)

### Step 1: Enable MFA in Snowflake

```sql
USE ROLE ACCOUNTADMIN;

-- Enable MFA for a user
ALTER USER YOUR_USERNAME SET MINS_TO_BYPASS_MFA = 0;

-- Or create new MFA-enabled user
CREATE USER IF NOT EXISTS mfa_user
  PASSWORD = 'SECURE_PASSWORD'
  LOGIN_NAME = 'mfa_user'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MINS_TO_BYPASS_MFA = 0;

GRANT ROLE PUBLIC TO USER mfa_user;
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
```

### Step 2: Enroll User in MFA

1. User logs into Snowflake web UI
2. Navigate to user preferences
3. Enroll in MFA using:
   - Duo Mobile
   - Google Authenticator
   - Microsoft Authenticator
4. Complete enrollment process

### Step 3: Test in DuckDB

**Note**: MFA authentication requires providing the MFA token during connection.

```sql
CREATE SECRET snowflake_mfa (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'mfa_user',
    PASSWORD 'SECURE_PASSWORD',
    AUTH_TYPE 'mfa',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'COMPUTE_WH'
);

-- MFA prompt will appear during ATTACH
ATTACH '' AS sf_mfa (TYPE snowflake, SECRET snowflake_mfa, READ_ONLY);
SELECT * FROM sf_mfa.information_schema.tables LIMIT 5;
```

### Environment Variables for Testing

```bash
export SNOWFLAKE_MFA_USER="mfa_user"
export SNOWFLAKE_MFA_PASSWORD="SECURE_PASSWORD"
```

---

## Testing All Authentication Methods

After setting up all authentication methods, use the comprehensive test:

```bash
# Set all required environment variables
export SNOWFLAKE_ACCOUNT="YOUR_ACCOUNT_IDENTIFIER"
export SNOWFLAKE_DATABASE="YOUR_DATABASE"

# Password auth (already tested)
export SNOWFLAKE_USERNAME="your_user"
export SNOWFLAKE_PASSWORD="your_password"

# OAuth
export SNOWFLAKE_OAUTH_USER="auth0|YOUR_CLIENT_ID"
export SNOWFLAKE_OAUTH_TOKEN="YOUR_ACCESS_TOKEN"

# Key Pair
export SNOWFLAKE_KEYPAIR_USER="keypair_user"
export SNOWFLAKE_PRIVATE_KEY_PATH="/absolute/path/to/rsa_key.p8"
export SNOWFLAKE_PRIVATE_KEY_PASSPHRASE="YOUR_PASSPHRASE"

# Okta
export SNOWFLAKE_OKTA_USER="okta_user@yourcompany.com"
export SNOWFLAKE_OKTA_URL="https://yourcompany.okta.com"

# MFA
export SNOWFLAKE_MFA_USER="mfa_user"
export SNOWFLAKE_MFA_PASSWORD="SECURE_PASSWORD"

# Run the comprehensive test
cd duckdb-snowflake
make test_release
```

---

## Troubleshooting

### OAuth Issues

**Problem**: Token validation fails
```
Error: Invalid OAuth token
```

**Solutions**:
- Verify token hasn't expired (check `expires_in`)
- Ensure `audience` matches Snowflake account URL exactly
- Verify `sub` claim matches Snowflake user's LOGIN_NAME
- Check security integration is enabled: `SHOW INTEGRATIONS;`

### Key Pair Issues

**Problem**: Public key not accepted
```
Error: JWT token is invalid
```

**Solutions**:
- Ensure public key is in correct format (single line, no headers)
- Verify key size is 2048 bits minimum
- Check private key passphrase is correct
- Verify user has RSA_PUBLIC_KEY set: `DESC USER username;`

### Okta Issues

**Problem**: Browser doesn't open
```
Error: Failed to authenticate with Okta
```

**Solutions**:
- Verify OKTA_URL is correct (no trailing slash)
- Ensure Okta is configured for Snowflake SSO
- Check network connectivity to Okta
- Verify user exists in both Okta and Snowflake

### MFA Issues

**Problem**: MFA prompt not appearing
```
Error: Authentication failed
```

**Solutions**:
- Verify user is enrolled in MFA
- Check `MINS_TO_BYPASS_MFA` is set to 0
- Ensure MFA is enabled at account level
- Try re-enrolling user in MFA

---

## Security Best Practices

1. **OAuth**
   - Rotate tokens regularly
   - Use short-lived tokens (24 hours or less)
   - Store tokens in secure credential managers
   - Never commit tokens to version control

2. **Key Pair**
   - Always use encrypted private keys in production
   - Use strong passphrases (16+ characters)
   - Rotate keys every 90 days
   - Store private keys in secure locations (e.g., HashiCorp Vault)
   - Never share private keys

3. **Okta/MFA**
   - Require MFA for all production users
   - Use hardware tokens for highest security
   - Monitor authentication logs regularly
   - Implement network policies to restrict access

4. **General**
   - Use READ_ONLY mode when possible
   - Implement least privilege access
   - Monitor and audit all connections
   - Use secure connection strings (no plaintext passwords in logs)

---

## Next Steps

1. Choose authentication methods appropriate for your environment
2. Follow setup instructions for each method
3. Test each method individually
4. Run comprehensive authentication test suite
5. Deploy to production with appropriate security measures

