PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=snowflake
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Check if ADBC driver exists before including the main makefile
ADBC_DRIVER_EXISTS := $(shell test -f adbc_drivers/libadbc_driver_snowflake.so && echo 1 || echo 0)

ifeq ($(ADBC_DRIVER_EXISTS),0)
$(info ADBC driver not found. Downloading...)
$(shell bash scripts/download_adbc_driver.sh >/dev/null 2>&1)
endif

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

# Download pre-built ADBC driver if not present
.PHONY: download-adbc
download-adbc:
	@echo "Checking for ADBC Snowflake driver..."
	@bash scripts/download_adbc_driver.sh

# Custom release target that downloads ADBC and builds
.PHONY: release-build
release-build: download-adbc
	mkdir -p build/release
	cmake -DEXTENSION_STATIC_BUILD=1 -DDUCKDB_EXTENSION_CONFIGS='${PROJ_DIR}extension_config.cmake' \
		-DCMAKE_BUILD_TYPE=Release -S "./duckdb/" -B build/release
	cmake --build build/release --config Release

# Custom debug target that downloads ADBC and builds  
.PHONY: debug-build
debug-build: download-adbc
	mkdir -p build/debug
	cmake -DEXTENSION_STATIC_BUILD=1 -DDUCKDB_EXTENSION_CONFIGS='${PROJ_DIR}extension_config.cmake' \
		-DCMAKE_BUILD_TYPE=Debug -S "./duckdb/" -B build/debug
	cmake --build build/debug --config Debug
