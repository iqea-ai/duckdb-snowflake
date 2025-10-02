#pragma once

#include <string>
#include <functional>
#include <memory>

namespace duckdb {
namespace snowflake {
namespace auth {

struct OIDCTokenRequest {
    std::string client_id;
    std::string issuer_url;
    std::string redirect_uri;
    std::string scope;
    std::string client_secret; // Optional for confidential clients
};

struct OIDCTokenResponse {
    std::string access_token;
    std::string id_token;
    std::string token_type;
    int expires_in = 0;
    std::string refresh_token;
    std::string error;
    std::string error_description;

    bool IsValid() const {
        return !access_token.empty() && error.empty();
    }
};

class OIDCTokenAcquisition {
public:
    // Interactive OIDC flow with browser launch
    static OIDCTokenResponse AcquireTokenInteractive(const OIDCTokenRequest &request);
    
    // Non-interactive flow using existing authorization code
    static OIDCTokenResponse ExchangeCodeForToken(const std::string &authorization_code, 
                                                  const OIDCTokenRequest &request);
    
    // Generate PKCE parameters
    static std::pair<std::string, std::string> GeneratePKCE();
    
    // Generate authorization URL
    static std::string GenerateAuthorizationURL(const OIDCTokenRequest &request,
                                               const std::string &code_challenge,
                                               const std::string &state);

private:
    // HTTP request helpers
    static std::string MakeHTTPRequest(const std::string &url, 
                                      const std::string &method,
                                      const std::string &data = "",
                                      const std::string &content_type = "application/x-www-form-urlencoded");
    
    // Browser launch helper
    static void LaunchBrowser(const std::string &url);
    
    // Local server for OAuth callback
    static std::string StartCallbackServer(int port = 8080);
    
    // Parse JSON response
    static OIDCTokenResponse ParseTokenResponse(const std::string &json_response);
};

} // namespace auth
} // namespace snowflake
} // namespace duckdb
