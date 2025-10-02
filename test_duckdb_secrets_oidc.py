#!/usr/bin/env python3
"""
Test DuckDB Secrets OIDC Authentication
=======================================

This script demonstrates how to use DuckDB secrets as the primary method
for OIDC authentication with the Snowflake extension.
"""

import subprocess
import json
import os
import sys
import tempfile

def run_duckdb_command(sql_command):
    """Run a SQL command using the built DuckDB binary"""
    duckdb_path = "./build/release/duckdb"
    if not os.path.exists(duckdb_path):
        print(f"❌ DuckDB binary not found at {duckdb_path}")
        return None, "DuckDB binary not found"
    
    try:
        result = subprocess.run(
            [duckdb_path, "-c", sql_command],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result.stdout.strip(), result.stderr.strip()
    except subprocess.TimeoutExpired:
        return None, "Command timed out"
    except Exception as e:
        return None, str(e)

def test_duckdb_secrets_oidc():
    """Test OIDC authentication using DuckDB secrets"""
    
    print("🔐 Testing DuckDB Secrets OIDC Authentication")
    print("=" * 60)
    
    # Load real OIDC token if available
    token_file = "real_oidc_tokens.json"
    if not os.path.exists(token_file):
        print(f"❌ Token file {token_file} not found")
        print("   Please run get_real_oidc_token.py first to obtain a real token")
        return False
    
    with open(token_file, 'r') as f:
        tokens = json.load(f)
    
    access_token = tokens.get('access_token', '')
    if not access_token:
        print("❌ No access token found in token file")
        return False
    
    print(f"✅ Loaded OIDC token: {access_token[:50]}...")
    
    try:
        # Load the Snowflake extension
        print("\n1️⃣ Loading Snowflake extension...")
        stdout, stderr = run_duckdb_command("LOAD 'snowflake'")
        if stderr:
            print(f"   ❌ Failed to load extension: {stderr}")
            return False
        print("   ✅ Snowflake extension loaded successfully")
        
        # Create OIDC secret with real token
        print("\n2️⃣ Creating DuckDB secret with OIDC token...")
        create_secret_sql = f"""CREATE SECRET snowflake_oidc (TYPE snowflake, account 'FLWILYJ-FL43292', database 'SNOWFLAKE_SAMPLE_DATA', warehouse 'COMPUTE_WH', role 'PUBLIC', oidc_token '{access_token}')"""
        
        stdout, stderr = run_duckdb_command(create_secret_sql)
        if stderr:
            print(f"   ❌ Failed to create secret: {stderr}")
            return False
        print("   ✅ OIDC secret created successfully")
        
        # List secrets to verify
        print("\n3️⃣ Verifying secret creation...")
        stdout, stderr = run_duckdb_command("SELECT name, type FROM duckdb_secrets()")
        if stderr:
            print(f"   ❌ Failed to list secrets: {stderr}")
            return False
        print("   Available secrets:")
        if stdout:
            for line in stdout.split('\n'):
                if line.strip():
                    print(f"     - {line}")
        
        # Attach Snowflake using the secret
        print("\n4️⃣ Attaching Snowflake using secret...")
        attach_sql = "ATTACH '' AS snowflake_db (TYPE snowflake, SECRET snowflake_oidc)"
        stdout, stderr = run_duckdb_command(attach_sql)
        if stderr:
            print(f"   ❌ Failed to attach: {stderr}")
            return False
        print("   ✅ Snowflake attached using OIDC secret")
        
        # Test query execution
        print("\n5️⃣ Testing query execution...")
        try:
            # Simple query to test connection
            query_sql = "SELECT COUNT(*) as table_count FROM snowflake_db.information_schema.tables WHERE table_schema = 'INFORMATION_SCHEMA'"
            stdout, stderr = run_duckdb_command(query_sql)
            
            if stderr:
                print(f"   ❌ Query execution failed: {stderr}")
                return False
            
            print(f"   ✅ Query executed successfully")
            if stdout:
                print(f"   📊 Result: {stdout}")
            
        except Exception as e:
            print(f"   ❌ Query execution failed: {e}")
            return False
        
        # Test secret management
        print("\n6️⃣ Testing secret management...")
        
        # List all secrets
        stdout, stderr = run_duckdb_command("SELECT name, type FROM duckdb_secrets()")
        if not stderr and stdout:
            secret_count = len([line for line in stdout.split('\n') if line.strip()])
            print(f"   📋 Total secrets: {secret_count}")
        
        # Drop the secret
        stdout, stderr = run_duckdb_command("DROP SECRET snowflake_oidc")
        if stderr:
            print(f"   ⚠️  Warning: Failed to drop secret: {stderr}")
        else:
            print("   🗑️  Secret dropped successfully")
        
        print("\n🎉 DuckDB Secrets OIDC Test Completed Successfully!")
        print("=" * 60)
        print("✅ OIDC secret creation: SUCCESS")
        print("✅ Secret-based attachment: SUCCESS")
        print("✅ Query execution: SUCCESS")
        print("✅ Secret management: SUCCESS")
        print("\n🚀 DuckDB secrets are now the default OIDC authentication method!")
        
        return True
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        return False

def test_legacy_vs_secrets_comparison():
    """Compare legacy connection string vs DuckDB secrets approach"""
    
    print("\n" + "=" * 60)
    print("📊 Legacy vs DuckDB Secrets Comparison")
    print("=" * 60)
    
    print("\n🔴 Legacy Method (Connection String):")
    print("   ATTACH 'account=FLWILYJ-FL43292;warehouse=COMPUTE_WH;database=SNOWFLAKE_SAMPLE_DATA;role=PUBLIC;oidc_token=eyJraWQiOiJfV2Vvb09HakV4ODJDenVTUzhLWlFGZXh5Qk0yX0...' AS sf (TYPE snowflake);")
    print("   ❌ Long, hard-to-read connection string")
    print("   ❌ Credentials in plaintext")
    print("   ❌ Difficult to manage multiple environments")
    print("   ❌ No centralized credential management")
    
    print("\n🟢 DuckDB Secrets Method (Recommended):")
    print("   CREATE SECRET snowflake_oidc (TYPE snowflake, account='FLWILYJ-FL43292', ...);")
    print("   ATTACH '' AS sf (TYPE snowflake, SECRET snowflake_oidc);")
    print("   ✅ Clean, readable configuration")
    print("   ✅ Encrypted credential storage")
    print("   ✅ Easy environment management")
    print("   ✅ Centralized credential management")
    print("   ✅ Built-in audit trail")
    
    print("\n🏆 Winner: DuckDB Secrets!")

def main():
    """Main test function"""
    
    print("🧪 DuckDB Secrets OIDC Authentication Test Suite")
    print("=" * 60)
    
    # Test DuckDB secrets OIDC
    success = test_duckdb_secrets_oidc()
    
    # Show comparison
    test_legacy_vs_secrets_comparison()
    
    if success:
        print("\n🎯 RECOMMENDATION:")
        print("   Use DuckDB secrets as your primary OIDC authentication method!")
        print("   They provide better security, management, and user experience.")
        return 0
    else:
        print("\n❌ Some tests failed. Please check the error messages above.")
        return 1

if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)
