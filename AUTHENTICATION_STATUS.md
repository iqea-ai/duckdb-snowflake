# Authentication Methods Testing Status

Last Updated: October 27, 2025

## Summary

| Method | Status | Production Ready | Notes |
|--------|--------|------------------|-------|
| **Password** | Tested | Development Only | Simple setup, not recommended for production |
| **Key Pair** | Tested | **Yes - Recommended** | Best for production, programmatic access, highest security |
| **EXT_BROWSER** | Tested | Yes (Interactive) | SAML 2.0 SSO, works with Auth0, Okta, AD FS, Azure AD |
| **MFA** | Tested | Yes | Enhanced password security |
| **OAuth** | Known Issues | No | ADBC driver token validation fails (use Key Pair instead) |
| **Okta Native** | Not Applicable | N/A | Requires Okta IdP (we use Auth0) |

## Tested and Working (4/6)

### 1. Password Authentication
- Status: Fully Tested
- Use Case: Development, testing
- Configuration: Simple username/password
- Production: Not recommended

### 2. Key Pair Authentication  
- Status: Fully Tested
- Use Case: **Production, automation, CI/CD**
- Configuration: RSA key pair with PKCS#8 encrypted private key
- Production: **Recommended - Highest Security**
- Test Command:
```bash
ATTACH 'account=FLWILYJ-FL43292;user=snowflake_keypair_user;auth_type=key_pair;private_key=/path/to/rsa_key.p8;private_key_passphrase=your_passphrase;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' AS sf (TYPE snowflake, READ_ONLY);
```

### 3. External Browser (SAML 2.0)
- Status: Fully Tested
- Use Case: Interactive SSO with Auth0, Okta, AD FS, Azure AD
- Configuration: SAML 2.0 integration
- Production: Yes (requires browser)
- Test Command:
```bash
ATTACH 'account=FLWILYJ-FL43292;auth_type=ext_browser;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' AS sf (TYPE snowflake, READ_ONLY);
```

### 4. Multi-Factor Authentication (MFA)
- Status: Fully Tested
- Use Case: Enhanced password security
- Configuration: MFA enrollment required
- Production: Yes (requires user interaction)
- Test Command:
```bash
ATTACH 'account=FLWILYJ-FL43292;user=mfa_test_user;password=your_password;auth_type=mfa;database=SNOWFLAKE_SAMPLE_DATA;warehouse=COMPUTE_WH' AS sf (TYPE snowflake, READ_ONLY);
```

## Known Issues (1/6)

### OAuth 2.0 Authentication
- Status: Known Issues
- Issue: ADBC Snowflake driver fails to validate OAuth tokens despite correct configuration
- Evidence:
  - Auth0 OAuth integration fully configured
  - Snowflake External OAuth security integration configured
  - Tokens work perfectly with SnowSQL
  - Same tokens fail with DuckDB/ADBC driver (error: "Invalid OAuth access token")
  - Token is passed correctly to ADBC (debug shows length 855)
- Configuration Completed:
  - Auth0 API with audience: https://FLWILYJ-FL43292.snowflakecomputing.com
  - Auth0 M2M application authorized with permissions
  - Snowflake integration AUTH0_OAUTH with scope mapping to 'permissions'
  - Snowflake user created matching token sub claim
- Root Cause: Likely ADBC driver OAuth implementation issue
- Workaround: Use **Key Pair** authentication for production programmatic access

## Not Applicable (1/6)

### Okta Native API
- Status: Not Applicable
- Reason: Requires Okta as identity provider
- Our Environment: Auth0 (not Okta)
- Alternative: Use **EXT_BROWSER** (SAML 2.0) which works with Auth0

## Recommendations by Use Case

| Use Case | Recommended Method | Alternative |
|----------|-------------------|-------------|
| **Production APIs** | Key Pair | - |
| **CI/CD Pipelines** | Key Pair | - |
| **Automation/Scripts** | Key Pair | - |
| **Interactive Users (Auth0)** | EXT_BROWSER | MFA |
| **Interactive Users (Okta)** | EXT_BROWSER | Okta Native |
| **Development/Testing** | Password | MFA |

## Environment Variables for Testing

```bash
# Common
export SNOWFLAKE_ACCOUNT="FLWILYJ-FL43292"
export SNOWFLAKE_DATABASE="SNOWFLAKE_SAMPLE_DATA"

# Password (Tested)
export SNOWFLAKE_USERNAME="your_username"
export SNOWFLAKE_PASSWORD="your_password"

# Key Pair (Tested - Recommended)
export SNOWFLAKE_KEYPAIR_USER="snowflake_keypair_user"
export SNOWFLAKE_PRIVATE_KEY_PATH="/Users/venkata/ddbsf-ext/.snowflake_keys/rsa_key.p8"
export SNOWFLAKE_PRIVATE_KEY_PASSPHRASE="SnowflakeKeyPair2024"

# MFA (Tested)
export SNOWFLAKE_MFA_USER="mfa_test_user"
export SNOWFLAKE_MFA_PASSWORD="SecurePassword123!"

# OAuth (Known Issues - Skip for now)
# export SNOWFLAKE_OAUTH_USER="..."
# export SNOWFLAKE_OAUTH_TOKEN="..."

# Okta Native (Not Applicable - Auth0 environment)
# Skip this test
```

## Test Results

### Successful Tests
- Password authentication with username/password
- Key pair authentication with encrypted RSA keys (PKCS#8)
- External browser SSO with Auth0 (SAML 2.0)
- Multi-factor authentication with password + MFA token

### Failed Tests
- OAuth 2.0 with Auth0 bearer tokens (ADBC driver issue)

### Skipped Tests
- Okta Native API (requires Okta IdP, not Auth0)

## Next Steps

1. **Production Deployment**: Use **Key Pair** authentication
2. **OAuth Issue**: File bug report with ADBC Snowflake driver project
3. **Testing**: Run comprehensive test suite with working auth methods
4. **Documentation**: Consider adding troubleshooting guide for OAuth issues

## Files Generated

- Private Key: `/Users/venkata/ddbsf-ext/.snowflake_keys/rsa_key.p8` (encrypted)
- Public Key: `/Users/venkata/ddbsf-ext/.snowflake_keys/rsa_key.pub`
- Public Key (one-line): `/Users/venkata/ddbsf-ext/.snowflake_keys/rsa_key_oneline.txt`
- Passphrase: `SnowflakeKeyPair2024`

**Security Note**: Never commit these files to git. They are local test keys only.




