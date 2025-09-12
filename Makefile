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
		-DCMAKE_BUILD_TYPE=Release -G Ninja -S "./duckdb/" -B build/release
	cmake --build build/release --config Release -j8

# Custom debug target that downloads ADBC and builds  
.PHONY: debug-build
debug-build: download-adbc
	mkdir -p build/debug
	cmake -DEXTENSION_STATIC_BUILD=1 -DDUCKDB_EXTENSION_CONFIGS='${PROJ_DIR}extension_config.cmake' \
		-DCMAKE_BUILD_TYPE=Debug -G Ninja -S "./duckdb/" -B build/debug
	cmake --build build/debug --config Debug -j8

# CI-specific debug build with explicit Ninja and parallel jobs
.PHONY: ci-debug-build
ci-debug-build: download-adbc
	mkdir -p build/debug
	cmake -DEXTENSION_STATIC_BUILD=1 -DDUCKDB_EXTENSION_CONFIGS='${PROJ_DIR}extension_config.cmake' \
		-DCMAKE_BUILD_TYPE=Debug -G Ninja -S "./duckdb/" -B build/debug
	# Use fewer parallel jobs to avoid memory issues on GitHub runners
	cmake --build build/debug --config Debug -j4

# Package extension with ADBC driver
.PHONY: package-with-driver
package-with-driver: release
	@echo "Packaging extension with ADBC driver..."
	@python3 scripts/package_extension_with_driver.py --build-dir build/release
	@echo "Package created successfully!"

# Package debug build with ADBC driver
.PHONY: package-debug-with-driver
package-debug-with-driver: debug
	@echo "Packaging debug extension with ADBC driver..."
	@python3 scripts/package_extension_with_driver.py --build-dir build/debug
	@echo "Debug package created successfully!"

# Validate existing packages
.PHONY: validate-packages
validate-packages:
	@echo "Validating existing packages..."
	@python3 scripts/package_extension_with_driver.py --validate-only

# Test the packaged extension (uses existing package if available)
.PHONY: test-package
test-package:
	@echo "Testing packaged extension..."
	@if [ ! -f build/packages/snowflake-extension-*.zip ]; then \
		echo "No package found. Creating one first..."; \
		make package-with-driver; \
	fi
	@mkdir -p test_package
	@cd test_package && unzip -q ../build/packages/snowflake-extension-*.zip
	@echo "Package extracted successfully!"
	@echo "To test manually:"
	@echo "1. cd test_package"
	@echo "2. Copy 'snowflake' directory to your DuckDB extensions location"
	@echo "3. Run: LOAD 'snowflake/snowflake.duckdb_extension'"

# Test existing package without rebuilding
.PHONY: test-existing-package
test-existing-package:
	@echo "Testing existing packaged extension..."
	@if [ ! -f build/packages/snowflake-extension-*.zip ]; then \
		echo "No package found at build/packages/snowflake-extension-*.zip"; \
		echo "Run 'make package-with-driver' or 'make package-debug-with-driver' first"; \
		exit 1; \
	fi
	@mkdir -p test_package
	@cd test_package && unzip -q ../build/packages/snowflake-extension-*.zip
	@echo "Package extracted successfully!"
	@echo "Package contents:"
	@ls -la test_package/snowflake/
	@echo ""
	@echo "To test manually:"
	@echo "1. cd test_package"
	@echo "2. Copy 'snowflake' directory to your DuckDB extensions location"
	@echo "3. Run: LOAD 'snowflake/snowflake.duckdb_extension'"

# Snowflake-specific test target
.PHONY: test-snowflake
test-snowflake: debug-build
	@echo "Running Snowflake extension tests..."
	@if [ -z "$$SNOWFLAKE_ACCOUNT" ] || [ -z "$$SNOWFLAKE_USERNAME" ] || [ -z "$$SNOWFLAKE_PASSWORD" ] || [ -z "$$SNOWFLAKE_DATABASE" ]; then \
		echo "Error: Snowflake environment variables not set. Please set:"; \
		echo "  SNOWFLAKE_ACCOUNT"; \
		echo "  SNOWFLAKE_USERNAME"; \
		echo "  SNOWFLAKE_PASSWORD"; \
		echo "  SNOWFLAKE_DATABASE"; \
		exit 1; \
	fi
	./build/debug/test/unittest "$(PROJ_DIR)test/sql/snowflake_*.test"
