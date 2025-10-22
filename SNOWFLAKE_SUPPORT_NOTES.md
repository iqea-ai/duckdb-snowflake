# Snowflake Support Meeting Notes - SAML2 Error with External OAuth

## Issue Summary

**Problem:** Getting SAML2 error (390190) when attempting to use External OAuth integration with ADBC driver's `auth_ext_browser` authentication type.

**Error Message:**
```
[Snowflake] 390190 (08004): There was an error related to the SAML Identity 
Provider account parameter. Contact Snowflake support.
```

**Expected Behavior:** According to Snowflake support, External OAuth should NOT trigger SAML2 errors. Browser should open for SSO authentication via Auth0.

**Actual Behavior:** 
- Browser does NOT open
- SAML2 error occurs immediately
- Connection fails

---

## Environment Details

**Snowflake Account:**
- Account Identifier: `FLWILYJ-FL43292`
- Account URL: `https://FLWILYJ-FL43292.snowflakecomputing.com`
- User: `iqeadev`
- Region: (from account identifier)

**ADBC Driver:**
- Version: 1.8.0 (Apache Arrow ADBC Snowflake Driver)
- File: `libadbc_driver_snowflake.so` (Mach-O 64-bit arm64)
- Date: September 9, 2024

**DuckDB Extension:**
- DuckDB Version: 1.4.0
- Extension: Custom Snowflake extension using ADBC driver
- Platform: macOS ARM64 (Apple Silicon)

---

## Snowflake Configuration

### Security Integration (EXTERNAL_OAUTH)

**Integration Name:** `AUTH0_EXTERNAL_BROWSER`

**Configuration:**
```sql
CREATE SECURITY INTEGRATION auth0_external_browser
  TYPE = EXTERNAL_OAUTH
  ENABLED = TRUE
  EXTERNAL_OAUTH_TYPE = CUSTOM
  EXTERNAL_OAUTH_ISSUER = 'https://dev-6xacnvcyhxwr6dzk.us.auth0.com'
  EXTERNAL_OAUTH_JWS_KEYS_URL = 'https://dev-6xacnvcyhxwr6dzk.us.auth0.com/.well-known/jwks.json'
  EXTERNAL_OAUTH_AUDIENCE_LIST = ('https://FLWILYJ-FL43292.snowflakecomputing.com')
  EXTERNAL_OAUTH_TOKEN_USER_MAPPING_CLAIM = 'email'
  EXTERNAL_OAUTH_SNOWFLAKE_USER_MAPPING_ATTRIBUTE = 'LOGIN_NAME'
  EXTERNAL_OAUTH_SCOPE_DELIMITER = ' ';
```

**Verified Settings:**
- ENABLED: `true`
- EXTERNAL_OAUTH_TYPE: `CUSTOM`
- EXTERNAL_OAUTH_ISSUER: `https://dev-6xacnvcyhxwr6dzk.us.auth0.com`
- EXTERNAL_OAUTH_AUDIENCE_LIST: `https://FLWILYJ-FL43292.snowflakecomputing.com`
- EXTERNAL_OAUTH_TOKEN_USER_MAPPING_CLAIM: `email`
- EXTERNAL_OAUTH_SNOWFLAKE_USER_MAPPING_ATTRIBUTE: `LOGIN_NAME`

**Integration Grants:**
```
USAGE | INTEGRATION | AUTH0_EXTERNAL_BROWSER | ROLE | ACCOUNTADMIN
USAGE | INTEGRATION | AUTH0_EXTERNAL_BROWSER | ROLE | DATA_USER
```

### User Configuration

**User:** `iqeadev`
- LOGIN_NAME: Should be `venkata@iqea.ai` (for OAuth email mapping)
- EMAIL: `venkata@iqea.ai`
- ROLE: `DATA_USER`

### Disabled Integrations

**Disabled:** `EXTERNAL_OAUTH_OKTA_2` (old Okta trial integration)
- Status: ENABLED = false
- Reason: Potentially conflicting with new Auth0 integration

---

## Auth0 Configuration

**Auth0 Tenant:** `dev-6xacnvcyhxwr6dzk.us.auth0.com`

**Application:**
- Name: Snowflake DuckDB Extension
- Type: Regular Web Application
- Client ID: `ZWo7a2VOLUD9NwT9FkoFa35Y0wLbB0RP`
- Client Secret: `vY022LDIZreG6bcHoPSL9HwNsozqlGpH9owo2CeJCt_uwNCaar4I12bYccWemW_G`

**Callback URLs:**
```
http://localhost:8080/callback
https://FLWILYJ-FL43292.snowflakecomputing.com/oauth/token-request-callback
```

**Post-Login Action:**
- Action: Add Email to ID Token
- Trigger: post-login
- Status: Deployed
- Code: Adds `email` claim to ID token

**Auth0 User:**
- Email: `venkata@iqea.ai`
- Connection: Username-Password-Authentication

---

## ADBC Parameters Used

Per Snowflake ADBC driver documentation, we're setting:

```cpp
// Account
AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.account", "FLWILYJ-FL43292", &error);

// Authentication type (per ADBC docs for external browser)
AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.auth_type", "auth_ext_browser", &error);

// Username
AdbcDatabaseSetOption(&database, "username", "iqeadev", &error);

// Optional timeouts
AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.client_option.login_timeout", "120s", &error);
AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.client_option.external_browser_timeout", "120s", &error);
```

---

## What We've Tried

### Attempt 1: Used `authenticator = OAUTH` (Original Code)
**Result:** SAML2 error 390190

### Attempt 2: Used `auth_type = auth_ext_browser`
**Result:** SAML2 error 390190

### Attempt 3: Used `authenticator = EXTERNALBROWSER` (uppercase)
**Result:** "password is empty" error

### Attempt 4: Used `authenticator = externalbrowser` (lowercase)
**Result:** "password is empty" error

### Attempt 5: Set username BEFORE authenticator
**Result:** "password is empty" error (driver thinks it's password auth)

### Attempt 6: Set username AFTER authenticator
**Result:** "user is empty" error

### Attempt 7: Set auth_type=auth_ext_browser + username (current)
**Result:** SAML2 error 390190 (same as original)

### Attempt 8: Disabled old OKTA integration
**Result:** SAML2 error 390190 (no change)

**Observation:** Browser NEVER opens in any configuration, suggesting the ADBC driver is not initiating the browser OAuth flow.

---

## Working Configuration (Password Auth)

**For comparison, password authentication works perfectly:**

```cpp
AdbcDatabaseSetOption(&database, "adbc.snowflake.sql.account", "FLWILYJ-FL43292", &error);
AdbcDatabaseSetOption(&database, "username", "iqeadev", &error);
AdbcDatabaseSetOption(&database, "password", "***", &error);
```

**Result:** Successful connection, queries execute correctly

---

## Discovery: ADBC Authentication Types

**Per ADBC Snowflake driver documentation:**
```
auth_ext_browser: Use an external browser to access a FED and perform SSO auth.
auth_oauth: Use OAuth authentication for the snowflake connection.
```

**Key Finding:** `auth_ext_browser` is for **FED (Federated Authentication)** only, NOT for custom EXTERNAL_OAUTH integrations!

**Correct Approach:**
- For custom EXTERNAL_OAUTH (Auth0): Use `auth_oauth` with pre-obtained token
- For Snowflake-managed FED: Use `auth_ext_browser` (browser opens automatically)

## Updated Implementation Plan

We've now implemented `auth_oauth` support:
- ✅ Enabled OAUTH case in authentication switch
- ✅ Added `oauth_token` parameter to secret provider
- ✅ Uses `auth_oauth` authenticator type per ADBC docs
- ✅ Validates OAuth token is provided
- ⚠️ Requires token to be obtained manually (browser flow not yet implemented)

## Questions for Snowflake Support

1. **Can you confirm our understanding is correct?**
   - `auth_ext_browser` = Snowflake-managed FED only (not custom OAuth)
   - `auth_oauth` = Custom EXTERNAL_OAUTH with pre-obtained token

2. **For `auth_oauth` with custom EXTERNAL_OAUTH:**
   - Is `adbc.snowflake.sql.auth_token` the correct parameter for the token?
   - Any other required parameters?
   - Does the token need to be the access_token or id_token from OAuth flow?

3. **Does ADBC support automatic token acquisition for custom OAuth?**
   - Or must applications always obtain tokens themselves?
   - Any plans to support browser-based token acquisition for custom OAuth?

4. **Best practices for programmatic OAuth with Snowflake:**
   - PKCE flow recommended?
   - Token refresh handling?
   - Session management?

---

## Additional Context

**What Works:**
- Snowflake account is accessible (web UI login works)
- Password authentication via ADBC works perfectly
- Auth0 integration is fully configured
- Snowflake EXTERNAL_OAUTH integration created and enabled
- User mapping is configured correctly

**What Doesn't Work:**
- Browser-based OAuth flow via ADBC
- Any variation of external browser authentication
- Custom EXTERNAL_OAUTH integration recognition

**Hypothesis:**
The ADBC driver's `auth_ext_browser` may be designed for Snowflake's native federated authentication (account-level Okta/Azure AD managed by Snowflake) rather than custom EXTERNAL_OAUTH security integrations.

---

## Reproduction Steps

1. Create EXTERNAL_OAUTH security integration in Snowflake (custom Auth0)
2. Configure Auth0 application with Snowflake as audience
3. Use ADBC driver with:
   - `adbc.snowflake.sql.account` = account identifier
   - `adbc.snowflake.sql.auth_type` = `auth_ext_browser`
   - `username` = Snowflake username
4. Initialize connection

**Expected:** Browser opens for OAuth flow
**Actual:** SAML2 error 390190 immediately

---

## Request

Please advise on the correct way to use ADBC driver with custom EXTERNAL_OAUTH integrations (Auth0/generic OAuth providers), or confirm if this is not currently supported and recommend an alternative approach.

GitHub Branch: `auth_ext_browser`
- Includes full implementation and test cases
- CI will run on push to validate build across platforms

---

## Contact Information

Developer: Venkata Chikkam
Email: venkata@iqea.ai
Account: FLWILYJ-FL43292
Project: DuckDB Snowflake Extension (Open Source)

