# 🔐 OIDC Authentication with DuckDB Secrets

## Overview

DuckDB secrets are the **primary and recommended** way to use OIDC authentication with the Snowflake extension. This approach provides better security, easier management, and a cleaner user experience.

## 🚀 Quick Start

### 1. Create OIDC Secret

```sql
-- Create a secret with OIDC token (pre-obtained)
CREATE SECRET snowflake_oidc (
    TYPE snowflake,
    account = 'FLWILYJ-FL43292',
    database = 'SNOWFLAKE_SAMPLE_DATA',
    warehouse = 'COMPUTE_WH',
    role = 'PUBLIC',
    oidc_token = 'eyJraWQiOiJfV2Vvb09HakV4ODJDenVTUzhLWlFGZXh5Qk0yX0...'
);

-- Or create a secret with OIDC client configuration for interactive flow
CREATE SECRET snowflake_oidc_config (
    TYPE snowflake,
    account = 'FLWILYJ-FL43292',
    database = 'SNOWFLAKE_SAMPLE_DATA',
    warehouse = 'COMPUTE_WH',
    role = 'PUBLIC',
    oidc_client_id = '0oavy4sm03zl5Wwjz697',
    oidc_issuer_url = 'https://trial-4708025.okta.com/oauth2/ausvy5xcluoqVgYQx697',
    oidc_redirect_uri = 'http://localhost:8080/callback',
    oidc_scope = 'openid profile email'
);
```

### 2. Attach Using Secret

```sql
-- Attach Snowflake using the secret (PREFERRED METHOD)
ATTACH '' AS snowflake_db (TYPE snowflake, SECRET snowflake_oidc);

-- Now you can query Snowflake tables directly
SELECT * FROM snowflake_db.SNOWFLAKE_SAMPLE_DATA.TPCH_SF1.CUSTOMER LIMIT 10;
```

## 📋 Complete OIDC Secret Parameters

### Required Parameters
- `account` - Your Snowflake account identifier
- `database` - Target database name

### Optional Parameters
- `warehouse` - Snowflake warehouse name
- `role` - Snowflake role name
- `schema` - Default schema name

### OIDC Authentication Parameters
- `oidc_token` - Pre-obtained JWT access token (recommended for production)
- `oidc_client_id` - OIDC client ID for interactive flow
- `oidc_issuer_url` - OIDC issuer URL (e.g., Okta authorization server)
- `oidc_redirect_uri` - OAuth redirect URI
- `oidc_scope` - OAuth scopes (default: "openid")

## 🔄 Authentication Methods

### Method 1: Pre-obtained Token (Recommended for Production)

```sql
-- Create secret with existing token
CREATE SECRET prod_snowflake (
    TYPE snowflake,
    account = 'YOUR_ACCOUNT',
    database = 'YOUR_DATABASE',
    warehouse = 'YOUR_WAREHOUSE',
    role = 'YOUR_ROLE',
    oidc_token = 'your_jwt_access_token_here'
);

-- Use immediately
ATTACH '' AS sf (TYPE snowflake, SECRET prod_snowflake);
```

### Method 2: Interactive OIDC Flow

```sql
-- Create secret with OIDC configuration
CREATE SECRET interactive_snowflake (
    TYPE snowflake,
    account = 'YOUR_ACCOUNT',
    database = 'YOUR_DATABASE',
    warehouse = 'YOUR_WAREHOUSE',
    role = 'YOUR_ROLE',
    oidc_client_id = 'your_client_id',
    oidc_issuer_url = 'https://your-okta-domain.com/oauth2/your_auth_server',
    oidc_redirect_uri = 'http://localhost:8080/callback',
    oidc_scope = 'openid profile email'
);

-- This will trigger interactive OIDC flow
ATTACH '' AS sf (TYPE snowflake, SECRET interactive_snowflake);
```

## 🛡️ Security Benefits

### 1. **Encrypted Storage**
- Secrets are encrypted at rest in DuckDB's secure storage
- No plaintext credentials in connection strings

### 2. **Access Control**
- Secrets can be scoped to specific users/roles
- Centralized credential management

### 3. **Audit Trail**
- DuckDB tracks secret usage and access
- Better compliance and monitoring

### 4. **Token Management**
- OIDC tokens are stored securely
- Automatic token refresh (when implemented)

## 📊 Comparison: Secrets vs Connection Strings

| Feature | DuckDB Secrets | Connection Strings |
|---------|----------------|-------------------|
| **Security** | ✅ Encrypted storage | ❌ Plaintext in memory |
| **Management** | ✅ Centralized | ❌ Scattered |
| **Reusability** | ✅ Named profiles | ❌ Copy-paste |
| **Audit** | ✅ Tracked access | ❌ No tracking |
| **OIDC Support** | ✅ Full support | ✅ Full support |
| **Ease of Use** | ✅ Simple reference | ❌ Long strings |

## 🔧 Advanced Usage

### Multiple Environments

```sql
-- Development environment
CREATE SECRET snowflake_dev (
    TYPE snowflake,
    account = 'DEV_ACCOUNT',
    database = 'DEV_DATABASE',
    warehouse = 'DEV_WH',
    oidc_token = 'dev_token_here'
);

-- Production environment
CREATE SECRET snowflake_prod (
    TYPE snowflake,
    account = 'PROD_ACCOUNT',
    database = 'PROD_DATABASE',
    warehouse = 'PROD_WH',
    oidc_token = 'prod_token_here'
);

-- Switch between environments easily
ATTACH '' AS dev_db (TYPE snowflake, SECRET snowflake_dev);
ATTACH '' AS prod_db (TYPE snowflake, SECRET snowflake_prod);
```

### Secret Management

```sql
-- List all secrets
SELECT * FROM duckdb_secrets();

-- Drop a secret
DROP SECRET snowflake_oidc;

-- Update a secret (drop and recreate)
DROP SECRET snowflake_oidc;
CREATE SECRET snowflake_oidc (
    TYPE snowflake,
    account = 'UPDATED_ACCOUNT',
    database = 'UPDATED_DATABASE',
    oidc_token = 'updated_token_here'
);
```

## 🚨 Migration from Connection Strings

### Before (Legacy Method)
```sql
-- Old way - connection string
ATTACH 'account=FLWILYJ-FL43292;warehouse=COMPUTE_WH;database=SNOWFLAKE_SAMPLE_DATA;role=PUBLIC;oidc_token=eyJraWQiOiJfV2Vvb09HakV4ODJDenVTUzhLWlFGZXh5Qk0yX0...' AS sf (TYPE snowflake);
```

### After (Recommended Method)
```sql
-- New way - DuckDB secrets
CREATE SECRET snowflake_oidc (
    TYPE snowflake,
    account = 'FLWILYJ-FL43292',
    warehouse = 'COMPUTE_WH',
    database = 'SNOWFLAKE_SAMPLE_DATA',
    role = 'PUBLIC',
    oidc_token = 'eyJraWQiOiJfV2Vvb09HakV4ODJDenVTUzhLWlFGZXh5Qk0yX0...'
);

ATTACH '' AS sf (TYPE snowflake, SECRET snowflake_oidc);
```

## 🎯 Best Practices

### 1. **Use Descriptive Names**
```sql
-- Good
CREATE SECRET snowflake_prod_analytics (...);

-- Avoid
CREATE SECRET secret1 (...);
```

### 2. **Environment Separation**
```sql
-- Separate secrets for different environments
CREATE SECRET snowflake_dev (...);
CREATE SECRET snowflake_staging (...);
CREATE SECRET snowflake_prod (...);
```

### 3. **Token Rotation**
```sql
-- Regularly update tokens
DROP SECRET snowflake_prod;
CREATE SECRET snowflake_prod (
    TYPE snowflake,
    account = 'PROD_ACCOUNT',
    database = 'PROD_DATABASE',
    oidc_token = 'new_rotated_token_here'
);
```

### 4. **Minimal Permissions**
```sql
-- Use least-privilege roles
CREATE SECRET snowflake_readonly (
    TYPE snowflake,
    account = 'PROD_ACCOUNT',
    database = 'PROD_DATABASE',
    role = 'READONLY_ROLE',  -- Limited permissions
    oidc_token = 'readonly_token_here'
);
```

## 🔍 Troubleshooting

### Common Issues

1. **Secret Not Found**
   ```sql
   -- Check if secret exists
   SELECT * FROM duckdb_secrets() WHERE name = 'your_secret_name';
   ```

2. **Invalid OIDC Token**
   ```sql
   -- Verify token is valid and not expired
   -- Check token expiration time
   ```

3. **Missing Required Parameters**
   ```sql
   -- Ensure account and database are provided
   CREATE SECRET snowflake_oidc (
       TYPE snowflake,
       account = 'REQUIRED',
       database = 'REQUIRED',
       oidc_token = 'your_token'
   );
   ```

## 🎉 Summary

DuckDB secrets provide a **secure, manageable, and user-friendly** way to handle OIDC authentication with Snowflake. They are now the **default and recommended** method for all OIDC authentication scenarios.

**Key Benefits:**
- ✅ **Security**: Encrypted storage and access control
- ✅ **Simplicity**: Clean, readable configuration
- ✅ **Management**: Centralized credential handling
- ✅ **Flexibility**: Support for all OIDC parameters
- ✅ **Compatibility**: Works with existing OIDC flows

**Next Steps:**
1. Create your first OIDC secret
2. Test with a simple query
3. Migrate existing connection strings to secrets
4. Set up environment-specific secrets
5. Implement token rotation procedures
