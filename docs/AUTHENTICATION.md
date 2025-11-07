# Authentication Methods

> **Version requirement:** The authentication methods described here (OAuth, key pair, Okta, EXT_BROWSER, MFA) require the Snowflake extension build for DuckDB **v1.4.1** or newer. Earlier releases do not register the necessary secret parameters. Download the v1.4.1 artifacts from the [`OAuth_KeyPair_SAML` GitHub Actions run](https://github.com/iqea-ai/duckdb-snowflake/actions/runs/18735204247) before following these steps.

The DuckDB Snowflake extension supports multiple authentication methods to connect to Snowflake. Choose the method that best fits your security requirements and infrastructure.

**Quick Start**: For step-by-step setup instructions for OAuth, Key Pair, Okta, and MFA, see [AUTHENTICATION_SETUP.md](AUTHENTICATION_SETUP.md)

## Authentication Protocol Overview

| Method | Protocol | Browser Required | Use Case | Status |
|--------|----------|------------------|----------|--------|
| **Password** | Username/Password | No | Development, simple setups | Tested |
| **OAuth** | OAuth 2.0 | No | Programmatic access, APIs, automation, **recommended for Okta** | Known Issues |
| **Key Pair** | RSA Key Pair | No | Production, headless servers, highest security | Tested |
| **EXT_BROWSER** | SAML 2.0 | Yes | Interactive SSO, any SAML 2.0 IdP (Okta, AD FS) | Tested |
| **OKTA** | Native Okta API | Yes | Okta-specific native integration (Okta IdP only) | Implemented |
| **MFA** | Password + MFA | No | Enhanced password security (interactive only) | Not Suitable for Programmatic Use |

**Important Notes:**
- **For Okta users**: OAuth is recommended over EXT_BROWSER/SAML for better security, flexibility, and programmatic access
- **EXT_BROWSER uses SAML 2.0**: Works with any SAML 2.0-compliant identity provider including Okta, AD FS, Azure AD
- **OAuth uses OAuth 2.0**: Ideal for headless environments, automation, and modern applications
- **Browser-based methods** (EXT_BROWSER, OKTA) require interactive browser access and won't work in containerized/headless environments

**Testing Status:**
- **Tested**: PASSWORD, EXT_BROWSER, and KEY_PAIR have been fully tested and verified working
- **Not Suitable for Programmatic Use**: 
  - **MFA**: Works for interactive browser sessions but not for programmatic connections (DuckDB, JDBC, ODBC). Error: "MFA methods are not supported for programmatic authentication." For secure programmatic access, use Key Pair instead.
- **Known Issues**: 
  - **OAuth**: Fully configured (Auth0 + Snowflake integration working with SnowSQL) but ADBC driver token validation fails. Tokens work correctly with SnowSQL but not with DuckDB/ADBC. Investigation needed.
- **Implemented**: 
  - **OKTA**: Native Okta API requires Okta as IdP (not compatible with Auth0). For Auth0 users, use EXT_BROWSER instead.

## 1. Password Authentication (Default)

Standard username and password authentication.

```sql
CREATE SECRET my_snowflake_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    PASSWORD 'mypassword',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;password=mypassword;database=mydb;warehouse=mywh
```

## 2. OAuth Token Authentication

Use **OAuth 2.0** tokens for authentication. **Recommended for Auth0/Okta environments** and ideal for programmatic access.

**Protocol**: OAuth 2.0

**Known Issue**: OAuth authentication is fully implemented and configured correctly (tokens work with SnowSQL), but the ADBC Snowflake driver currently fails to validate tokens when used through DuckDB. This is under investigation. For production use with Auth0/Okta, use **Key Pair** or **EXT_BROWSER** authentication instead.

**Advantages**:
- No browser required (works in headless/containerized environments)
- Supports programmatic authentication for APIs and automation
- Better security with configurable authorization flows
- Token-based access control with granular permissions
- Recommended by Snowflake for modern IdP integration

### DuckDB Usage

```sql
CREATE SECRET my_oauth_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'your_user_or_client_id@clients',
    AUTH_TYPE 'oauth',
    TOKEN 'your_oauth_access_token',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);

-- Use the secret
ATTACH '' AS sf (TYPE snowflake, SECRET my_oauth_secret, READ_ONLY);
SELECT * FROM sf.schema.table LIMIT 10;
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=oauth;token=your_oauth_access_token;database=mydb;warehouse=mywh
```

### Setup Required in Snowflake

Before using OAuth authentication, you must configure External OAuth in Snowflake:

**Step 1: Create Security Integration in Snowflake**

```sql
USE ROLE ACCOUNTADMIN;

CREATE SECURITY INTEGRATION my_oauth_integration
  TYPE = EXTERNAL_OAUTH
  ENABLED = TRUE
  EXTERNAL_OAUTH_TYPE = CUSTOM
  EXTERNAL_OAUTH_ISSUER = 'https://YOUR_IDP_DOMAIN'  -- e.g., 'https://dev-abc123.us.auth0.com/' for Auth0
  EXTERNAL_OAUTH_JWS_KEYS_URL = 'https://YOUR_IDP_DOMAIN/.well-known/jwks.json'
  EXTERNAL_OAUTH_AUDIENCE_LIST = ('https://YOUR_ACCOUNT.snowflakecomputing.com')
  EXTERNAL_OAUTH_TOKEN_USER_MAPPING_CLAIM = 'sub'  -- or 'email' depending on your token
  EXTERNAL_OAUTH_SNOWFLAKE_USER_MAPPING_ATTRIBUTE = 'LOGIN_NAME'
  EXTERNAL_OAUTH_ANY_ROLE_MODE = 'ENABLE';

GRANT USAGE ON INTEGRATION my_oauth_integration TO ROLE PUBLIC;
```

**Step 2: Create Snowflake User Matching Token Claim**

```sql
-- The username must match the claim in your OAuth token (e.g., 'sub' claim)
CREATE USER IF NOT EXISTS "your_token_sub_claim"
  LOGIN_NAME = 'your_token_sub_claim'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH';

GRANT ROLE PUBLIC TO USER "your_token_sub_claim";
GRANT IMPORTED PRIVILEGES ON DATABASE your_database TO ROLE PUBLIC;
```

### Setup for Auth0

**1. Configure Auth0 API:**
   - Go to Auth0 Dashboard → APIs → Create API
   - **Name**: `Snowflake OAuth`
   - **Identifier**: `https://YOUR_ACCOUNT.snowflakecomputing.com`
   - Click Create

**2. Create Machine-to-Machine Application:**
   - Go to Applications → Create Application
   - Choose "Machine to Machine Applications"
   - Authorize it to use your Snowflake API
   - Note down **Client ID**, **Client Secret**, and **Domain**

**3. Get OAuth Token:**

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

The response will contain an `access_token` - use this as your `TOKEN` in the CREATE SECRET statement.

### Setup for Okta

**1. Create OAuth Integration in Snowflake:**
   - Use the same Snowflake configuration above
   - Set `EXTERNAL_OAUTH_ISSUER` to your Okta issuer URL
   - Set `EXTERNAL_OAUTH_JWS_KEYS_URL` to your Okta JWKS URL

**2. Configure Okta:**
   - Create an OAuth 2.0 application in Okta
   - Configure client credentials grant type
   - Set audience to match your Snowflake account URL
   - Use Okta's token endpoint to obtain access tokens

**Note**: For both Auth0 and Okta, ensure the token's `sub` or `email` claim matches the Snowflake user's `LOGIN_NAME`.

## 3. Key Pair Authentication (Tested)

Use RSA key pairs for secure, password-less authentication. This is the recommended method for production environments and programmatic access.

**Basic Key Pair (Unencrypted Private Key):**

```sql
CREATE SECRET my_keypair_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/path/to/rsa_key.p8',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

> You can supply either a filesystem path (recommended) or the PEM contents of the PKCS#8 private key in `PRIVATE_KEY`. If you omit the value entirely, the Snowflake driver will fail with: `trying to use keypair authentication, but PrivateKey was not provided in the driver config`.

**Connection String:**
```
account=myaccount;user=myusername;auth_type=key_pair;private_key=/path/to/rsa_key.p8;database=mydb;warehouse=mywh
```

**Key Pair with Encrypted Private Key:**

For enhanced security, use a passphrase-protected private key (PKCS#8 format).

```sql
CREATE SECRET my_secure_keypair_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/path/to/encrypted_rsa_key.p8',
    PRIVATE_KEY_PASSPHRASE 'your_passphrase',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=key_pair;private_key=/path/to/encrypted_rsa_key.p8;private_key_passphrase=your_passphrase;database=mydb;warehouse=mywh
```

**Generating Key Pairs:**

```bash
# Generate private key with passphrase
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out rsa_key.p8 -passout pass:your_passphrase

# Generate public key
openssl rsa -in rsa_key.p8 -pubout -out rsa_key.pub -passin pass:your_passphrase

# Assign public key to Snowflake user
# In Snowflake, run:
# ALTER USER myusername SET RSA_PUBLIC_KEY='<public_key_content>';
```

## 4. External Browser SSO Authentication

Authenticate using your web browser with **SAML 2.0** identity providers (Auth0, Okta, AD FS, Azure AD, etc.). This opens a browser window for SSO login.

**Protocol**: SAML 2.0 (Security Assertion Markup Language)

**How it works**:
1. Client initiates authentication
2. Browser window opens to your IdP login page
3. User authenticates with IdP (e.g., Auth0/Okta)
4. IdP sends SAML assertion to Snowflake
5. Snowflake validates assertion and creates session

### DuckDB Usage

```sql
CREATE SECRET my_sso_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse',
    AUTH_TYPE 'ext_browser'
);

-- Use the secret (browser will open for authentication)
ATTACH '' AS sf (TYPE snowflake, SECRET my_sso_secret, READ_ONLY);
SELECT * FROM sf.schema.table LIMIT 10;
```

**Connection String:**
```
account=myaccount;auth_type=ext_browser;database=mydb;warehouse=mywh
```

**Note:** External browser authentication is also supported with the alias `externalbrowser`:
```
account=myaccount;auth_type=externalbrowser;database=mydb;warehouse=mywh
```

### Setup Required in Snowflake

**For SAML 2.0 SSO with Auth0:**

**Step 1: Configure SAML Integration in Snowflake**

```sql
USE ROLE ACCOUNTADMIN;

CREATE SECURITY INTEGRATION auth0_saml
  TYPE = SAML2
  ENABLED = TRUE
  SAML2_ISSUER = 'urn:YOUR_AUTH0_DOMAIN'  -- e.g., 'urn:dev-abc123.us.auth0.com'
  SAML2_SSO_URL = 'https://YOUR_AUTH0_DOMAIN/samlp/YOUR_CLIENT_ID'
  SAML2_PROVIDER = 'CUSTOM'
  SAML2_X509_CERT = 'YOUR_AUTH0_SIGNING_CERTIFICATE'  -- Download from Auth0
  SAML2_ENABLE_SP_INITIATED = TRUE;

GRANT USAGE ON INTEGRATION auth0_saml TO ROLE PUBLIC;
```

**Step 2: Get Snowflake SAML Metadata**

```sql
-- Get the metadata URL
SELECT SYSTEM$SHOW_SAML_IDP_METADATA('auth0_saml');
```

**Step 3: Configure Auth0:**
   1. Go to Auth0 Dashboard → Applications → Create Application
   2. Choose "Regular Web Application"
   3. Go to "Addons" tab → Enable "SAML2 Web App"
   4. Configure SAML settings:
      - **Application Callback URL**: Use Snowflake's ACS URL from metadata
      - **Audience**: `https://YOUR_ACCOUNT.snowflakecomputing.com`
      - **Name Identifier Format**: `urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress`
   5. Download the signing certificate and add it to the Snowflake integration

**For SAML 2.0 SSO with Okta:**

**Step 1: Configure Okta Application**
   1. Okta Admin Console → Applications → Create App Integration
   2. Choose "SAML 2.0"
   3. Configure Single sign-on URL and Audience URI from Snowflake metadata
   4. Complete attribute mapping

**Step 2: Configure Snowflake**
   - Use the same `CREATE SECURITY INTEGRATION` as above
   - Set `SAML2_ISSUER` to your Okta issuer URI
   - Set `SAML2_SSO_URL` to your Okta SSO URL
   - Add Okta's X.509 certificate

**Limitations**:
- Requires interactive browser access (not suitable for headless/containerized environments)
- User must be present to authenticate
- For programmatic access, consider using OAuth instead

## 5. Native Okta Authentication

Direct authentication with Okta using a custom Okta URL.

**Important**: This authentication method only works if your organization uses **Okta** as the identity provider. It is **not compatible with Auth0**. If you are using Auth0, use **EXT_BROWSER** (SAML 2.0) authentication instead.

```sql
CREATE SECRET my_okta_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    AUTH_TYPE 'okta',
    OKTA_URL 'https://yourcompany.okta.com',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;auth_type=okta;okta_url=https://yourcompany.okta.com;database=mydb;warehouse=mywh
```

## 6. Multi-Factor Authentication (MFA)

Authenticate using username, password, and MFA token. Provides enhanced security for password-based authentication.

**Important Limitation**: MFA with TOTP authenticators (Google Authenticator, Duo Mobile, iCloud Keychain, etc.) is designed for **interactive browser sessions only** and does not work with programmatic connections like DuckDB, JDBC, ODBC, or Python connectors. Snowflake returns error: "MFA methods are not supported for programmatic authentication."

**For programmatic access with high security, use Key Pair authentication instead**, which provides even stronger security than MFA and works seamlessly with DuckDB.

```sql
CREATE SECRET my_mfa_secret (
    TYPE snowflake,
    ACCOUNT 'myaccount.snowflakecomputing.com',
    USER 'myusername',
    PASSWORD 'mypassword',
    AUTH_TYPE 'mfa',
    DATABASE 'mydatabase',
    WAREHOUSE 'mywarehouse'
);
```

**Connection String:**
```
account=myaccount;user=myusername;password=mypassword;auth_type=mfa;database=mydb;warehouse=mywh
```
