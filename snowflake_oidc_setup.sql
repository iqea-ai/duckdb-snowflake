-- Phase 3: Snowflake OIDC Workload Identity Setup
-- Run these commands in your Snowflake account

-- 1. Create a service user with OIDC workload identity
CREATE USER snowflake_oidc_service
  WORKLOAD_IDENTITY = (
    TYPE = OIDC
    ISSUER = 'http://localhost:8080/realms/master'
    SUBJECT = 'service-account-duckdb-snowflake-client'  -- Service account username from Keycloak
    AUDIENCE = 'master-realm'  -- Snowflake expects this audience
  )
  TYPE = SERVICE;

-- 2. Grant necessary privileges to the service user
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO snowflake_oidc_service;
GRANT USAGE ON DATABASE YOUR_DATABASE_NAME TO snowflake_oidc_service;
GRANT USAGE ON SCHEMA YOUR_DATABASE_NAME.YOUR_SCHEMA_NAME TO snowflake_oidc_service;

-- 3. Grant SELECT privileges on tables (adjust as needed)
GRANT SELECT ON ALL TABLES IN SCHEMA YOUR_DATABASE_NAME.YOUR_SCHEMA_NAME TO snowflake_oidc_service;

-- 4. Optional: Create a role for the service user
CREATE ROLE snowflake_oidc_role;
GRANT ROLE snowflake_oidc_role TO USER snowflake_oidc_service;

-- 5. Grant role privileges
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE snowflake_oidc_role;
GRANT USAGE ON DATABASE YOUR_DATABASE_NAME TO ROLE snowflake_oidc_role;
GRANT USAGE ON SCHEMA YOUR_DATABASE_NAME.YOUR_SCHEMA_NAME TO ROLE snowflake_oidc_role;
GRANT SELECT ON ALL TABLES IN SCHEMA YOUR_DATABASE_NAME.YOUR_SCHEMA_NAME TO ROLE snowflake_oidc_role;
