# SSO Testing Guide for UI Team

This guide helps the UI team test Single Sign-On (SSO) authentication with the DuckDB Snowflake extension.

## Overview

The extension supports two SSO authentication methods:
1. **EXT_BROWSER** (SAML 2.0) - Works with any SAML provider (Okta, Auth0, AD FS, Azure AD)
2. **OKTA** (Native Okta) - Okta-specific integration

Both methods require browser interaction - a browser window will open for authentication.

---

## Prerequisites Setup (Admin Required)

Before testing, the following must be configured by an admin:

### For EXT_BROWSER (SAML 2.0) SSO

#### 1. Snowflake Configuration (ACCOUNTADMIN role required)

```sql
USE ROLE ACCOUNTADMIN;

-- Create SAML security integration
CREATE SECURITY INTEGRATION sso_saml_integration
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'urn:YOUR_IDP_DOMAIN'  -- e.g., 'urn:dev-abc123.us.auth0.com' or Okta issuer
  SAML2_SSO_URL = 'https://YOUR_IDP_DOMAIN/samlp/YOUR_CLIENT_ID'  -- SSO URL from IdP
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = 'YOUR_IDP_SIGNING_CERTIFICATE'  -- Certificate from IdP
  SAML2_ENABLE_SP_INITIATED = TRUE;

GRANT USAGE ON INTEGRATION sso_saml_integration TO ROLE PUBLIC;

-- Get Snowflake metadata URL (needed for IdP configuration)
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('sso_saml_integration');
```

#### 2. Identity Provider Configuration

**For Auth0:**
- Go to Auth0 Dashboard → Applications → Create Application
- Choose "Regular Web Application"
- Enable "SAML2 Web App" addon
- Configure:
  - Application Callback URL: Use Snowflake's ACS URL from metadata
  - Audience: `https://YOUR_ACCOUNT.snowflakecomputing.com`
  - Name Identifier Format: `urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress`
- Download signing certificate and add to Snowflake integration

**For Okta:**
- Okta Admin Console → Applications → Create App Integration
- Choose "SAML 2.0"
- Configure Single sign-on URL and Audience URI from Snowflake metadata
- Complete attribute mapping
- Add Okta's X.509 certificate to Snowflake integration

#### 3. User Setup in Snowflake

```sql
-- Create user (if not exists) - LOGIN_NAME should match IdP identifier
CREATE USER IF NOT EXISTS "user@company.com"
  LOGIN_NAME = 'user@company.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "user@company.com";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
```

### For Native OKTA SSO

#### 1. Verify Okta Configuration
- Ensure Okta is configured for Snowflake SSO in your organization
- Note your Okta domain (e.g., `https://yourcompany.okta.com`)

#### 2. User Setup in Snowflake

```sql
USE ROLE ACCOUNTADMIN;

CREATE USER IF NOT EXISTS okta_user
  LOGIN_NAME = 'okta_user@yourcompany.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC';

GRANT ROLE PUBLIC TO USER okta_user;
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
```

---

## Testing SSO Authentication

### Test 1: EXT_BROWSER (SAML 2.0) SSO

#### Step 1: Create Secret

```sql
-- Load the Snowflake extension
LOAD snowflake;

-- Create secret with EXT_BROWSER auth type
CREATE SECRET my_sso_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',  -- e.g., 'myaccount' or 'xy12345.us-east-1'
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    AUTH_TYPE 'ext_browser'  -- or 'externalbrowser'
);
```

**Note:** Account should be the identifier (e.g., `myaccount`), NOT the full URL.

#### Step 2: Connect (Browser Will Open)

```sql
-- This will open a browser window for SSO authentication
ATTACH '' AS sf (TYPE snowflake, SECRET my_sso_secret, READ_ONLY);
```

**What to expect:**
1. Browser window opens automatically
2. Redirects to your IdP login page (Auth0/Okta/etc.)
3. User enters credentials
4. IdP redirects back to Snowflake
5. Connection is established

#### Step 3: Verify Connection

```sql
-- List databases (should show 'sf' database)
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf';

-- Query Snowflake data
SELECT * FROM sf.information_schema.tables LIMIT 10;

-- Test a simple query
SELECT COUNT(*) FROM sf.YOUR_SCHEMA.YOUR_TABLE;
```

### Test 2: Native OKTA SSO

#### Step 1: Create Secret

```sql
CREATE SECRET my_okta_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'okta_user@yourcompany.com',  -- Must match Okta username
    AUTH_TYPE 'okta',
    OKTA_URL 'https://yourcompany.okta.com',  -- Your Okta domain
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE'
);
```

#### Step 2: Connect (Browser Will Open)

```sql
-- This will open a browser window for Okta SSO authentication
ATTACH '' AS sf_okta (TYPE snowflake, SECRET my_okta_secret, READ_ONLY);
```

**What to expect:**
1. Browser window opens automatically
2. Redirects to Okta login page
3. User enters Okta credentials
4. Okta authenticates and redirects back
5. Connection is established

#### Step 3: Verify Connection

```sql
-- List databases
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf_okta';

-- Query Snowflake data
SELECT * FROM sf_okta.information_schema.tables LIMIT 10;
```

---

## What to Test

### UI Testing Checklist

1. **Secret Creation**
   - [ ] Can create secret with `AUTH_TYPE 'ext_browser'`
   - [ ] Can create secret with `AUTH_TYPE 'okta'`
   - [ ] Secret appears in `duckdb_secrets()` view
   - [ ] Error handling for invalid account/warehouse/database

2. **Browser Interaction**
   - [ ] Browser window opens automatically when using `ATTACH`
   - [ ] Browser redirects to correct IdP login page
   - [ ] After login, browser redirects back successfully
   - [ ] Connection completes without errors

3. **Connection Verification**
   - [ ] Can query `information_schema.tables`
   - [ ] Can query `information_schema.schemata`
   - [ ] Can execute SELECT queries on Snowflake tables
   - [ ] Can perform JOINs and aggregations
   - [ ] Connection persists for multiple queries

4. **Error Handling**
   - [ ] Invalid credentials show appropriate error
   - [ ] Network errors are handled gracefully
   - [ ] Browser cancellation/close is handled
   - [ ] Expired sessions show appropriate error

5. **Multiple Connections**
   - [ ] Can create multiple SSO connections simultaneously
   - [ ] Each connection uses separate browser session
   - [ ] Connections don't interfere with each other

---

## Troubleshooting

### Browser Doesn't Open

**Possible causes:**
- Running in headless/containerized environment (SSO requires browser)
- Browser blocked by security settings
- Missing browser dependencies

**Solutions:**
- Ensure running in environment with GUI/browser access
- Check browser permissions
- Verify `DISPLAY` environment variable (Linux)

### Authentication Fails

**Error:** "Failed to authenticate" or "Invalid credentials"

**Check:**
1. Snowflake security integration is enabled: `SHOW INTEGRATIONS;`
2. User exists in Snowflake: `SHOW USERS LIKE 'user@company.com';`
3. User has proper grants: `SHOW GRANTS TO USER 'user@company.com';`
4. IdP configuration matches Snowflake integration settings
5. SAML certificate is valid and not expired

### Connection Established But Queries Fail

**Error:** "Insufficient privileges" or "Object does not exist"

**Check:**
1. User has `USAGE` on database: `GRANT USAGE ON DATABASE YOUR_DB TO ROLE PUBLIC;`
2. User has `USAGE` on warehouse: `GRANT USAGE ON WAREHOUSE YOUR_WH TO ROLE PUBLIC;`
3. User has `SELECT` on tables: `GRANT SELECT ON ALL TABLES IN SCHEMA YOUR_SCHEMA TO ROLE PUBLIC;`

### Browser Opens But Redirects Fail

**Possible causes:**
- Network connectivity issues
- Firewall blocking redirect URLs
- IdP configuration incorrect

**Solutions:**
- Verify network connectivity to IdP and Snowflake
- Check firewall rules allow redirects
- Verify IdP callback URLs match Snowflake metadata

---

## Quick Test Script

Here's a complete test script you can use:

```sql
-- Load extension
LOAD snowflake;

-- Test EXT_BROWSER SSO
CREATE SECRET test_sso (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    AUTH_TYPE 'ext_browser'
);

-- Connect (browser opens)
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_sso, READ_ONLY);

-- Verify connection
SELECT COUNT(*) FROM sf_test.information_schema.tables;

-- Test query
SELECT * FROM sf_test.information_schema.schemata LIMIT 5;

-- Cleanup
DETACH sf_test;
DROP SECRET test_sso;
```

---

## Environment Variables (Optional)

For automated testing, you can use environment variables:

```bash
export SNOWFLAKE_ACCOUNT="YOUR_ACCOUNT_IDENTIFIER"
export SNOWFLAKE_DATABASE="YOUR_DATABASE"
export SNOWFLAKE_WAREHOUSE="YOUR_WAREHOUSE"
export SNOWFLAKE_OKTA_USER="user@company.com"
export SNOWFLAKE_OKTA_URL="https://yourcompany.okta.com"
```

Then reference in SQL:
```sql
CREATE SECRET test_sso (
    TYPE snowflake,
    ACCOUNT '${SNOWFLAKE_ACCOUNT}',
    DATABASE '${SNOWFLAKE_DATABASE}',
    WAREHOUSE '${SNOWFLAKE_WAREHOUSE}',
    AUTH_TYPE 'ext_browser'
);
```

---

## Important Notes

1. **Browser Required**: Both SSO methods require interactive browser access. They won't work in headless/containerized environments without special browser setup.

2. **Account Format**: Use the account identifier (e.g., `myaccount` or `xy12345.us-east-1`), NOT the full URL (`https://myaccount.snowflakecomputing.com`).

3. **Username for OKTA**: For native Okta auth, the `USER` field must match your Okta username exactly.

4. **Session Persistence**: SSO sessions are managed by Snowflake. The browser session may persist, but you may need to re-authenticate after token expiration.

5. **Security**: SSO credentials are stored securely in DuckDB secrets. Never commit secrets to version control.

---

## Getting Help

If you encounter issues:
1. Check the [AUTHENTICATION.md](docs/AUTHENTICATION.md) documentation
2. Review [AUTHENTICATION_SETUP.md](docs/AUTHENTICATION_SETUP.md) for detailed setup
3. Verify Snowflake integration configuration with your admin
4. Check Snowflake query history for authentication errors












