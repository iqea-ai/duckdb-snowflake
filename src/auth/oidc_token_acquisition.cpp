#include "oidc_token_acquisition.hpp"
#include "pkce.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include <curl/curl.h>
#include <json/json.h>
#include <sstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#else
#include <cstdlib>
#endif

namespace duckdb {
namespace snowflake {
namespace auth {

// HTTP response callback for libcurl
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    std::string *response = static_cast<std::string *>(userp);
    response->append(static_cast<char *>(contents), total_size);
    return total_size;
}

std::string OIDCTokenAcquisition::MakeHTTPRequest(const std::string &url, 
                                                 const std::string &method,
                                                 const std::string &data,
                                                 const std::string &content_type) {
    CURL *curl;
    CURLcode res;
    std::string response_data;

    curl = curl_easy_init();
    if (!curl) {
        throw InvalidInputException("Failed to initialize HTTP client");
    }

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    }
    
    // Set headers
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("Content-Type: " + content_type).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    // Set SSL options
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw InvalidInputException("HTTP request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    return response_data;
}

void OIDCTokenAcquisition::LaunchBrowser(const std::string &url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    CFURLRef cf_url = CFURLCreateWithBytes(nullptr, 
                                          reinterpret_cast<const UInt8*>(url.c_str()), 
                                          url.length(), 
                                          kCFStringEncodingUTF8, 
                                          nullptr);
    if (cf_url) {
        LSOpenCFURLRef(cf_url, nullptr);
        CFRelease(cf_url);
    }
#else
    std::string command = "xdg-open '" + url + "'";
    std::system(command.c_str());
#endif
}

std::string OIDCTokenAcquisition::StartCallbackServer(int port) {
    // For now, we'll return instructions for manual completion
    // In a full implementation, this would start a local HTTP server
    // to capture the OAuth callback
    std::stringstream ss;
    ss << "http://localhost:" << port << "/callback";
    return ss.str();
}

std::pair<std::string, std::string> OIDCTokenAcquisition::GeneratePKCE() {
    return PKCEGenerator::GeneratePKCE();
}

std::string OIDCTokenAcquisition::GenerateAuthorizationURL(const OIDCTokenRequest &request,
                                                          const std::string &code_challenge,
                                                          const std::string &state) {
    std::stringstream url;
    url << request.issuer_url << "/v1/authorize";
    url << "?client_id=" << request.client_id;
    url << "&response_type=code";
    url << "&scope=" << request.scope;
    url << "&redirect_uri=" << request.redirect_uri;
    url << "&state=" << state;
    url << "&code_challenge=" << code_challenge;
    url << "&code_challenge_method=S256";
    
    return url.str();
}

OIDCTokenResponse OIDCTokenAcquisition::ParseTokenResponse(const std::string &json_response) {
    OIDCTokenResponse response;
    
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_response, root)) {
        response.error = "invalid_json";
        response.error_description = "Failed to parse JSON response";
        return response;
    }
    
    if (root.isMember("error")) {
        response.error = root["error"].asString();
        response.error_description = root.get("error_description", "").asString();
        return response;
    }
    
    response.access_token = root.get("access_token", "").asString();
    response.id_token = root.get("id_token", "").asString();
    response.token_type = root.get("token_type", "").asString();
    response.expires_in = root.get("expires_in", 0).asInt();
    response.refresh_token = root.get("refresh_token", "").asString();
    
    return response;
}

OIDCTokenResponse OIDCTokenAcquisition::ExchangeCodeForToken(const std::string &authorization_code,
                                                            const OIDCTokenRequest &request) {
    // Get token endpoint
    std::string token_url = request.issuer_url + "/v1/token";
    
    // Prepare token request data
    std::stringstream data;
    data << "grant_type=authorization_code";
    data << "&client_id=" << request.client_id;
    data << "&redirect_uri=" << request.redirect_uri;
    data << "&code=" << authorization_code;
    
    // Add client secret if provided (for confidential clients)
    if (!request.client_secret.empty()) {
        data << "&client_secret=" << request.client_secret;
    }
    
    // Make HTTP request
    std::string response_json = MakeHTTPRequest(token_url, "POST", data.str());
    
    // Parse response
    return ParseTokenResponse(response_json);
}

OIDCTokenResponse OIDCTokenAcquisition::AcquireTokenInteractive(const OIDCTokenRequest &request) {
    // Generate PKCE parameters
    auto [code_verifier, code_challenge] = GeneratePKCE();
    
    // Generate state parameter
    std::string state = "state_" + std::to_string(std::time(nullptr));
    
    // Generate authorization URL
    std::string auth_url = GenerateAuthorizationURL(request, code_challenge, state);
    
    // Launch browser
    LaunchBrowser(auth_url);
    
    // For now, throw an exception with instructions
    // In a full implementation, this would start a callback server
    // and wait for the authorization code
    throw NotImplementedException(
        "Interactive OIDC flow requires manual completion.\n"
        "Please visit the following URL to complete authentication:\n" +
        auth_url + "\n\n" +
        "After authentication, you will be redirected to a URL with an authorization code.\n"
        "Please provide the authorization code to continue.\n\n" +
        "Alternatively, you can pre-obtain an OIDC token and set it in the configuration."
    );
}

} // namespace auth
} // namespace snowflake
} // namespace duckdb
