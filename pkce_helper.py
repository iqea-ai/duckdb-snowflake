import base64
import hashlib
import os
import secrets
import webbrowser
import requests
from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib.parse as urlparse

# ===== CONFIG (replace with your app values) =====
OKTA_DOMAIN = "https://trial-4708025.okta.com"
AUTH_SERVER_ID = "ausvy5xcluoqVgYQx697"  # from your Okta authorization server
CLIENT_ID = "0oavy4sm03zl5Wwjz697"       # your Okta app's client ID
REDIRECT_URI = "http://localhost:8080/callback"
SCOPES = "openid profile email"
# ================================================

def generate_pkce_pair():
    code_verifier = base64.urlsafe_b64encode(os.urandom(40)).rstrip(b'=').decode('utf-8')
    code_challenge = base64.urlsafe_b64encode(
        hashlib.sha256(code_verifier.encode('utf-8')).digest()
    ).rstrip(b'=').decode('utf-8')
    return code_verifier, code_challenge

class OAuthCallbackHandler(BaseHTTPRequestHandler):
    auth_code = None

    def do_GET(self):
        parsed = urlparse.urlparse(self.path)
        qs = urlparse.parse_qs(parsed.query)
        if "code" in qs:
            OAuthCallbackHandler.auth_code = qs["code"][0]
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Authorization successful. You can close this window.")
        else:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"Authorization failed.")

def start_local_server():
    server = HTTPServer(("localhost", 8080), OAuthCallbackHandler)
    while OAuthCallbackHandler.auth_code is None:
        server.handle_request()
    return OAuthCallbackHandler.auth_code

def get_tokens(auth_code, code_verifier):
    token_url = f"{OKTA_DOMAIN}/oauth2/{AUTH_SERVER_ID}/v1/token"
    data = {
        "grant_type": "authorization_code",
        "client_id": CLIENT_ID,
        "redirect_uri": REDIRECT_URI,
        "code": auth_code,
        "code_verifier": code_verifier,
    }
    headers = {"Content-Type": "application/x-www-form-urlencoded"}
    resp = requests.post(token_url, data=data, headers=headers)
    resp.raise_for_status()
    return resp.json()

def main():
    code_verifier, code_challenge = generate_pkce_pair()
    state = secrets.token_urlsafe(16)

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

    print("Opening browser for authentication...")
    webbrowser.open(authorize_url)

    print("Waiting for redirect with authorization code...")
    auth_code = start_local_server()

    print(f"Authorization code received: {auth_code}")
    tokens = get_tokens(auth_code, code_verifier)

    print("Access token response:")
    print(tokens)

if __name__ == "__main__":
    main()
