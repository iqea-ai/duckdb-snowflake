#pragma once

#include <string>
#include <functional>

namespace duckdb {
namespace snowflake {
namespace auth {

/**
 * OIDC authentication flow configuration
 */
struct OIDCConfig {
    std::string client_id;
    std::string issuer_url;
    std::string redirect_uri;
    std::string scope = "openid";
    std::string response_type = "code";
    std::string code_challenge_method = "S256";
};

/**
 * OIDC token response structure
 */
struct TokenResponse {
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

/**
 * OIDC authentication flow handler
 * 
 * This class handles the complete OIDC authorization code flow with PKCE,
 * including authorization URL generation, token exchange, and browser interaction.
 */
class OIDCFlow {
public:
    /**
     * Constructor
     * 
     * @param config OIDC configuration
     */
    explicit OIDCFlow(const OIDCConfig& config);

    /**
     * Start the OIDC authorization flow
     * 
     * This method:
     * 1. Generates PKCE parameters (code_verifier, code_challenge, state)
     * 2. Constructs the authorization URL
     * 3. Opens the URL in the user's browser (or returns it for manual access)
     * 
     * @param browser_callback Optional callback to open URL in browser
     * @return std::string The authorization URL
     */
    std::string StartAuthFlow(const std::function<void(const std::string&)>& browser_callback = nullptr);

    /**
     * Exchange authorization code for tokens
     * 
     * @param authorization_code The authorization code from the callback
     * @param state The state parameter (for CSRF protection)
     * @return TokenResponse The token response
     */
    TokenResponse ExchangeCodeForToken(const std::string& authorization_code, const std::string& state);

    /**
     * Get the current PKCE code verifier (for token exchange)
     * 
     * @return std::string The code verifier
     */
    std::string GetCodeVerifier() const { return code_verifier_; }

    /**
     * Get the current state parameter
     * 
     * @return std::string The state parameter
     */
    std::string GetState() const { return state_; }

    /**
     * Check if the flow is in progress
     * 
     * @return bool True if flow is active
     */
    bool IsFlowActive() const { return flow_active_; }

    /**
     * Reset the flow state
     */
    void ResetFlow();

private:
    OIDCConfig config_;
    std::string code_verifier_;
    std::string code_challenge_;
    std::string state_;
    bool flow_active_;

    /**
     * Construct the authorization URL
     * 
     * @return std::string The complete authorization URL
     */
    std::string BuildAuthorizationURL() const;

    /**
     * Make HTTP POST request to token endpoint
     * 
     * @param request_body The request body
     * @return std::string The response body
     */
    std::string MakeTokenRequest(const std::string& request_body);

    /**
     * Parse token response JSON
     * 
     * @param response_body The JSON response
     * @return TokenResponse The parsed response
     */
    TokenResponse ParseTokenResponse(const std::string& response_body);

    /**
     * URL encode a string
     * 
     * @param str The string to encode
     * @return std::string The URL-encoded string
     */
    std::string UrlEncode(const std::string& str) const;

    /**
     * Open URL in default browser (platform-specific)
     * 
     * @param url The URL to open
     */
    void OpenBrowser(const std::string& url) const;
};

} // namespace auth
} // namespace snowflake
} // namespace duckdb
