#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/rand.h>

// Standalone PKCE test without DuckDB dependencies
class PKCEGenerator {
public:
    static std::pair<std::string, std::string> GeneratePKCE() {
        std::string code_verifier = GenerateCodeVerifier();
        std::string code_challenge = GenerateCodeChallenge(code_verifier);
        return {code_verifier, code_challenge};
    }

    static std::string GenerateCodeVerifier(size_t length = 64) {
        if (length < 43 || length > 128) {
            throw std::invalid_argument("Code verifier length must be between 43 and 128 characters");
        }

        const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
        
        std::string code_verifier;
        code_verifier.reserve(length);
        
        std::vector<uint8_t> random_bytes(length);
        GenerateRandomBytes(random_bytes.data(), length);
        
        for (size_t i = 0; i < length; ++i) {
            code_verifier += charset[random_bytes[i] % charset.length()];
        }
        
        return code_verifier;
    }

    static std::string GenerateCodeChallenge(const std::string& code_verifier) {
        std::array<uint8_t, SHA256_DIGEST_LENGTH> hash;
        SHA256_CTX sha256;
        
        if (!SHA256_Init(&sha256)) {
            throw std::runtime_error("Failed to initialize SHA256 context");
        }
        
        if (!SHA256_Update(&sha256, code_verifier.c_str(), code_verifier.length())) {
            throw std::runtime_error("Failed to update SHA256 context");
        }
        
        if (!SHA256_Final(hash.data(), &sha256)) {
            throw std::runtime_error("Failed to finalize SHA256 hash");
        }
        
        return Base64UrlEncode(hash);
    }

    static std::string GenerateState(size_t length = 32) {
        std::vector<uint8_t> random_bytes(length);
        GenerateRandomBytes(random_bytes.data(), length);
        
        std::stringstream ss;
        for (size_t i = 0; i < length; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(random_bytes[i]);
        }
        
        return ss.str();
    }

private:
    static std::string Base64UrlEncode(const std::array<uint8_t, 32>& data) {
        const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        
        std::string result;
        result.reserve(44);
        
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t value = 0;
            int bits = 0;
            
            for (int j = 0; j < 3 && i + j < data.size(); ++j) {
                value = (value << 8) | data[i + j];
                bits += 8;
            }
            
            for (int j = 0; j < 4; ++j) {
                if (bits > 0) {
                    result += alphabet[(value >> (18 - j * 6)) & 0x3F];
                    bits -= 6;
                } else {
                    break;
                }
            }
        }
        
        return result;
    }

    static void GenerateRandomBytes(uint8_t* output, size_t size) {
        if (RAND_bytes(output, static_cast<int>(size)) != 1) {
            throw std::runtime_error("Failed to generate cryptographically secure random bytes");
        }
    }
};

void TestPKCEGeneration() {
    std::cout << "Testing PKCE generation..." << std::endl;
    
    auto pkce_pair = PKCEGenerator::GeneratePKCE();
    std::string code_verifier = pkce_pair.first;
    std::string code_challenge = pkce_pair.second;
    
    if (code_verifier.length() < 43 || code_verifier.length() > 128) {
        throw std::runtime_error("Code verifier length is invalid: " + std::to_string(code_verifier.length()));
    }
    
    const std::string allowed_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    for (char c : code_verifier) {
        if (allowed_chars.find(c) == std::string::npos) {
            throw std::runtime_error("Code verifier contains invalid character: " + std::string(1, c));
        }
    }
    
    if (code_challenge.empty()) {
        throw std::runtime_error("Code challenge is empty");
    }
    
    std::string regenerated_challenge = PKCEGenerator::GenerateCodeChallenge(code_verifier);
    if (regenerated_challenge != code_challenge) {
        throw std::runtime_error("Code challenge regeneration failed");
    }
    
    std::cout << "✓ PKCE generation test passed" << std::endl;
    std::cout << "  Code verifier length: " << code_verifier.length() << std::endl;
    std::cout << "  Code challenge length: " << code_challenge.length() << std::endl;
}

void TestCodeVerifierGeneration() {
    std::cout << "Testing code verifier generation with different lengths..." << std::endl;
    
    std::string min_verifier = PKCEGenerator::GenerateCodeVerifier(43);
    if (min_verifier.length() != 43) {
        throw std::runtime_error("Minimum length code verifier has wrong length: " + std::to_string(min_verifier.length()));
    }
    
    std::string max_verifier = PKCEGenerator::GenerateCodeVerifier(128);
    if (max_verifier.length() != 128) {
        throw std::runtime_error("Maximum length code verifier has wrong length: " + std::to_string(max_verifier.length()));
    }
    
    std::string default_verifier = PKCEGenerator::GenerateCodeVerifier();
    if (default_verifier.length() != 64) {
        throw std::runtime_error("Default length code verifier has wrong length: " + std::to_string(default_verifier.length()));
    }
    
    std::cout << "✓ Code verifier generation test passed" << std::endl;
}

void TestStateGeneration() {
    std::cout << "Testing state parameter generation..." << std::endl;
    
    std::string state = PKCEGenerator::GenerateState();
    if (state.empty()) {
        throw std::runtime_error("Generated state is empty");
    }
    
    if (state.length() % 2 != 0) {
        throw std::runtime_error("Generated state has odd length (not hex): " + std::to_string(state.length()));
    }
    
    std::string custom_state = PKCEGenerator::GenerateState(16);
    if (custom_state.length() != 32) {
        throw std::runtime_error("Custom length state has wrong length: " + std::to_string(custom_state.length()));
    }
    
    std::cout << "✓ State generation test passed" << std::endl;
}

void TestErrorHandling() {
    std::cout << "Testing error handling..." << std::endl;
    
    try {
        PKCEGenerator::GenerateCodeVerifier(42);
        throw std::runtime_error("Should have thrown exception for too short code verifier");
    } catch (const std::invalid_argument& e) {
        // Expected
    }
    
    try {
        PKCEGenerator::GenerateCodeVerifier(129);
        throw std::runtime_error("Should have thrown exception for too long code verifier");
    } catch (const std::invalid_argument& e) {
        // Expected
    }
    
    std::cout << "✓ Error handling test passed" << std::endl;
}

int main() {
    try {
        TestPKCEGeneration();
        TestCodeVerifierGeneration();
        TestStateGeneration();
        TestErrorHandling();
        
        std::cout << "\n🎉 All PKCE tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
