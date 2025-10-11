cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME snowflake)
project(${TARGET_NAME})
include_directories(src/include)
include_directories(third_party)

# Configure build system for MinGW Windows builds
# Force Ninja generator for MinGW builds to fix OpenSSL build issues
# This helps avoid make install DESTDIR problems on MinGW
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Detected MinGW build - configuring Ninja generator for OpenSSL compatibility")
    # Set environment variable to suggest Ninja to vcpkg
    set(ENV{VCPKG_CMAKE_GENERATOR} "Ninja")
    # Also set CMAKE_GENERATOR for this project if not already set
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        message(STATUS "Setting CMAKE_GENERATOR to Ninja for MinGW build")
        set(CMAKE_GENERATOR "Ninja" CACHE STRING "CMake generator" FORCE)
    endif()
endif()

# DuckDB's extension distribution supports vcpkg. As such, dependencies can be added in ./vcpkg.json and then
# used in cmake with find_package. Feel free to remove or replace with other dependencies.
# Note that it should also be removed from vcpkg.json to prevent needlessly installing it..
# OpenSSL is only needed by the ADBC driver (loaded at runtime), not by our extension
find_package(OpenSSL QUIET)

# ADBC will be loaded dynamically at runtime
# Define the expected library name for each platform
if(WIN32)
    set(SNOWFLAKE_ADBC_LIB "libadbc_driver_snowflake.so")  # Yes, .so even on Windows
elseif(APPLE)
    set(SNOWFLAKE_ADBC_LIB "libadbc_driver_snowflake.so")  # .so on macOS too
else()
    set(SNOWFLAKE_ADBC_LIB "libadbc_driver_snowflake.so")  # .so on Linux
endif()

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

set(EXTENSION_SOURCES
    src/snowflake_extension.cpp
    src/snowflake_arrow_utils.cpp
    src/snowflake_secrets.cpp
    src/snowflake_secret_provider.cpp
    src/snowflake_scan.cpp
    src/snowflake_client.cpp
    src/snowflake_client_manager.cpp
    src/snowflake_config.cpp
    src/snowflake_types.cpp
    src/snowflake_query_builder.cpp
    src/snowflake_transaction.cpp
    src/storage/snowflake_storage.cpp
    src/storage/snowflake_catalog.cpp
    src/storage/snowflake_catalog_set.cpp
    src/storage/snowflake_schema_entry.cpp
    src/storage/snowflake_schema_set.cpp
    src/storage/snowflake_table_entry.cpp
    src/storage/snowflake_table_set.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      add_definitions(-DDEBUG_SNOWFLAKE)
endif()

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})
set_property(TARGET ${LOADABLE_EXTENSION_NAME} PROPERTY "EXTENSION_VERSION_SCRIPT" "TRUE")

# Link dependencies in both the static library and the loadable extension
# OpenSSL is optional - only link if found (ADBC driver brings its own)
if(OpenSSL_FOUND)
    target_link_libraries(${EXTENSION_NAME} 
        OpenSSL::SSL 
        OpenSSL::Crypto
    )
    target_link_libraries(${LOADABLE_EXTENSION_NAME} 
        OpenSSL::SSL 
        OpenSSL::Crypto
    )
    message(STATUS "Linking with OpenSSL")
else()
    message(STATUS "OpenSSL not found - skipping (ADBC driver provides SSL at runtime)")
endif()

# Add platform-specific libraries for dynamic loading
if(WIN32)
    # Windows doesn't need special libraries for dynamic loading (uses Windows API)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${EXTENSION_NAME} ${CMAKE_DL_LIBS})
    target_link_libraries(${LOADABLE_EXTENSION_NAME} ${CMAKE_DL_LIBS})
endif()

# Link filesystem library if needed (only on Linux/Unix, not Windows or macOS)
if(NOT APPLE AND NOT WIN32)
    target_link_libraries(${EXTENSION_NAME} stdc++fs)
    target_link_libraries(${LOADABLE_EXTENSION_NAME} stdc++fs)
endif()

# Define path to Snowflake ADBC shared library for dynamic loading
target_compile_definitions(${EXTENSION_NAME} PRIVATE 
    -DSNOWFLAKE_ADBC_LIB="${SNOWFLAKE_ADBC_LIB}"
    -DADBC_AVAILABLE=1  # ADBC is always available via dynamic loading
)
target_compile_definitions(${LOADABLE_EXTENSION_NAME} PRIVATE 
    -DSNOWFLAKE_ADBC_LIB="${SNOWFLAKE_ADBC_LIB}"
    -DADBC_AVAILABLE=1  # ADBC is always available via dynamic loading
)

# Platform-specific ADBC driver file name (all platforms use .so for ADBC)
set(ADBC_DRIVER_FILE "libadbc_driver_snowflake.so")
set(ADBC_DRIVER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/adbc_drivers/${ADBC_DRIVER_FILE}")

# Check if ADBC driver exists and we're not building for Wasm
if(NOT EXISTS "${ADBC_DRIVER_PATH}")
    message(WARNING "ADBC Snowflake driver not found at ${ADBC_DRIVER_PATH}. Run 'make download-adbc' to download it.")
elseif(EMSCRIPTEN)
    # Skip copying ADBC driver for Wasm builds - it's not supported
    message(STATUS "Skipping ADBC driver copy for Wasm build - driver not supported on this platform")
else()
    message(STATUS "Found ADBC Snowflake driver at ${ADBC_DRIVER_PATH}")
    
    # Copy ADBC driver to output directories after build
    add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ADBC_DRIVER_PATH}"
            "$<TARGET_FILE_DIR:${EXTENSION_NAME}>/${ADBC_DRIVER_FILE}"
        COMMENT "Copying ADBC Snowflake driver to extension directory"
    )
    
    # Also copy for the loadable extension
    add_custom_command(TARGET ${LOADABLE_EXTENSION_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ADBC_DRIVER_PATH}"
            "$<TARGET_FILE_DIR:${LOADABLE_EXTENSION_NAME}>/${ADBC_DRIVER_FILE}"
        COMMENT "Copying ADBC Snowflake driver to loadable extension directory"
    )
    
    # Copy to the main build directory as well
    add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ADBC_DRIVER_PATH}"
            "${CMAKE_BINARY_DIR}/${ADBC_DRIVER_FILE}"
        COMMENT "Copying ADBC Snowflake driver to build directory"
    )
    
    # On Windows, also copy to directories where DLLs are typically searched
    if(WIN32)
        add_custom_command(TARGET ${EXTENSION_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ADBC_DRIVER_PATH}"
                "${CMAKE_BINARY_DIR}/Release/${ADBC_DRIVER_FILE}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ADBC_DRIVER_PATH}"
                "${CMAKE_BINARY_DIR}/Debug/${ADBC_DRIVER_FILE}"
            COMMENT "Copying ADBC Snowflake driver to Windows configuration directories"
        )
    endif()
endif()

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")

# Also install the ADBC driver if it exists and we're not building for Wasm
if(EXISTS "${ADBC_DRIVER_PATH}" AND NOT EMSCRIPTEN)
    install(FILES "${ADBC_DRIVER_PATH}"
        DESTINATION "${INSTALL_LIB_DIR}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    
    # On Windows, also install to bin directory where DLLs are expected
    if(WIN32)
        install(FILES "${ADBC_DRIVER_PATH}"
            DESTINATION "bin"
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    endif()
endif()

# Add unit test for pushdown functionality (runs without Snowflake credentials)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(test_snowflake_pushdown_unit
        test/test_snowflake_pushdown_unit.cpp
    )
    target_link_libraries(test_snowflake_pushdown_unit
        ${EXTENSION_NAME}
        duckdb
    )
    target_include_directories(test_snowflake_pushdown_unit PRIVATE
        src/include
        ${CMAKE_SOURCE_DIR}/duckdb/src/include
    )
    
    # Add test target
    add_custom_target(run_pushdown_unit_tests
        COMMAND test_snowflake_pushdown_unit
        DEPENDS test_snowflake_pushdown_unit
        COMMENT "Running Snowflake pushdown unit tests"
    )
endif()
