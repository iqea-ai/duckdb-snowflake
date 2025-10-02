#include "pkce.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <vector>

namespace duckdb {
namespace snowflake {
namespace auth {

std::pair<std::string, std::string> PKCEGenerator::GeneratePKCE() {
    std::string code_verifier = GenerateCodeVerifier();
    std::string code_challenge = GenerateCodeChallenge(code_verifier);
    return {code_verifier, code_challenge};
}

std::string PKCEGenerator::GenerateCodeVerifier(size_t length) {
    // RFC 7636: code_verifier should be 43-128 characters
    if (length < 43 || length > 128) {
        throw InvalidInputException("Code verifier length must be between 43 and 128 characters");
    }

    // Characters allowed in code_verifier: [A-Z], [a-z], [0-9], "-", ".", "_", "~"
    const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    
    std::string code_verifier;
    code_verifier.reserve(length);
    
    // Generate cryptographically secure random bytes
    std::vector<uint8_t> random_bytes(length);
    GenerateRandomBytes(random_bytes.data(), length);
    
    // Convert random bytes to characters from the allowed charset
    for (size_t i = 0; i < length; ++i) {
        code_verifier += charset[random_bytes[i] % charset.length()];
    }
    
    return code_verifier;
}

std::string PKCEGenerator::GenerateCodeChallenge(const std::string& code_verifier) {
    // SHA256 hash the code_verifier
    std::array<uint8_t, SHA256_DIGEST_LENGTH> hash;
    SHA256_CTX sha256;
    
    if (!SHA256_Init(&sha256)) {
        throw IOException("Failed to initialize SHA256 context");
    }
    
    if (!SHA256_Update(&sha256, code_verifier.c_str(), code_verifier.length())) {
        throw IOException("Failed to update SHA256 context");
    }
    
    if (!SHA256_Final(hash.data(), &sha256)) {
        throw IOException("Failed to finalize SHA256 hash");
    }
    
    // Base64url encode the hash
    return Base64UrlEncode(hash);
}

std::string PKCEGenerator::GenerateState(size_t length) {
    // Generate random state parameter for CSRF protection
    std::vector<uint8_t> random_bytes(length);
    GenerateRandomBytes(random_bytes.data(), length);
    
    // Convert to hex string
    std::stringstream ss;
    for (size_t i = 0; i < length; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(random_bytes[i]);
    }
    
    return ss.str();
}

std::string PKCEGenerator::Base64UrlEncode(const std::array<uint8_t, 32>& data) {
    // Base64url alphabet (RFC 4648) - using - and _ instead of + and /
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    
    std::string result;
    result.reserve(44); // 32 bytes * 4/3 = 42.67, rounded up to 44
    
    // Process 3 bytes at a time
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t value = 0;
        int bits = 0;
        
        // Collect up to 3 bytes
        for (int j = 0; j < 3 && i + j < data.size(); ++j) {
            value = (value << 8) | data[i + j];
            bits += 8;
        }
        
        // Convert to base64url characters
        for (int j = 0; j < 4; ++j) {
            if (bits > 0) {
                result += alphabet[(value >> (18 - j * 6)) & 0x3F];
                bits -= 6;
            } else {
                // Add padding for incomplete groups (but we'll strip it later)
                result += '=';
            }
        }
    }
    
    // Strip padding characters (base64url doesn't use padding)
    while (!result.empty() && result.back() == '=') {
        result.pop_back();
    }
    
    return result;
}

void PKCEGenerator::GenerateRandomBytes(uint8_t* output, size_t size) {
    if (RAND_bytes(output, static_cast<int>(size)) != 1) {
        throw IOException("Failed to generate cryptographically secure random bytes");
    }
}

} // namespace auth
} // namespace snowflake
} // namespace duckdb
