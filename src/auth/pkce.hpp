#pragma once

#include <string>
#include <array>

namespace duckdb {
namespace snowflake {
namespace auth {

/**
 * PKCE (Proof Key for Code Exchange) utility class for OIDC authentication
 * 
 * PKCE is a security extension to OAuth 2.0 for public clients that cannot
 * securely store a client secret. It uses a dynamically generated code_verifier
 * and its SHA256 hash (code_challenge) to prevent authorization code interception attacks.
 */
class PKCEGenerator {
public:
    /**
     * Generate a PKCE code verifier and challenge pair
     * 
     * The code_verifier is a cryptographically random string using the characters
     * [A-Z], [a-z], [0-9], "-", ".", "_", and "~", with a length between 43 and 128 characters.
     * 
     * The code_challenge is the SHA256 hash of the code_verifier, base64url-encoded.
     * 
     * @return std::pair<std::string, std::string> - {code_verifier, code_challenge}
     */
    static std::pair<std::string, std::string> GeneratePKCE();

    /**
     * Generate a random code verifier
     * 
     * @param length The length of the code verifier (43-128 characters)
     * @return std::string The generated code verifier
     */
    static std::string GenerateCodeVerifier(size_t length = 64);

    /**
     * Generate a code challenge from a code verifier
     * 
     * @param code_verifier The code verifier to hash
     * @return std::string The base64url-encoded SHA256 hash
     */
    static std::string GenerateCodeChallenge(const std::string& code_verifier);

    /**
     * Generate a random state parameter for CSRF protection
     * 
     * @param length The length of the state parameter
     * @return std::string The generated state parameter
     */
    static std::string GenerateState(size_t length = 32);

private:
    /**
     * Base64url encode a byte array
     * 
     * @param data The data to encode
     * @return std::string The base64url-encoded string
     */
    static std::string Base64UrlEncode(const std::array<uint8_t, 32>& data);

    /**
     * Generate cryptographically secure random bytes
     * 
     * @param output The output buffer
     * @param size The number of bytes to generate
     */
    static void GenerateRandomBytes(uint8_t* output, size_t size);
};

} // namespace auth
} // namespace snowflake
} // namespace duckdb
