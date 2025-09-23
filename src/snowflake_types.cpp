#include "snowflake_types.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception/conversion_exception.hpp"

namespace duckdb {
namespace snowflake {
LogicalType SnowflakeTypeToLogicalType(const std::string &snowflake_type_str) {
	string normalized_type = StringUtil::Upper(snowflake_type_str);
	normalized_type = StringUtil::Replace(normalized_type, " ", "");

	auto paren_pos = normalized_type.find('(');
	string base_type = normalized_type.substr(0, paren_pos);

	if (base_type.find("INT") != std::string::npos) {
		if (base_type == "TINYINT") {
			return LogicalType::TINYINT;
		} else if (base_type == "SMALLINT") {
			return LogicalType::SMALLINT;
		} else if (base_type == "BIGINT") {
			return LogicalType::BIGINT;
		}
		return LogicalType::INTEGER;
	}

	if (base_type == "VARCHAR" || base_type == "STRING") {
		return LogicalType::VARCHAR;
	}

	if (base_type == "BOOLEAN") {
		return LogicalType::BOOLEAN;
	}

	// Floating point types
	if (base_type == "FLOAT" || base_type == "FLOAT4" || base_type == "REAL") {
		return LogicalType::FLOAT;
	}

	if (base_type == "DOUBLE" || base_type == "FLOAT8" || base_type == "DOUBLEPRECISION") {
		return LogicalType::DOUBLE;
	}

	// DECIMAL/NUMERIC types
	if (base_type == "DECIMAL" || base_type == "NUMERIC") {
		if (paren_pos == std::string::npos) {
			// Default precision and scale for DECIMAL without parameters
			return LogicalType::DECIMAL(18, 0);
		}

		auto close_paren_pos = normalized_type.find(')');
		if (close_paren_pos == std::string::npos) {
			throw InvalidInputException("Expected closing ')' for DECIMAL type: " + snowflake_type_str);
		}

		auto params = normalized_type.substr(paren_pos + 1, close_paren_pos - paren_pos - 1);
		auto comma_pos = params.find(',');

		int precision = 18; // Default precision
		int scale = 0;      // Default scale

		if (comma_pos == std::string::npos) {
			// Only precision specified
			try {
				precision = std::stoi(params);
			} catch (const std::exception &e) {
				throw ConversionException("Invalid precision '%s' in type: %s", params, snowflake_type_str);
			}
		} else {
			// Both precision and scale specified
			std::string precision_str = params.substr(0, comma_pos);
			std::string scale_str = params.substr(comma_pos + 1);

			try {
				precision = std::stoi(precision_str);
			} catch (const std::exception &e) {
				throw ConversionException("Invalid precision '%s' in type: %s", precision_str, snowflake_type_str);
			}

			try {
				scale = std::stoi(scale_str);
			} catch (const std::exception &e) {
				throw ConversionException("Invalid scale '%s' in type: %s", scale_str, snowflake_type_str);
			}
		}

		if (precision < 1 || precision > 38) {
			throw ConversionException("DECIMAL precision %d out of range (1-38)", precision);
		}
		if (scale < 0 || scale > precision) {
			throw ConversionException("DECIMAL scale %d invalid (must be 0-%d)", scale, precision);
		}

		return LogicalType::DECIMAL(static_cast<uint8_t>(precision), static_cast<uint8_t>(scale));
	}

	// NUMBER type (Snowflake's variable precision numeric)
	if (base_type == "NUMBER") {
		if (paren_pos == std::string::npos) {
			// Note: Consider creating user setting to specify behavior. DOUBLE is relatively safe but loses precision for
			// large decimals, and could possibly be losing out on performance.
			return LogicalType::DOUBLE;
		}

		auto close_paren_pos = normalized_type.find(')');
		if (close_paren_pos == std::string::npos) {
			throw InvalidInputException("Expected closing ')' for NUMBER type: " + snowflake_type_str);
		}

		auto params = normalized_type.substr(paren_pos + 1, close_paren_pos - paren_pos - 1);

		auto comma_pos = params.find(',');
		int temp_precision = 0;
		int temp_scale = 0;

		if (comma_pos == std::string::npos) {
			try {
				temp_precision = std::stoi(params);
			} catch (const std::exception &e) {
				throw ConversionException("Invalid precision '%s' in type: %s", params, snowflake_type_str);
			}
		} else {
			std::string precision_str = params.substr(0, comma_pos);
			std::string scale_str = params.substr(comma_pos + 1);

			try {
				temp_precision = std::stoi(precision_str);
			} catch (const std::exception &e) {
				throw ConversionException("Invalid precision '%s' in type: %s", precision_str, snowflake_type_str);
			}

			try {
				temp_scale = std::stoi(scale_str);
			} catch (const std::exception &e) {
				throw ConversionException("Invalid scale '%s' in type: %s", scale_str, snowflake_type_str);
			}
		}

		if (temp_precision < 1 || temp_precision > 38) {
			throw ConversionException("NUMBER precision %d out of range (1-38)", temp_precision);
		}
		if (temp_scale < 0 || temp_scale > temp_precision) {
			throw ConversionException("NUMBER scale %d invalid (must be 0-%d)", temp_scale, temp_precision);
		}

		uint8_t precision = static_cast<uint8_t>(temp_precision);
		uint8_t scale = static_cast<uint8_t>(temp_scale);

		return ConvertNumber(precision, scale);
	}

	// Note: Additional type mappings (TIMESTAMP_NTZ, TIMESTAMP_TZ, etc.) can be added as needed

	return LogicalType::VARCHAR; // fallback type
}

LogicalType ConvertNumber(uint8_t precision, uint8_t scale) {
	// For integer types (scale = 0), map to appropriate integer type based on precision
	if (scale == 0) {
		if (precision <= 2) {
			return LogicalType::TINYINT; // -128 to 127
		}
		if (precision <= 4) {
			return LogicalType::SMALLINT; // -32,768 to 32,767
		}
		if (precision <= 9) {
			return LogicalType::INTEGER; // -2,147,483,648 to 2,147,483,647
		}
		if (precision <= 18) {
			return LogicalType::BIGINT; // -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
		}
	}

	// For any type with scale > 0 or precision > 18, use DECIMAL to maintain exact precision
	// This ensures no loss of precision for financial/monetary calculations
	return LogicalType::DECIMAL(precision, scale);
}
} // namespace snowflake
} // namespace duckdb
