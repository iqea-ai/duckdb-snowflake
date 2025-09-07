#pragma once

#ifdef DEBUG_SNOWFLAKE
#define DPRINT(...) fprintf(stderr, "[DEBUG] " __VA_ARGS__)
#else
#define DPRINT(...) ((void)0)
#endif
