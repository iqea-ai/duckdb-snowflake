#include "flow.hpp"
#include "pkce.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <curl/curl.h>
#include <json/json.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif __APPLE__
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace duckdb {
namespace snowflake {
namespace auth {

OIDCFlow::OIDCFlow(const OIDCConfig& config) : config_(config), flow_active_(false) {
    // Validate configuration
    if (config_.client_id.empty()) {
        throw InvalidInputException("OIDC client_id is required");
    }
    if (config_.issuer_url.empty()) {
        throw InvalidInputException("OIDC issuer_url is required");
    }
    if (config_.redirect_uri.empty()) {
        throw InvalidInputException("OIDC redirect_uri is required");
    }
}

std::string OIDCFlow::StartAuthFlow(const std::function<void(const std::string&)>& browser_callback) {
    // Generate PKCE parameters
    auto pkce_pair = PKCEGenerator::GeneratePKCE();
    code_verifier_ = pkce_pair.first;
    code_challenge_ = pkce_pair.second;
    
    // Generate state parameter for CSRF protection
    state_ = PKCEGenerator::GenerateState();
    
    // Build authorization URL
    std::string auth_url = BuildAuthorizationURL();
    
    // Mark flow as active
    flow_active_ = true;
    
    // Open browser or use callback
    if (browser_callback) {
        browser_callback(auth_url);
    } else {
        OpenBrowser(auth_url);
    }
    
    return auth_url;
}

TokenResponse OIDCFlow::ExchangeCodeForToken(const std::string& authorization_code, const std::string& state) {
    if (!flow_active_) {
        throw InvalidInputException("No active OIDC flow. Call StartAuthFlow() first.");
    }
    
    // Verify state parameter for CSRF protection
    if (state != state_) {
        throw InvalidInputException("Invalid state parameter. Possible CSRF attack.");
    }
    
    // Build token request
    std::stringstream request_body;
    request_body << "grant_type=authorization_code"
                 << "&client_id=" << UrlEncode(config_.client_id)
                 << "&redirect_uri=" << UrlEncode(config_.redirect_uri)
                 << "&code=" << UrlEncode(authorization_code)
                 << "&code_verifier=" << UrlEncode(code_verifier_);
    
    // Make token request
    std::string response_body = MakeTokenRequest(request_body.str());
    
    // Parse response
    TokenResponse response = ParseTokenResponse(response_body);
    
    // Reset flow state
    ResetFlow();
    
    return response;
}

void OIDCFlow::ResetFlow() {
    code_verifier_.clear();
    code_challenge_.clear();
    state_.clear();
    flow_active_ = false;
}

std::string OIDCFlow::BuildAuthorizationURL() const {
    std::stringstream url;
    url << config_.issuer_url;
    
    // Ensure issuer URL ends with /v1/authorize (Okta format)
    if (!StringUtil::EndsWith(config_.issuer_url, "/v1/authorize")) {
        if (!StringUtil::EndsWith(config_.issuer_url, "/")) {
            url << "/";
        }
        url << "v1/authorize";
    }
    
    // Add query parameters
    url << "?client_id=" << UrlEncode(config_.client_id)
        << "&redirect_uri=" << UrlEncode(config_.redirect_uri)
        << "&response_type=" << UrlEncode(config_.response_type)
        << "&scope=" << UrlEncode(config_.scope)
        << "&state=" << UrlEncode(state_)
        << "&code_challenge=" << UrlEncode(code_challenge_)
        << "&code_challenge_method=" << UrlEncode(config_.code_challenge_method);
    
    return url.str();
}

std::string OIDCFlow::MakeTokenRequest(const std::string& request_body) {
    CURL* curl;
    CURLcode res;
    std::string response_data;
    
    curl = curl_easy_init();
    if (!curl) {
        throw IOException("Failed to initialize CURL");
    }
    
    // Build token endpoint URL
    std::string token_url = config_.issuer_url;
    if (!StringUtil::EndsWith(token_url, "/token")) {
        if (!StringUtil::EndsWith(token_url, "/")) {
            token_url += "/";
        }
        token_url += "token";
    }
    
    // Set up CURL options
    curl_easy_setopt(curl, CURLOPT_URL, token_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_body.length());
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, std::string* data) -> size_t {
        size_t total_size = size * nmemb;
        data->append(static_cast<char*>(contents), total_size);
        return total_size;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw IOException("HTTP request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    return response_data;
}

TokenResponse OIDCFlow::ParseTokenResponse(const std::string& response_body) {
    TokenResponse response;
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    std::istringstream stream(response_body);
    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        response.error = "invalid_json";
        response.error_description = "Failed to parse JSON response: " + errors;
        return response;
    }
    
    // Check for error response
    if (root.isMember("error")) {
        response.error = root["error"].asString();
        if (root.isMember("error_description")) {
            response.error_description = root["error_description"].asString();
        }
        return response;
    }
    
    // Parse successful response
    if (root.isMember("access_token")) {
        response.access_token = root["access_token"].asString();
    }
    if (root.isMember("id_token")) {
        response.id_token = root["id_token"].asString();
    }
    if (root.isMember("token_type")) {
        response.token_type = root["token_type"].asString();
    }
    if (root.isMember("expires_in")) {
        response.expires_in = root["expires_in"].asInt();
    }
    if (root.isMember("refresh_token")) {
        response.refresh_token = root["refresh_token"].asString();
    }
    
    return response;
}

std::string OIDCFlow::UrlEncode(const std::string& str) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return str; // Fallback to original string
    }
    
    char* encoded = curl_easy_escape(curl, str.c_str(), static_cast<int>(str.length()));
    std::string result = encoded ? encoded : str;
    
    curl_free(encoded);
    curl_easy_cleanup(curl);
    
    return result;
}

void OIDCFlow::OpenBrowser(const std::string& url) const {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::string command = "open '" + url + "'";
    system(command.c_str());
#else
    std::string command = "xdg-open '" + url + "'";
    system(command.c_str());
#endif
}

} // namespace auth
} // namespace snowflake
} // namespace duckdb
