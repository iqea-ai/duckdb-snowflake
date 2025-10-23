# DuckDB Snowflake SAML2 Authentication Setup Guide

Complete guide for setting up browser-based SAML2 authentication with any Identity Provider (IdP).

---

## Prerequisites

- Snowflake account with ACCOUNTADMIN access
- Identity Provider account (Auth0, Okta, Azure AD, etc.)
- Interactive terminal with browser access

---

## Part 1: Find Your Snowflake Account Identifier

**Run this in Snowflake:**
```sql
SELECT CURRENT_ACCOUNT() as account_locator;
```

**Two URL formats exist:**
- **Account Locator**: `qd71618.us-east-2.aws` ← Use this one
- **Account Identifier**: `FLWILYJ-FL43292`

**Use the Account Locator format for SAML2 setup**

---

## Part 2: Configure Your Identity Provider

### Option A: Auth0

1. **Create SAML Application**
   - Go to: Applications > Create Application
   - Type: Regular Web Applications
   - Enable: Addons > SAML2 Web App

2. **Configure SAML2 Addon JSON**
   ```json
   {
     "audience": "https://<your-account-locator>.snowflakecomputing.com",
     "recipient": "https://<your-account-locator>.snowflakecomputing.com/fed/login",
     "destination": "https://<your-account-locator>.snowflakecomputing.com/fed/login",
     "mappings": {
       "email": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress",
       "given_name": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/givenname",
       "family_name": "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/surname"
     },
     "signatureAlgorithm": "rsa-sha256",
     "digestAlgorithm": "sha256",
     "nameIdentifierFormat": "urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress",
     "nameIdentifierProbes": [
       "http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress"
     ],
     "lifetimeInSeconds": 3600,
     "signResponse": false,
     "authnContextClassRef": "urn:oasis:names:tc:SAML:2.0:ac:classes:PasswordProtectedTransport",
     "logout": {
       "callback": "https://<your-account-locator>.snowflakecomputing.com/fed/logout"
     }
   }
   ```

3. **Add Callback URL**
   - Go to: Settings > Allowed Callback URLs
   - Add: `https://<your-account-locator>.snowflakecomputing.com/fed/login`

4. **Get Metadata**
   - Metadata URL: `https://<your-auth0-domain>/samlp/metadata/<client-id>`
   - Open in browser and copy the X509Certificate value

### Option B: Okta

1. **Create SAML Application**
   - Applications > Create App Integration > SAML 2.0

2. **SAML Settings**
   - Single Sign-On URL: `https://<your-account-locator>.snowflakecomputing.com/fed/login`
   - Audience URI: `https://<your-account-locator>.snowflakecomputing.com`
   - Name ID Format: `EmailAddress`
   - Application Username: `Email`

3. **Attribute Statements**
   | Name | Value |
   |------|-------|
   | `http://schemas.xmlsoap.org/ws/2005/05/identity/claims/emailaddress` | `user.email` |
   | `http://schemas.xmlsoap.org/ws/2005/05/identity/claims/givenname` | `user.firstName` |
   | `http://schemas.xmlsoap.org/ws/2005/05/identity/claims/surname` | `user.lastName` |

4. **Assign Users**
   - Assignments tab > Assign users or groups

5. **Get Metadata**
   - Sign On tab > Metadata URL
   - Open and extract: Issuer, SSO URL, X509 Certificate

---

## Part 3: Configure Snowflake

**Run as ACCOUNTADMIN:**

```sql
USE ROLE ACCOUNTADMIN;

-- Create SAML2 Integration
CREATE OR REPLACE SECURITY INTEGRATION saml_integration
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = '<issuer-from-idp-metadata>'
  SAML2_SSO_URL = '<sso-url-from-idp-metadata>'
  SAML2_PROVIDER = 'Okta'
  SAML2_X509_CERT = '<certificate-from-idp-metadata>';

-- Create User
CREATE USER IF NOT EXISTS saml_user
  EMAIL = '<your-email-in-idp>'
  MUST_CHANGE_PASSWORD = FALSE;

-- Grant Permissions
GRANT ROLE PUBLIC TO USER saml_user;
GRANT USAGE ON INTEGRATION saml_integration TO USER saml_user;
GRANT USAGE ON INTEGRATION saml_integration TO ROLE PUBLIC;
```

**Verify:**
```sql
DESC SECURITY INTEGRATION saml_integration;
DESC USER saml_user;
```

---

## Part 4: Configure DuckDB

**Create Secret:**
```sql
LOAD 'build/release/extension/snowflake/snowflake.duckdb_extension';

CREATE SECRET saml_secret (
  TYPE snowflake,
  PROVIDER config,
  ACCOUNT '<account-locator>',        -- e.g., 'qd71618.us-east-2.aws'
  DATABASE '<your-database>',         -- e.g., 'SNOWFLAKE_SAMPLE_DATA'
  WAREHOUSE '<your-warehouse>',       -- e.g., 'COMPUTE_WH'
  AUTH_TYPE 'ext_browser'
);
```

**Connect (opens browser):**
```sql
ATTACH '' AS sf (TYPE snowflake, SECRET saml_secret);
```

**Query (use lowercase!):**
```sql
SELECT COUNT(*) FROM sf.tpch_sf1.customer;
```

**Disconnect:**
```sql
DETACH sf;
```

---

## Important Notes

### Schema/Table Names Must Be Lowercase
```sql
-- Wrong
SELECT * FROM sf.TPCH_SF1.CUSTOMER;

-- Correct
SELECT * FROM sf.tpch_sf1.customer;
```

### Account Format
- Use: `qd71618.us-east-2.aws` (account locator)
- Not: `FLWILYJ-FL43292` (account identifier)

### Browser Required
- Interactive terminal with browser access needed
- Not suitable for headless/containerized environments

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **Callback URL mismatch** | Add Snowflake URL to IdP allowed callbacks |
| **HTTP 404 auth error** | Check account format, verify integration exists |
| **User not found** | Verify email matches between IdP and Snowflake |
| **Table does not exist** | Use lowercase schema/table names |
| **Browser won't open** | Check interactive terminal, configure default browser |
| **Auth timeout** | Complete login faster in browser |

---

## Quick Reference

**Auth0 Metadata URL Format:**
```
https://<domain>.auth0.com/samlp/metadata/<client-id>
```

**Snowflake Account Locator:**
```sql
SELECT CURRENT_ACCOUNT();
```

**Lowercase Query Pattern:**
```sql
SELECT * FROM sf.schema_name.table_name;
```

---

## Testing Checklist

- [ ] IdP SAML application configured
- [ ] Snowflake SAML integration created
- [ ] Snowflake user created with matching email
- [ ] Integration usage granted
- [ ] DuckDB secret created
- [ ] Browser opens during ATTACH
- [ ] Successfully authenticated
- [ ] Can query with lowercase names
- [ ] DETACH works

---

## Security Best Practices

1. Enable MFA in your IdP
2. Use least privilege for Snowflake roles
3. Monitor login history regularly
4. Audit access quarterly
5. Never hardcode credentials

---

## Resources

- [Snowflake SAML Docs](https://docs.snowflake.com/en/user-guide/admin-security-fed-auth-configure-snowflake)
- [Auth0 SAML Docs](https://auth0.com/docs/authenticate/protocols/saml)
- [Okta SAML Docs](https://developer.okta.com/docs/guides/saml-application-setup/)

