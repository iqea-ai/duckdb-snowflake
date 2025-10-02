# Makefile for running OIDC auth tests

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I../src/include -I../src -I../../duckdb/src/include
LDFLAGS = -lssl -lcrypto -lcurl -ljsoncpp

# Test targets
PKCE_TEST = auth/pkce_test
FLOW_TEST = auth/flow_test

all: $(PKCE_TEST) $(FLOW_TEST)

$(PKCE_TEST): auth/pkce_test.cpp ../src/auth/pkce.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(FLOW_TEST): auth/flow_test.cpp ../src/auth/flow.cpp ../src/auth/pkce.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test: all
	@echo "Running PKCE tests..."
	./$(PKCE_TEST)
	@echo "Running OIDC flow tests..."
	./$(FLOW_TEST)
	@echo "All tests completed!"

clean:
	rm -f $(PKCE_TEST) $(FLOW_TEST)

.PHONY: all test clean
