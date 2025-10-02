#!/usr/bin/env python3
"""
Get Real OIDC Token from Okta for Testing
This script will get a real OIDC token that we can use with Snowflake
"""
import base64
import hashlib
import os
import secrets
import webbrowser
import requests
from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse as urlparse
import threading
import time
import json
import sys

# ===== CONFIG (replace with your app values) =====
OKTA_DOMAIN = "https://trial-4708025.okta.com"
AUTH_SERVER_ID = "ausvy5xcluoqVgYQx697"  # from your Okta authorization server
CLIENT_ID = "0oavy4sm03zl5Wwjz697"       # your Okta app's client ID
CLIENT_SECRET = "j_Z2id1EowrKAGrxvwOp4PvzraFyHf40lPgHqXjpSmNAcjIFpglfhfgvDU7s0PSP"
REDIRECT_URI = "http://localhost:8080/callback"
SCOPES = "openid profile email"
# ================================================

class OAuthCallbackHandler(BaseHTTPRequestHandler):
    """HTTP server to catch OAuth callback"""
    auth_code = None
    state = None
    
    def do_GET(self):
        parsed = urlparse.urlparse(self.path)
        qs = urlparse.parse_qs(parsed.query)
        
        if "code" in qs and "state" in qs:
            OAuthCallbackHandler.auth_code = qs["code"][0]
            OAuthCallbackHandler.state = qs["state"][0]
            
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            
            html = """
            <!DOCTYPE html>
            <html>
            <head>
                <title>OIDC Authentication Success</title>
                <style>
                    body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }
                    .success { color: #28a745; }
                    .info { color: #17a2b8; }
                </style>
            </head>
            <body>
                <h1 class="success">✅ Authentication Successful!</h1>
                <p>You can now close this window and return to the terminal.</p>
                <p class="info">The authorization code has been captured and will be exchanged for tokens.</p>
            </body>
            </html>
            """
            self.wfile.write(html.encode())
        else:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"Authorization failed - missing code or state parameter.")
    
    def log_message(self, format, *args):
        """Suppress default logging"""
        pass

def generate_pkce_pair():
    """Generate PKCE code verifier and challenge"""
    code_verifier = base64.urlsafe_b64encode(os.urandom(40)).rstrip(b'=').decode('utf-8')
    code_challenge = base64.urlsafe_b64encode(
        hashlib.sha256(code_verifier.encode('utf-8')).digest()
    ).rstrip(b'=').decode('utf-8')
    return code_verifier, code_challenge

def start_local_server():
    """Start local server to catch OAuth callback"""
    server = HTTPServer(("localhost", 8080), OAuthCallbackHandler)
    
    # Start server in a separate thread
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True
    server_thread.start()
    
    print("🌐 Local server started on http://localhost:8080")
    return server

def wait_for_callback(server):
    """Wait for OAuth callback"""
    print("   Waiting for OAuth callback...")
    
    # Wait for authorization code
    timeout = 300  # 5 minutes
    start_time = time.time()
    
    while OAuthCallbackHandler.auth_code is None:
        if time.time() - start_time > timeout:
            print("⏰ Timeout waiting for authorization code")
            server.shutdown()
            return None, None
        time.sleep(0.1)
    
    auth_code = OAuthCallbackHandler.auth_code
    state = OAuthCallbackHandler.state
    
    # Shutdown server
    server.shutdown()
    server.server_close()
    
    return auth_code, state

def open_browser_for_auth(authorize_url):
    """Open browser for authentication"""
    print("4️⃣ Opening browser for authentication...")
    try:
        webbrowser.open(authorize_url)
        print("   ✅ Browser opened successfully")
        print("   🔐 Please complete authentication in your browser")
        return True
    except Exception as e:
        print(f"   ❌ Failed to open browser: {e}")
        print(f"   📋 Please manually visit: {authorize_url}")
        return False

def get_tokens(auth_code, code_verifier):
    """Exchange authorization code for tokens"""
    token_url = f"{OKTA_DOMAIN}/oauth2/{AUTH_SERVER_ID}/v1/token"
    data = {
        "grant_type": "authorization_code",
        "client_id": CLIENT_ID,
        "redirect_uri": REDIRECT_URI,
        "code": auth_code,
        "code_verifier": code_verifier,
    }
    
    # Add client secret if provided
    if CLIENT_SECRET:
        data["client_secret"] = CLIENT_SECRET
    
    headers = {"Content-Type": "application/x-www-form-urlencoded"}
    
    print("🔄 Exchanging authorization code for tokens...")
    resp = requests.post(token_url, data=data, headers=headers)
    resp.raise_for_status()
    return resp.json()

def main():
    print("🔐 Get Real OIDC Token for Snowflake Testing")
    print("=" * 60)
    print("This will get a real OIDC token that we can use with Snowflake")
    print()
    
    # Step 1: Generate PKCE parameters
    print("1️⃣ Generating PKCE parameters...")
    code_verifier, code_challenge = generate_pkce_pair()
    state = secrets.token_urlsafe(16)
    
    print(f"   Code Verifier: {code_verifier[:20]}...")
    print(f"   Code Challenge: {code_challenge}")
    print(f"   State: {state}")
    print()
    
    # Step 2: Construct authorization URL
    print("2️⃣ Constructing authorization URL...")
    authorize_url = (
        f"{OKTA_DOMAIN}/oauth2/{AUTH_SERVER_ID}/v1/authorize"
        f"?client_id={CLIENT_ID}"
        f"&response_type=code"
        f"&scope={SCOPES}"
        f"&redirect_uri={REDIRECT_URI}"
        f"&state={state}"
        f"&code_challenge={code_challenge}"
        f"&code_challenge_method=S256"
    )
    
    print(f"   Authorization URL: {authorize_url[:80]}...")
    print()
    
    # Step 3: Start local server
    print("3️⃣ Starting local server for OAuth callback...")
    server = start_local_server()
    
    # Step 4: Open browser for authentication
    open_browser_for_auth(authorize_url)
    print()
    
    # Step 5: Wait for callback
    auth_code, received_state = wait_for_callback(server)
    
    if auth_code is None:
        print("❌ Failed to get authorization code")
        return 1
    
    # Step 4: Verify state parameter
    print("4️⃣ Verifying state parameter...")
    if state != received_state:
        print(f"❌ State mismatch! Expected: {state}, Received: {received_state}")
        return 1
    
    print(f"✅ State verification successful")
    print(f"   Authorization Code: {auth_code[:20]}...")
    print()
    
    # Step 5: Exchange code for tokens
    print("5️⃣ Exchanging authorization code for tokens...")
    try:
        tokens = get_tokens(auth_code, code_verifier)
        print("✅ Token exchange successful!")
        print()
        
        # Display token information
        print("📋 Real Token Information:")
        print(f"   Access Token: {tokens.get('access_token', 'N/A')[:50]}...")
        print(f"   ID Token: {tokens.get('id_token', 'N/A')[:50]}...")
        print(f"   Token Type: {tokens.get('token_type', 'N/A')}")
        print(f"   Expires In: {tokens.get('expires_in', 'N/A')} seconds")
        print(f"   Scope: {tokens.get('scope', 'N/A')}")
        print()
        
        # Save tokens to file
        with open('real_oidc_tokens.json', 'w') as f:
            json.dump(tokens, f, indent=2)
        print("💾 Real tokens saved to: real_oidc_tokens.json")
        
        # Generate Snowflake connection string with real token
        if 'access_token' in tokens:
            snowflake_conn = (
                f"account=your-account.snowflakecomputing.com;"
                f"warehouse=COMPUTE_WH;"
                f"database=SNOWFLAKE_SAMPLE_DATA;"
                f"role=PUBLIC;"
                f"auth_type=OIDC;"
                f"oidc_token={tokens['access_token']}"
            )
            
            with open('real_snowflake_connection.txt', 'w') as f:
                f.write(snowflake_conn)
            print("💾 Real Snowflake connection string saved to: real_snowflake_connection.txt")
            print()
            
            print("🎉 Real OIDC Token Obtained Successfully!")
            print("=" * 60)
            print("✅ Real token obtained from Okta")
            print("✅ Token exchange completed")
            print("✅ Connection string generated")
            print("✅ Ready for real Snowflake testing")
            print()
            print("🚀 Next Steps:")
            print("   1. Use the real token with a real Snowflake instance")
            print("   2. Test actual query execution")
            print("   3. Validate end-to-end functionality")
            
            return 0
        else:
            print("❌ No access token in response")
            return 1
            
    except requests.exceptions.RequestException as e:
        print(f"❌ Token exchange failed: {e}")
        if hasattr(e, 'response') and e.response is not None:
            print(f"   Response: {e.response.text}")
        return 1
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return 1

if __name__ == "__main__":
    try:
        exit_code = main()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n\n⏹️  Token retrieval interrupted by user")
        sys.exit(1)
