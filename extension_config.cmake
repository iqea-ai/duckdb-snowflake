# This file is included by DuckDB's build system. It specifies which extension to load

# Extension from this repo
duckdb_extension_load(snowflake
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
)

# DuckDB v2 builds a loaded extension without linking it. Link it into the
# shell and unittest so tests and the manifest smoke test use the static build.
duckdb_extension_statically_link(snowflake)

# Any extra extensions that should be built
# e.g.: duckdb_extension_load(json)