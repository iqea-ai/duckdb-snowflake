#include "duckdb/common/test_helpers.hpp"
#include "duckdb/common/exception.hpp"
#include "src/auth/pkce.hpp"
#include <iostream>
#include <string>

using namespace duckdb;
using namespace duckdb::snowflake::auth;

void TestPKCEGeneration() {
    std::cout << "Testing PKCE generation..." << std::endl;
    
    // Test basic PKCE generation
    auto pkce_pair = PKCEGenerator::GeneratePKCE();
    std::string code_verifier = pkce_pair.first;
    std::string code_challenge = pkce_pair.second;
    
    // Verify code_verifier length (should be 43-128 characters)
    if (code_verifier.length() < 43 || code_verifier.length() > 128) {
        throw Exception("Code verifier length is invalid: " + std::to_string(code_verifier.length()));
    }
    
    // Verify code_verifier contains only allowed characters
    const std::string allowed_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    for (char c : code_verifier) {
        if (allowed_chars.find(c) == std::string::npos) {
            throw Exception("Code verifier contains invalid character: " + std::string(1, c));
        }
    }
    
    // Verify code_challenge is not empty
    if (code_challenge.empty()) {
        throw Exception("Code challenge is empty");
    }
    
    // Test that the same code_verifier produces the same code_challenge
    std::string regenerated_challenge = PKCEGenerator::GenerateCodeChallenge(code_verifier);
    if (regenerated_challenge != code_challenge) {
        throw Exception("Code challenge regeneration failed");
    }
    
    std::cout << "✓ PKCE generation test passed" << std::endl;
    std::cout << "  Code verifier length: " << code_verifier.length() << std::endl;
    std::cout << "  Code challenge length: " << code_challenge.length() << std::endl;
}

void TestCodeVerifierGeneration() {
    std::cout << "Testing code verifier generation with different lengths..." << std::endl;
    
    // Test minimum length
    std::string min_verifier = PKCEGenerator::GenerateCodeVerifier(43);
    if (min_verifier.length() != 43) {
        throw Exception("Minimum length code verifier has wrong length: " + std::to_string(min_verifier.length()));
    }
    
    // Test maximum length
    std::string max_verifier = PKCEGenerator::GenerateCodeVerifier(128);
    if (max_verifier.length() != 128) {
        throw Exception("Maximum length code verifier has wrong length: " + std::to_string(max_verifier.length()));
    }
    
    // Test default length
    std::string default_verifier = PKCEGenerator::GenerateCodeVerifier();
    if (default_verifier.length() != 64) {
        throw Exception("Default length code verifier has wrong length: " + std::to_string(default_verifier.length()));
    }
    
    std::cout << "✓ Code verifier generation test passed" << std::endl;
}

void TestStateGeneration() {
    std::cout << "Testing state parameter generation..." << std::endl;
    
    std::string state = PKCEGenerator::GenerateState();
    if (state.empty()) {
        throw Exception("Generated state is empty");
    }
    
    // State should be hex string, so should have even length
    if (state.length() % 2 != 0) {
        throw Exception("Generated state has odd length (not hex): " + std::to_string(state.length()));
    }
    
    // Test custom length
    std::string custom_state = PKCEGenerator::GenerateState(16);
    if (custom_state.length() != 32) { // 16 bytes = 32 hex characters
        throw Exception("Custom length state has wrong length: " + std::to_string(custom_state.length()));
    }
    
    std::cout << "✓ State generation test passed" << std::endl;
}

void TestErrorHandling() {
    std::cout << "Testing error handling..." << std::endl;
    
    // Test invalid code verifier length
    try {
        PKCEGenerator::GenerateCodeVerifier(42); // Too short
        throw Exception("Should have thrown exception for too short code verifier");
    } catch (const InvalidInputException& e) {
        // Expected
    }
    
    try {
        PKCEGenerator::GenerateCodeVerifier(129); // Too long
        throw Exception("Should have thrown exception for too long code verifier");
    } catch (const InvalidInputException& e) {
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
