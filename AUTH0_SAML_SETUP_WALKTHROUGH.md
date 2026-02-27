# Auth0 SAML Setup Walkthrough

Complete step-by-step guide to set up SAML authentication with Auth0 for DuckDB Snowflake extension.

---

## Prerequisites

- Auth0 account (free tier works)
- Snowflake account with `ACCOUNTADMIN` role access
- DuckDB v1.4.2+ with Snowflake extension installed
- Your Snowflake account identifier (e.g., `myaccount` or `xy12345.us-east-1`)

---

## Step 1: Configure Auth0 SAML Application

### 1.1 Log in to Auth0 Dashboard

1. Go to https://manage.auth0.com
2. Log in with your Auth0 credentials

### 1.2 Create a New Application

1. In the left sidebar, click **Applications**
2. Click **Create Application** button (top right)
3. Fill in the form:
   - **Name**: `Snowflake SAML SSO` (or any name you prefer)
   - **Application Type**: Select **Regular Web Application**
4. Click **Create**

### 1.3 Enable SAML2 Web App Addon

1. You should now be on your application's page
2. Click the **Addons** tab (next to Settings, Advanced Settings, etc.)
3. Find **SAML2 Web App** in the list
4. Toggle it **ON** (click the switch)
5. Click the **Settings** icon (gear) next to SAML2 Web App

### 1.4 Configure SAML Settings (Part 1 - Initial Setup)

In the SAML2 Web App settings, you'll configure this in two parts. First, let's set up what we can:

1. **Application Callback URL**: Leave this blank for now (we'll get it from Snowflake)
2. **Audience**: Enter your Snowflake account URL:
   ```
   https://YOUR_ACCOUNT.snowflakecomputing.com
   ```
   Replace `YOUR_ACCOUNT` with your actual Snowflake account identifier.
   
   Example: If your account is `myaccount`, enter:
   ```
   https://myaccount.snowflakecomputing.com
   ```

3. **Name Identifier Format**: Select:
   ```
   urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress
   ```

4. **Name Identifier**: Enter:
   ```
   {email}
   ```

5. **Settings** (JSON): You can leave this as default for now

6. **Don't click Save yet!** We need to get information from Snowflake first.

### 1.5 Note Your Auth0 Information

Before leaving Auth0, note down:
- **Your Auth0 Domain**: Look at the top of the Auth0 dashboard. It's something like `dev-abc123.us.auth0.com`
- **Your Application's Client ID**: On your application page, under the **Basic Information** tab, you'll see **Client ID** (copy this)

Keep these handy - you'll need them for Snowflake configuration.

---

## Step 2: Configure Snowflake SAML Integration

### 2.1 Log in to Snowflake

1. Log in to your Snowflake account as a user with `ACCOUNTADMIN` role
2. Open a SQL worksheet

### 2.2 Create SAML Security Integration

Run this SQL (replace the placeholders):

```sql
USE ROLE ACCOUNTADMIN;

CREATE OR REPLACE SECURITY INTEGRATION auth0_saml
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'urn:YOUR_AUTH0_DOMAIN'  -- e.g., 'urn:dev-abc123.us.auth0.com'
  SAML2_SSO_URL = 'https://YOUR_AUTH0_DOMAIN/samlp/YOUR_CLIENT_ID'  -- e.g., 'https://dev-abc123.us.auth0.com/samlp/abc123xyz'
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = 'PLACEHOLDER_CERT'  -- We'll update this later
  SAML2_ENABLE_SP_INITIATED = TRUE;

GRANT USAGE ON INTEGRATION auth0_saml TO ROLE PUBLIC;
```

**Replace:**
- `YOUR_AUTH0_DOMAIN` with your Auth0 domain (e.g., `dev-abc123.us.auth0.com`)
- `YOUR_CLIENT_ID` with your Auth0 application's Client ID

**Example:**
If your Auth0 domain is `dev-abc123.us.auth0.com` and Client ID is `xyz789abc`, your SQL would be:

```sql
CREATE OR REPLACE SECURITY INTEGRATION auth0_saml
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'urn:dev-abc123.us.auth0.com'
  SAML2_SSO_URL = 'https://dev-abc123.us.auth0.com/samlp/xyz789abc'
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = 'PLACEHOLDER_CERT'
  SAML2_ENABLE_SP_INITIATED = TRUE;

GRANT USAGE ON INTEGRATION auth0_saml TO ROLE PUBLIC;
```

### 2.3 Get Snowflake SAML Metadata URL

Run this query:

```sql
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('auth0_saml');
```

**Important:** Copy the URL that's returned. It will look something like:
```
https://YOUR_ACCOUNT.snowflakecomputing.com/fed/login?id=abc123...
```

This URL contains the **ACS URL** (Assertion Consumer Service URL) that Auth0 needs.

### 2.4 Extract the ACS URL from Metadata

The easiest way is to:

1. Copy the metadata URL from Step 2.3
2. Open it in a web browser
3. Look for a line that says something like:
   ```xml
   <md:AssertionConsumerService Binding="urn:oasis:names:tc:SAML:2.0:bindings:HTTP-POST" Location="https://YOUR_ACCOUNT.snowflakecomputing.com/fed/login?id=abc123..."/>
   ```
4. Copy the entire `Location` URL value (everything after `Location="` and before `"`)

Alternatively, you can use this simpler approach:
- The ACS URL is typically the metadata URL itself, or very similar
- Try using the metadata URL directly in Auth0 - if it doesn't work, we'll troubleshoot

---

## Step 3: Complete Auth0 Configuration

### 3.1 Go Back to Auth0 SAML Settings

1. Return to Auth0 Dashboard
2. Go to **Applications** → Your application → **Addons** → **SAML2 Web App** → **Settings**

### 3.2 Update Application Callback URL

1. In the **Application Callback URL** field, paste the ACS URL you got from Snowflake metadata (Step 2.4)

   Example:
   ```
   https://myaccount.snowflakecomputing.com/fed/login?id=abc123def456
   ```

### 3.3 Download the Signing Certificate

1. Scroll down in the SAML2 Web App settings
2. Find the **Signing Certificate** section
3. Click **Download Certificate** (or copy the certificate text)
4. **Save this certificate** - you'll need it for Snowflake

The certificate will look like:
```
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKL...
...lots of characters...
...xyzABC123...
-----END CERTIFICATE-----
```

### 3.4 Save Auth0 Settings

1. Click **Save** at the bottom of the SAML2 Web App settings page
2. You should see a success message

### 3.5 Get the SSO URL

1. Still in the SAML2 Web App settings, look for the **Usage** section
2. You'll see an **SSO URL** that looks like:
   ```
   https://dev-abc123.us.auth0.com/samlp/xyz789abc
   ```
3. **Copy this URL** - you'll need it to verify Snowflake configuration

---

## Step 4: Update Snowflake with Auth0 Certificate

### 4.1 Update the Security Integration

Go back to Snowflake and run this SQL (replace with your actual certificate):

```sql
USE ROLE ACCOUNTADMIN;

ALTER SECURITY INTEGRATION auth0_saml SET
  SAML2_X509_CERT = '-----BEGIN CERTIFICATE-----
PASTE_YOUR_CERTIFICATE_HERE
-----END CERTIFICATE-----';
```

**Important:** 
- Paste your entire certificate between the quotes
- Keep the `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----` lines
- Make sure there are no extra spaces or line breaks

**Example:**
```sql
ALTER SECURITY INTEGRATION auth0_saml SET
  SAML2_X509_CERT = '-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKLxyz123abc456
DEF789GHI012JKL345MNO678PQR901STU234
VWX567YZA890BCD123EFG456HIJ789KLM012
-----END CERTIFICATE-----';
```

### 4.2 Verify the Integration

Run this to check your integration:

```sql
DESCRIBE INTEGRATION auth0_saml;
```

You should see:
- `enabled: true`
- `saml2_issuer: urn:dev-abc123.us.auth0.com` (your Auth0 domain)
- `saml2_sso_url: https://dev-abc123.us.auth0.com/samlp/xyz789abc` (your SSO URL)
- `saml2_x509_cert: [your certificate]`

---

## Step 5: Create Snowflake User

### 5.1 Create User Matching Auth0 Email

The user's `LOGIN_NAME` must match the email address that Auth0 will send in the SAML assertion.

```sql
USE ROLE ACCOUNTADMIN;

CREATE USER IF NOT EXISTS "user@example.com"
  LOGIN_NAME = 'user@example.com'  -- Must match the email from Auth0
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'  -- Change to your warehouse name
  DEFAULT_NAMESPACE = 'YOUR_DATABASE.PUBLIC'  -- Change to your database
  MUST_CHANGE_PASSWORD = FALSE;

GRANT ROLE PUBLIC TO USER "user@example.com";
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE YOUR_WAREHOUSE TO ROLE PUBLIC;
```

**Replace:**
- `user@example.com` with the actual email address of the user who will authenticate
- `YOUR_DATABASE` with your Snowflake database name
- `YOUR_WAREHOUSE` with your Snowflake warehouse name (e.g., `COMPUTE_WH`)

### 5.2 Verify User Creation

```sql
SHOW USERS LIKE 'user@example.com';
```

You should see your user listed.

---

## Step 6: Test in DuckDB

### 6.1 Create Secret in DuckDB

Open DuckDB (CLI, Python, or your preferred interface) and run:

```sql
-- Load the Snowflake extension
LOAD snowflake;

-- Create secret with EXT_BROWSER auth type
CREATE SECRET my_auth0_saml_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',  -- e.g., 'myaccount' or 'xy12345.us-east-1'
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    AUTH_TYPE 'ext_browser'  -- This enables SAML authentication
);
```

**Important:** Use your Snowflake account **identifier** (e.g., `myaccount`), NOT the full URL.

### 6.2 Connect (Browser Will Open)

```sql
-- This will open a browser window for SAML authentication
ATTACH '' AS sf (TYPE snowflake, SECRET my_auth0_saml_secret, READ_ONLY);
```

**What happens:**
1. A browser window will open automatically
2. You'll be redirected to Auth0 login page
3. Enter your Auth0 credentials
4. Auth0 will redirect back to Snowflake
5. Snowflake validates the SAML assertion
6. Connection is established!

### 6.3 Verify Connection

```sql
-- List databases (should show 'sf' database)
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf';

-- Query Snowflake data
SELECT * FROM sf.information_schema.tables LIMIT 10;

-- Test a simple query
SELECT COUNT(*) FROM sf.YOUR_SCHEMA.YOUR_TABLE;
```

If these queries work, congratulations! SAML authentication is set up correctly.

---

## Troubleshooting

### Browser Doesn't Open

**Problem:** Browser window doesn't open when running `ATTACH`

**Solutions:**
- Ensure you're running in an environment with GUI/browser access
- Check browser permissions
- On Linux, verify `DISPLAY` environment variable: `echo $DISPLAY`

### Authentication Fails

**Problem:** "Failed to authenticate" or "Invalid credentials"

**Check:**
1. **Snowflake integration enabled:**
   ```sql
   SHOW INTEGRATIONS;
   ```
   Look for `auth0_saml` with `enabled: true`

2. **User exists in Snowflake:**
   ```sql
   SHOW USERS LIKE 'user@example.com';
   ```

3. **User has proper grants:**
   ```sql
   SHOW GRANTS TO USER 'user@example.com';
   ```

4. **IdP configuration matches:**
   - Verify `SAML2_ISSUER` matches Auth0 domain (with `urn:` prefix)
   - Verify `SAML2_SSO_URL` matches Auth0 SSO URL exactly
   - Verify certificate is correct and not expired

5. **Email matches:**
   - The email you use to log into Auth0 must match the `LOGIN_NAME` in Snowflake

### Browser Opens But Redirects Fail

**Problem:** Browser opens but authentication doesn't complete

**Solutions:**
- Verify network connectivity to Auth0 and Snowflake
- Check firewall rules allow redirects
- Verify Auth0 callback URL matches Snowflake ACS URL exactly
- Check browser console for JavaScript errors (F12 → Console)

### Certificate Errors

**Problem:** "Invalid certificate" or certificate validation fails

**Solutions:**
- Ensure certificate includes `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----`
- Check for extra spaces or line breaks in certificate
- Verify certificate hasn't expired
- Download certificate again from Auth0 if needed

### Connection Established But Queries Fail

**Problem:** Connection works but queries return "Insufficient privileges"

**Solutions:**
```sql
-- Grant database usage
GRANT USAGE ON DATABASE YOUR_DATABASE TO ROLE PUBLIC;

-- Grant warehouse usage
GRANT USAGE ON WAREHOUSE YOUR_WAREHOUSE TO ROLE PUBLIC;

-- Grant table access
GRANT SELECT ON ALL TABLES IN SCHEMA YOUR_SCHEMA TO ROLE PUBLIC;
```

---

## Quick Reference

### Auth0 Information Needed:
- Domain: `dev-abc123.us.auth0.com`
- Client ID: `xyz789abc`
- SSO URL: `https://dev-abc123.us.auth0.com/samlp/xyz789abc`
- Signing Certificate: (downloaded from Auth0)

### Snowflake SQL Commands:

```sql
-- Create integration
CREATE SECURITY INTEGRATION auth0_saml
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'urn:YOUR_AUTH0_DOMAIN'
  SAML2_SSO_URL = 'https://YOUR_AUTH0_DOMAIN/samlp/YOUR_CLIENT_ID'
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = 'YOUR_CERTIFICATE'
  SAML2_ENABLE_SP_INITIATED = TRUE;

-- Get metadata
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('auth0_saml');

-- Create user
CREATE USER IF NOT EXISTS "user@example.com"
  LOGIN_NAME = 'user@example.com'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  MUST_CHANGE_PASSWORD = FALSE;
```

### DuckDB Usage:

```sql
CREATE SECRET my_saml_secret (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT',
    DATABASE 'YOUR_DATABASE',
    WAREHOUSE 'YOUR_WAREHOUSE',
    AUTH_TYPE 'ext_browser'
);

ATTACH '' AS sf (TYPE snowflake, SECRET my_saml_secret, READ_ONLY);
```

---

## Next Steps

Once SAML is working:
1. Test with multiple users
2. Set up proper roles and permissions in Snowflake
3. Consider setting up OAuth for programmatic access (see `setup_oauth_saml.md`)
4. Document your specific configuration for your team

---

## Getting Help

If you encounter issues:
1. Check Snowflake query history for authentication errors
2. Review Auth0 logs (Dashboard → Monitoring → Logs)
3. Verify all URLs and certificates are correct
4. Test SAML flow manually in a browser first

For more details, see:
- `setup_oauth_saml.md` - Complete OAuth and SAML guide
- `SSO_TESTING_GUIDE.md` - Testing procedures
- `docs/AUTHENTICATION.md` - Authentication reference










