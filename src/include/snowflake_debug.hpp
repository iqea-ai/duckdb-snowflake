#pragma once

#include <cstdio>

// Logging macros for Snowflake extension
// Usage: LOG_INFO("Connected to account: %s", account.c_str());

// Always enabled - for important user-facing information
#define LOG_INFO(...) fprintf(stderr, "[Snowflake Ext INFO] " __VA_ARGS__)
#define LOG_WARN(...) fprintf(stderr, "[Snowflake Ext WARN] " __VA_ARGS__)
#define LOG_ERROR(...) fprintf(stderr, "[Snowflake Ext ERROR] " __VA_ARGS__)

// Debug logging - only enabled in debug builds
#ifdef DEBUG_SNOWFLAKE
#define LOG_DEBUG(...) fprintf(stderr, "[Snowflake Ext DEBUG] " __VA_ARGS__)
#define DPRINT(...) fprintf(stderr, "[Snowflake Ext DEBUG] " __VA_ARGS__)  // Backwards compatibility
#else
#define LOG_DEBUG(...) ((void)0)
#define DPRINT(...) ((void)0)  // Backwards compatibility
#endif
