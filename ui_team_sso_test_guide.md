# UI Team SSO Login Guide

## ✅ Setup Complete!

The following has been configured:
- ✅ Auth0 SAML integration (`auth0_saml`) is enabled
- ✅ Integration is granted to `UI_READONLY_ROLE` and `PUBLIC`
- ✅ Users configured for SAML authentication:
  - sabida@narveetech.com
  - anand@narveetech.com
  - vamshi@narveetech.com
- ✅ All users have `UI_READONLY_ROLE` assigned
- ✅ `LOGIN_NAME` matches Auth0 SAML assertion format

## Prerequisites for UI Team

1. **Users must exist in Auth0** (or be able to authenticate via Auth0 connections like Google OAuth)
2. **DuckDB installed** with Snowflake extension
3. **ADBC driver installed** (if not using community extension)

## How to Test SSO Login

### Step 1: Load Snowflake Extension in DuckDB

```sql
-- Install and load extension (if not already installed)
INSTALL snowflake FROM community;
LOAD snowflake;
```

### Step 2: Create Secret with SAML Authentication

```sql
CREATE SECRET my_snowflake_saml (
    TYPE snowflake,
    ACCOUNT 'YFEQLOK-BD05926',  -- Your Snowflake account identifier
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',  -- Adjust if needed
    WAREHOUSE 'COMPUTE_WH',  -- Adjust if needed
    AUTH_TYPE 'ext_browser'  -- This enables SAML SSO
);
```

### Step 3: Connect (Browser Will Open)

```sql
-- This will open a browser window for SAML authentication
ATTACH '' AS sf (TYPE snowflake, SECRET my_snowflake_saml, READ_ONLY);
```

**What happens:**
1. Browser window opens automatically
2. Redirects to Auth0 login page
3. User authenticates (via Google OAuth or Auth0 credentials)
4. Redirects back to Snowflake
5. Connection established!

### Step 4: Verify Connection

```sql
-- Check connection
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf';

-- List schemas
SELECT schema_name FROM sf.information_schema.schemata LIMIT 10;

-- Test query
SELECT COUNT(*) FROM sf.information_schema.tables;
```

## Expected Behavior

### ✅ Success Scenario:
- Browser opens automatically
- User logs in via Auth0
- Redirects to Snowflake
- Connection succeeds
- User can query Snowflake data

### ❌ If Authentication Fails:

**Check 1: LOGIN_NAME mismatch**
```sql
-- Verify user LOGIN_NAME matches Auth0 assertion
SHOW USERS LIKE '%@narveetech.com';
-- LOGIN_NAME should be lowercase: sabida@narveetech.com
```

**Check 2: User exists in Auth0**
- Verify user can log into Auth0 dashboard
- Check Auth0 logs (Dashboard → Monitoring → Logs)

**Check 3: Integration is enabled**
```sql
DESCRIBE INTEGRATION auth0_saml;
-- Should show ENABLED = true
```

**Check 4: User has correct role**
```sql
SHOW GRANTS TO USER "sabida@narveetech.com";
-- Should show UI_READONLY_ROLE
```

## Troubleshooting

### Error: "User not found" or "Invalid credentials"
- **Solution**: Verify `LOGIN_NAME` in Snowflake matches exactly what Auth0 sends (usually lowercase email)
- Check Auth0 SAML response to see what NameID is being sent

### Error: "Integration not accessible"
- **Solution**: Verify integration grants:
```sql
SHOW GRANTS ON INTEGRATION auth0_saml;
-- Should show UI_READONLY_ROLE has USAGE privilege
```

### Browser doesn't open
- **Solution**: Make sure you're using `AUTH_TYPE 'ext_browser'` in the secret
- Check DuckDB version supports SAML (v1.4.2+)

### Auth0 login succeeds but Snowflake connection fails
- **Solution**: Check Snowflake query history for detailed error messages
- Verify SAML integration is enabled: `DESCRIBE INTEGRATION auth0_saml;`
- Check that user's `LOGIN_NAME` matches Auth0 assertion exactly

## Quick Test Script

Save this as `test_sso.sql` and run in DuckDB:

```sql
-- Load extension
INSTALL snowflake FROM community;
LOAD snowflake;

-- Create SAML secret
CREATE SECRET my_saml_test (
    TYPE snowflake,
    ACCOUNT 'YFEQLOK-BD05926',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH',
    AUTH_TYPE 'ext_browser'
);

-- Connect (browser will open)
ATTACH '' AS sf (TYPE snowflake, SECRET my_saml_test, READ_ONLY);

-- Verify connection
SELECT 'Connection successful!' AS status;
SELECT database_name FROM duckdb_databases() WHERE database_name = 'sf';
SELECT COUNT(*) AS table_count FROM sf.information_schema.tables;
```

## Summary

✅ **Yes, the UI team should be able to login with SSO!**

The setup is complete. Users just need to:
1. Use DuckDB with Snowflake extension
2. Create a secret with `AUTH_TYPE 'ext_browser'`
3. Run `ATTACH` command
4. Authenticate via browser when prompted

If they encounter any issues, refer to the troubleshooting section above.










