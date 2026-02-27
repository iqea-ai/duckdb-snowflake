# Testing PR #12: Snowflake Cert and ADBC Driver Install Fix

This guide walks you through testing PR #12 locally, including building the extension, setting up Snowflake, and testing all the fixes.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Checkout PR #12](#checkout-pr-12)
3. [Build the Extension](#build-the-extension)
4. [Snowflake Setup](#snowflake-setup)
5. [Testing Scenarios](#testing-scenarios)
6. [Verification Checklist](#verification-checklist)

---

## Prerequisites

### Required Software

- **Git** (with access to GitHub)
- **CMake** >= 3.18
- **C++17 compiler** (GCC 7+, Clang 6+, or MSVC 2019+)
- **OpenSSL** (for generating RSA keys)
- **DuckDB** (will be built as part of the extension)
- **Snowflake account** with admin access

### Required Snowflake Access

- `ACCOUNTADMIN` role (to create users and set RSA public keys)
- Access to a database and warehouse for testing

---

## Checkout PR #12

### Step 1: Navigate to Repository

```bash
cd duckdb-snowflake
```

### Step 2: Fetch PR Branch

**Method 1: Direct Fetch from GitHub (Recommended)**

```bash
# Fetch the PR branch directly from GitHub
git fetch https://github.com/iqea-ai/duckdb-snowflake.git pull/12/head:pr12-test

# Checkout the PR branch
git checkout pr12-test
```

**Method 2: Add Fork Remote (If Method 1 doesn't work)**

The PR is from `praveentandra`'s fork. You can add it as a remote:

```bash
# Add the fork as a remote
git remote add pr12-fork https://github.com/praveentandra/duckdb-snowflake.git

# Fetch from the fork
git fetch pr12-fork

# Checkout the branch (check the actual branch name on the PR page)
git checkout pr12-fork/snowflake-cert-and-adbc-driver-install-fix

# Or create a local branch from it
git checkout -b pr12-test pr12-fork/snowflake-cert-and-adbc-driver-install-fix
```

**Method 3: Manual Checkout (If you know the branch name)**

```bash
# Fetch all refs from origin
git fetch origin

# Check if the branch exists locally or remotely
git branch -a | grep -i "12\|cert\|adbc"

# If you see a branch name, checkout directly
git checkout -b pr12-test origin/branch-name
```

**Method 4: Using GitHub CLI (if installed)**

```bash
# Install gh CLI if not installed: brew install gh
gh pr checkout 12

# Or fetch without checking out
gh pr checkout 12 --branch pr12-test
```

**Method 5: Manual Download and Apply**

If all else fails, you can manually download the PR patch:

```bash
# Download the PR as a patch file
curl -L https://github.com/iqea-ai/duckdb-snowflake/pull/12.patch -o pr12.patch

# Apply the patch to your current branch
git apply pr12.patch

# Or create a new branch and apply
git checkout -b pr12-test
git apply pr12.patch
```

### Step 3: Handle Local Changes (if any)

If you have uncommitted changes, you'll need to stash or commit them first:

```bash
# Check what files have changed
git status

# Option 1: Stash your changes (recommended for testing)
git stash
git checkout pr12-test
# Later, restore your changes: git stash pop

# Option 2: Commit your changes
git add .
git commit -m "WIP: Local changes before testing PR #12"
git checkout pr12-test

# Option 3: Create a new branch from current state, then merge PR
git checkout -b my-test-branch
git merge pr12-test
```

### Step 4: Verify You're on the Right Branch

```bash
git branch
# Should show: * pr12-test (or the branch name you used)

git log --oneline -5
# Should show commits from PR #12, including:
# - Fix private key authentication and improve error messages
# - Add consistent logging system for user-facing messages
# - Improve ADBC driver installation scripts for all platforms
# - Update documentation and cleanup
# - Preserving casing for table name identifiers...
```

---

## Build the Extension

### Step 1: Initialize Submodules (if needed)

```bash
git submodule update --init --recursive
```

### Step 2: Update DuckDB Submodule to v1.4.2

**IMPORTANT:** PR #12 requires DuckDB v1.4.2. The submodule might be pointing to an older version.

```bash
# Navigate to duckdb submodule
cd duckdb

# Check current version
git describe --tags

# Update to v1.4.2 (required for PR #12)
git checkout v1.4.2

# Verify version
git log --oneline -1
# Should show: Fix minor crypto issues (#19716) or similar v1.4.2 commit

# Go back to extension root
cd ..
```

**Note:** If you see `v1.3.1` or earlier, you MUST update to `v1.4.2` or the build will fail with `ExtensionLoader` errors.

### Step 3: Build in Debug Mode (to see logging)

```bash
# Build debug version (includes DEBUG_SNOWFLAKE for logging)
make debug-build
```

**Expected output:**
```
Building DuckDB Snowflake Extension...
[Build output...]
Build complete!
```

**If you see errors about `ExtensionLoader` not found:**
- The DuckDB submodule is not on v1.4.2
- Run Step 2 again to update it

### Step 3: Verify Build

```bash
# Check if DuckDB binary exists
ls -lh build/debug/duckdb

# Check if extension is built
ls -lh build/debug/extension/snowflake/snowflake.duckdb_extension
```

### Step 4: Install ADBC Driver (if not already installed)

```bash
# Run the installer script
bash scripts/install-adbc-driver.sh
```

**Expected output:**
```
════════════════════════════════════════════════════════════════
     DuckDB Snowflake ADBC Driver Installer
     Version: 1.8.0
════════════════════════════════════════════════════════════════

ℹ Detected platform: Darwin arm64 (osx_arm64)
ℹ Detected DuckDB version: v1.4.2
ℹ Installation directory: /Users/yourname/.duckdb/extensions/v1.4.2/osx_arm64
...
✓ ADBC Snowflake driver installed successfully!
```

---

## Snowflake Setup

### Step 1: Generate RSA Key Pair

Create a directory for your keys:

```bash
mkdir -p ~/snowflake_keys
cd ~/snowflake_keys
```

#### Option A: Encrypted Private Key (Recommended for Testing PR #12)

```bash
# Generate encrypted private key with passphrase
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out rsa_key_encrypted.p8 -passout pass:testpass123

# Generate public key from encrypted private key
openssl rsa -in rsa_key_encrypted.p8 -pubout -out rsa_key_encrypted.pub -passin pass:testpass123

# Extract public key content (remove headers/footers, single line)
grep -v "BEGIN PUBLIC KEY" rsa_key_encrypted.pub | grep -v "END PUBLIC KEY" | tr -d '\n' > rsa_key_encrypted_oneline.txt

# Display the public key (you'll need this for Snowflake)
cat rsa_key_encrypted_oneline.txt
```

**Save the output** - you'll paste it into Snowflake.

#### Option B: Unencrypted Private Key (For Testing Path Handling)

```bash
# Generate unencrypted private key
openssl genrsa 2048 | openssl pkcs8 -topk8 -inform PEM -out rsa_key_unencrypted.p8 -nocrypt

# Generate public key
openssl rsa -in rsa_key_unencrypted.p8 -pubout -out rsa_key_unencrypted.pub

# Extract public key content
grep -v "BEGIN PUBLIC KEY" rsa_key_unencrypted.pub | grep -v "END PUBLIC KEY" | tr -d '\n' > rsa_key_unencrypted_oneline.txt

# Display the public key
cat rsa_key_unencrypted_oneline.txt
```

**Save this output too.**

#### Option C: Generate .pem Format Key (For Testing PEM Support)

PR #12 uses `FileExists()` which works with any file extension. Test `.pem` format:

```bash
# Generate private key in PEM format (traditional format)
openssl genrsa 2048 -out rsa_key.pem

# Convert to PKCS#8 format (which Snowflake requires)
openssl pkcs8 -topk8 -inform PEM -in rsa_key.pem -out rsa_key_pkcs8.pem -nocrypt

# Generate public key
openssl rsa -in rsa_key_pkcs8.pem -pubout -out rsa_key_pem.pub

# Extract public key content
grep -v "BEGIN PUBLIC KEY" rsa_key_pem.pub | grep -v "END PUBLIC KEY" | tr -d '\n' > rsa_key_pem_oneline.txt

# Display the public key
cat rsa_key_pem_oneline.txt
```

**Note:** Snowflake requires PKCS#8 format, so even if the file is named `.pem`, it should be PKCS#8 encoded. The `.pem` extension is just a naming convention - PR #12's `FileExists()` will detect it as a file regardless of extension.

### Step 2: Create Test User in Snowflake

Log into Snowflake and run these SQL commands:

```sql
-- Switch to ACCOUNTADMIN role
USE ROLE ACCOUNTADMIN;

-- Create a test user for key pair authentication
CREATE USER IF NOT EXISTS test_keypair_user
  LOGIN_NAME = 'test_keypair_user'
  DEFAULT_ROLE = PUBLIC
  DEFAULT_WAREHOUSE = 'COMPUTE_WH'
  DEFAULT_NAMESPACE = 'SNOWFLAKE_SAMPLE_DATA.PUBLIC';

-- Grant necessary permissions
GRANT ROLE PUBLIC TO USER test_keypair_user;
GRANT USAGE ON DATABASE SNOWFLAKE_SAMPLE_DATA TO ROLE PUBLIC;
GRANT USAGE ON WAREHOUSE COMPUTE_WH TO ROLE PUBLIC;
GRANT USAGE ON SCHEMA SNOWFLAKE_SAMPLE_DATA.TPCH_SF1 TO ROLE PUBLIC;
GRANT SELECT ON ALL TABLES IN SCHEMA SNOWFLAKE_SAMPLE_DATA.TPCH_SF1 TO ROLE PUBLIC;
```

### Step 3: Set RSA Public Key in Snowflake

#### For Encrypted Key:

```sql
USE ROLE ACCOUNTADMIN;

-- Set the public key (replace with your actual public key from rsa_key_encrypted_oneline.txt)
ALTER USER test_keypair_user SET RSA_PUBLIC_KEY='PASTE_YOUR_ENCRYPTED_PUBLIC_KEY_HERE';

-- Verify the key is set
DESC USER test_keypair_user;
-- Look for RSA_PUBLIC_KEY_FP field - it should show a fingerprint
```

#### For Unencrypted Key:

```sql
USE ROLE ACCOUNTADMIN;

-- Set the public key (replace with your actual public key from rsa_key_unencrypted_oneline.txt)
ALTER USER test_keypair_user SET RSA_PUBLIC_KEY='PASTE_YOUR_UNENCRYPTED_PUBLIC_KEY_HERE';

-- Verify the key is set
DESC USER test_keypair_user;
```

### Step 4: Get Your Snowflake Account Identifier

```sql
-- Get your account identifier (use the short form, not the full URL)
SELECT CURRENT_ACCOUNT() AS account_identifier;
```

**Save this value** - you'll need it for testing.

---

## Testing Scenarios

### Test 1: Encrypted Private Key with Absolute Path

This tests the main PR #12 fix for encrypted keys.

```bash
cd duckdb-snowflake

# Start DuckDB CLI
./build/debug/duckdb
```

In DuckDB CLI:

```sql
-- Load the extension
LOAD snowflake;

-- Create secret with encrypted private key (absolute path)
CREATE SECRET test_keypair_encrypted (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',  -- Replace with your account from Step 4
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/YOUR_USERNAME/snowflake_keys/rsa_key_encrypted.p8',  -- Replace with your path
    PRIVATE_KEY_PASSPHRASE 'testpass123',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Verify secret was created
SELECT name, type FROM duckdb_secrets() WHERE name = 'test_keypair_encrypted';

-- Attach Snowflake database
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_keypair_encrypted, READ_ONLY);

-- Test connection - should succeed
SELECT COUNT(*) FROM sf_test.information_schema.tables WHERE table_schema = 'TPCH_SF1';

-- Test data access
SELECT COUNT(*) FROM sf_test.tpch_sf1.customer;

-- Clean up
DETACH sf_test;
DROP SECRET test_keypair_encrypted;
```

**Expected Result:** Connection succeeds, queries return data.

### Test 2: Unencrypted Private Key with Absolute Path

```sql
-- Create secret with unencrypted private key
CREATE SECRET test_keypair_unencrypted (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/YOUR_USERNAME/snowflake_keys/rsa_key_unencrypted.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Attach and test
ATTACH '' AS sf_test2 (TYPE snowflake, SECRET test_keypair_unencrypted, READ_ONLY);
SELECT COUNT(*) FROM sf_test2.tpch_sf1.customer;
DETACH sf_test2;
DROP SECRET test_keypair_unencrypted;
```

**Expected Result:** Connection succeeds without passphrase.

### Test 3: Relative Path Handling (PR #12 Fix)

This tests the FileExists() improvement for relative paths.

```bash
# Navigate to the keys directory
cd ~/snowflake_keys

# Start DuckDB from this directory
/path/to/duckdb-snowflake/build/debug/duckdb
```

In DuckDB CLI:

```sql
LOAD snowflake;

-- Test with relative path (tests PR #12 FileExists() fix)
CREATE SECRET test_keypair_relative (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY './rsa_key_unencrypted.p8',  -- Relative path
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Should work with relative path
ATTACH '' AS sf_test3 (TYPE snowflake, SECRET test_keypair_relative, READ_ONLY);
SELECT COUNT(*) FROM sf_test3.tpch_sf1.customer;
DETACH sf_test3;
DROP SECRET test_keypair_relative;
```

**Expected Result:** Relative path works correctly (this is a PR #12 fix).

### Test 4: Windows Path Handling (if on Windows or testing cross-platform)

```sql
LOAD snowflake;

-- Test Windows-style path (tests PR #12 Windows path fix)
CREATE SECRET test_keypair_windows (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY 'C:\\Users\\YOUR_USERNAME\\snowflake_keys\\rsa_key_unencrypted.p8',  -- Windows path
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_test4 (TYPE snowflake, SECRET test_keypair_windows, READ_ONLY);
SELECT COUNT(*) FROM sf_test4.tpch_sf1.customer;
DETACH sf_test4;
DROP SECRET test_keypair_windows;
```

**Expected Result:** Windows path format works (PR #12 fix).

### Test 5: Inline PEM Key Content

```sql
LOAD snowflake;

-- Get the key content
-- In terminal: cat ~/snowflake_keys/rsa_key_unencrypted.p8
-- Copy the entire content including BEGIN/END lines

-- Create secret with inline PEM content
CREATE SECRET test_keypair_inline (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC...
[PASTE FULL PEM CONTENT HERE]
-----END PRIVATE KEY-----',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_test5 (TYPE snowflake, SECRET test_keypair_inline, READ_ONLY);
SELECT COUNT(*) FROM sf_test5.tpch_sf1.customer;
DETACH sf_test5;
DROP SECRET test_keypair_inline;
```

**Expected Result:** Inline PEM content works.

### Test 6: .pem File Extension Support

Test that PR #12's `FileExists()` works with `.pem` files (not just `.p8`):

```sql
LOAD snowflake;

-- Create secret with .pem file extension (tests FileExists() works with any extension)
CREATE SECRET test_keypair_pem (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/YOUR_USERNAME/snowflake_keys/rsa_key_pkcs8.pem',  -- .pem extension
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Should work because FileExists() detects it as a file (PR #12 improvement)
ATTACH '' AS sf_test6 (TYPE snowflake, SECRET test_keypair_pem, READ_ONLY);
SELECT COUNT(*) FROM sf_test6.tpch_sf1.customer;
DETACH sf_test6;
DROP SECRET test_keypair_pem;
```

**Expected Result:** `.pem` files work correctly (PR #12 uses `FileExists()` instead of checking extensions).

**Key Point:** PR #12's improvement is that it uses `FileExists()` to detect file paths, which works with:
- `.p8` files (PKCS#8 format)
- `.pem` files (PEM format, still PKCS#8 encoded)
- Any other file extension
- Relative paths
- Windows paths

### Test 7: Error Handling (Improved Messages)

Test the improved error messages from PR #12:

```sql
LOAD snowflake;

-- Test with non-existent file (should show helpful error)
CREATE SECRET test_keypair_missing (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/nonexistent/path/key.p8',  -- Doesn't exist
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- This should fail with a helpful error message
ATTACH '' AS sf_test6 (TYPE snowflake, SECRET test_keypair_missing, READ_ONLY);
```

**Expected Result:** Error message should show:
- `Failed to open private key file: /nonexistent/path/key.p8`
- Clear indication of what went wrong

### Test 8: Wrong Passphrase Handling

```sql
LOAD snowflake;

-- Test with wrong passphrase
CREATE SECRET test_keypair_wrong_passphrase (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/YOUR_USERNAME/snowflake_keys/rsa_key_encrypted.p8',
    PRIVATE_KEY_PASSPHRASE 'wrong_password',  -- Wrong passphrase
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

-- Should fail with authentication error
ATTACH '' AS sf_test7 (TYPE snowflake, SECRET test_keypair_wrong_passphrase, READ_ONLY);
```

**Expected Result:** Authentication error from Snowflake (not a file error).

### Test 9: Table Name Casing Preservation

Test the table name casing fix from PR #12:

```sql
LOAD snowflake;

-- Use your working secret
CREATE SECRET test_keypair_casing (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/YOUR_USERNAME/snowflake_keys/rsa_key_unencrypted.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_test8 (TYPE snowflake, SECRET test_keypair_casing, READ_ONLY);

-- Check table names preserve original casing from Snowflake
SELECT table_name FROM sf_test8.information_schema.tables 
WHERE table_schema = 'TPCH_SF1' 
ORDER BY table_name 
LIMIT 10;

-- Test case-insensitive querying (should work)
SELECT COUNT(*) FROM sf_test8.tpch_sf1.customer;  -- lowercase
SELECT COUNT(*) FROM sf_test8.tpch_sf1.CUSTOMER;  -- uppercase
SELECT COUNT(*) FROM sf_test8.tpch_sf1."CUSTOMER"; -- quoted

-- All should work, but table_name in information_schema should show original case
DETACH sf_test8;
DROP SECRET test_keypair_casing;
```

**Expected Result:** 
- Table names in `information_schema.tables` show original casing from Snowflake
- Case-insensitive queries work
- Identifiers preserve case (DuckDB convention)

### Test 10: Logging Improvements

Check for improved logging output (if built in debug mode):

```bash
# Run DuckDB with debug output
DEBUG_SNOWFLAKE=1 ./build/debug/duckdb
```

In DuckDB CLI:

```sql
LOAD snowflake;

-- Create secret and attach
CREATE SECRET test_logging (
    TYPE snowflake,
    ACCOUNT 'YOUR_ACCOUNT_IDENTIFIER',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '/Users/YOUR_USERNAME/snowflake_keys/rsa_key_unencrypted.p8',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);

ATTACH '' AS sf_test9 (TYPE snowflake, SECRET test_logging, READ_ONLY);
```

**Expected Output:** Look for:
- `[Snowflake Ext INFO]` messages (if PR #12 logging macros are used)
- Extension directory and driver path shown on startup
- Helpful error messages if something fails

### Test 11: ADBC Driver Installation Script

Test the improved installer script:

```bash
# Remove existing driver to test fresh install
rm ~/.duckdb/extensions/v1.4.2/osx_arm64/libadbc_driver_snowflake.so

# Run installer
bash scripts/install-adbc-driver.sh
```

**Expected Output:**
- Shows DuckDB version (not ADBC version) in header
- Detects platform correctly
- Installs to correct location: `~/.duckdb/extensions/<version>/<platform>`
- Clean, informative output

---

## Verification Checklist

Use this checklist to verify all PR #12 fixes:

### Private Key Authentication Fixes
- [ ] Encrypted private key with passphrase works
- [ ] Unencrypted private key works
- [ ] Absolute path works
- [ ] Relative path works (PR #12 fix)
- [ ] Windows path format works (PR #12 fix)
- [ ] Inline PEM content works
- [ ] `.pem` file extension works (tests FileExists() with different extensions)
- [ ] Error messages are helpful when file doesn't exist
- [ ] Wrong passphrase shows authentication error (not file error)

### Logging Improvements
- [ ] Consistent log format (if implemented)
- [ ] Helpful error messages show searched locations
- [ ] Extension directory and driver path shown on startup

### ADBC Driver Installation
- [ ] Installer script works on your platform
- [ ] Shows DuckDB version (not ADBC version)
- [ ] Installs to correct location
- [ ] Clean, informative output

### Table Name Casing
- [ ] Table names preserve original casing from Snowflake
- [ ] Case-insensitive queries work
- [ ] Identifiers preserve case (DuckDB convention)

### Documentation
- [ ] README installation instructions are clear
- [ ] Windows PowerShell instructions present (if applicable)
- [ ] Platform support matrix accurate

---

## Troubleshooting

### Build Issues

```bash
# Clean build
make clean
make debug-build

# If submodule issues:
git submodule update --init --recursive
```

### Connection Issues

```sql
-- Verify secret exists
SELECT * FROM duckdb_secrets() WHERE name = 'your_secret_name';

-- Check secret contents (will show redacted values)
DESCRIBE SECRET your_secret_name;
```

### Key Issues

```bash
# Verify key file exists
ls -lh ~/snowflake_keys/rsa_key*.p8

# Test key format
openssl rsa -in ~/snowflake_keys/rsa_key_encrypted.p8 -check -passin pass:testpass123
```

### Snowflake Issues

```sql
-- Verify user exists
SELECT * FROM SNOWFLAKE.ACCOUNT_USAGE.USERS WHERE name = 'TEST_KEYPAIR_USER';

-- Verify RSA public key is set
DESC USER test_keypair_user;
-- Look for RSA_PUBLIC_KEY_FP field

-- Test connection from Snowflake CLI (if available)
snowflake sql -a YOUR_ACCOUNT -u test_keypair_user --private-key-path ~/snowflake_keys/rsa_key_unencrypted.p8
```

---

## Quick Test Script

Save this as `test_pr12.sh`:

```bash
#!/bin/bash
set -e

echo "=== Testing PR #12 ==="
echo ""

# Set your values here
ACCOUNT="YOUR_ACCOUNT_IDENTIFIER"
KEY_PATH="$HOME/snowflake_keys/rsa_key_unencrypted.p8"
DUCKDB="./build/debug/duckdb"

echo "1. Testing encrypted key..."
$DUCKDB <<EOF
LOAD snowflake;
CREATE SECRET test_pr12_encrypted (
    TYPE snowflake,
    ACCOUNT '$ACCOUNT',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '$HOME/snowflake_keys/rsa_key_encrypted.p8',
    PRIVATE_KEY_PASSPHRASE 'testpass123',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_pr12_encrypted, READ_ONLY);
SELECT COUNT(*) as table_count FROM sf_test.information_schema.tables WHERE table_schema = 'TPCH_SF1';
DETACH sf_test;
DROP SECRET test_pr12_encrypted;
EOF

echo ""
echo "2. Testing unencrypted key..."
$DUCKDB <<EOF
LOAD snowflake;
CREATE SECRET test_pr12_unencrypted (
    TYPE snowflake,
    ACCOUNT '$ACCOUNT',
    USER 'test_keypair_user',
    AUTH_TYPE 'key_pair',
    PRIVATE_KEY '$KEY_PATH',
    DATABASE 'SNOWFLAKE_SAMPLE_DATA',
    WAREHOUSE 'COMPUTE_WH'
);
ATTACH '' AS sf_test (TYPE snowflake, SECRET test_pr12_unencrypted, READ_ONLY);
SELECT COUNT(*) as customer_count FROM sf_test.tpch_sf1.customer;
DETACH sf_test;
DROP SECRET test_pr12_unencrypted;
EOF

echo ""
echo "=== All tests passed! ==="
```

Make it executable and run:

```bash
chmod +x test_pr12.sh
# Edit the script to set your ACCOUNT and paths
./test_pr12.sh
```

---

## Next Steps

After testing:

1. **Report Issues**: If you find any problems, create an issue or comment on PR #12
2. **Test Edge Cases**: Try different path formats, key sizes, etc.
3. **Performance Testing**: Test with larger datasets
4. **Cross-Platform**: Test on different operating systems if possible

---

## References

- [PR #12](https://github.com/iqea-ai/duckdb-snowflake/pull/12)
- [Issue #6](https://github.com/iqea-ai/duckdb-snowflake/issues/6)
- [Authentication Documentation](docs/AUTHENTICATION.md)
- [Authentication Setup Guide](docs/AUTHENTICATION_SETUP.md)

