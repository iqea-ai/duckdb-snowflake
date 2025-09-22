#include "duckdb.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "duckdb/planner/filter/null_filter.hpp"
#include "duckdb/planner/filter/in_filter.hpp"
#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "duckdb/common/enums/expression_type.hpp"

#include "snowflake_query_builder.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>

using namespace duckdb;
using namespace duckdb::snowflake;

// Unit tests for Snowflake Predicate Pushdown - Public API Only
// These tests run without external dependencies and focus on the public interface

void testBuildWhereClause() {
    std::cout << "Testing BuildWhereClause..." << std::endl;

    // Test single filter
    TableFilterSet filter_set;
    filter_set.filters[0] = make_uniq<ConstantFilter>(ExpressionType::COMPARE_EQUAL, Value::INTEGER(42));

    std::vector<std::string> column_names = {"id"};
    std::string result = SnowflakeQueryBuilder::BuildWhereClause(&filter_set, column_names);
    std::cout << "Single filter WHERE clause: " << result << std::endl;
    assert(result == "WHERE \"id\" = 42");

    // Test multiple filters
    filter_set.filters.clear();
    filter_set.filters[0] = make_uniq<ConstantFilter>(ExpressionType::COMPARE_EQUAL, Value::INTEGER(42));
    filter_set.filters[1] = make_uniq<ConstantFilter>(ExpressionType::COMPARE_GREATERTHAN, Value::INTEGER(0));

    column_names = {"id", "age"};
    result = SnowflakeQueryBuilder::BuildWhereClause(&filter_set, column_names);
    std::cout << "Multiple filters WHERE clause: " << result << std::endl;
    assert(result == "WHERE \"id\" = 42 AND \"age\" > 0");

    // Test empty filters
    filter_set.filters.clear();
    result = SnowflakeQueryBuilder::BuildWhereClause(&filter_set, column_names);
    std::cout << "Empty filters WHERE clause: " << result << std::endl;
    assert(result.empty());

    std::cout << "BuildWhereClause tests passed!" << std::endl;
}

void testBuildSelectClause() {
    std::cout << "Testing BuildSelectClause..." << std::endl;

    std::vector<std::string> projection_columns = {"id", "name", "age"};
    std::vector<std::string> all_columns = {"id", "name", "age", "address"};

    std::string result = SnowflakeQueryBuilder::BuildSelectClause(projection_columns, all_columns);
    std::cout << "Select clause result: " << result << std::endl;
    assert(result == "\"id\", \"name\", \"age\"");

    // Test with single column
    projection_columns = {"id"};
    result = SnowflakeQueryBuilder::BuildSelectClause(projection_columns, all_columns);
    std::cout << "Single column select: " << result << std::endl;
    assert(result == "\"id\"");

    std::cout << "BuildSelectClause tests passed!" << std::endl;
}

void testModifyQuery() {
    std::cout << "Testing ModifyQuery..." << std::endl;

    // Test with WHERE clause only
    std::string original_query = "SELECT * FROM my_table";
    std::string where_clause = "WHERE \"id\" > 100";
    std::string select_clause = "";

    std::string result = SnowflakeQueryBuilder::ModifyQuery(original_query, select_clause, where_clause);
    std::cout << "WHERE only modification: " << result << std::endl;
    assert(result == "SELECT * FROM my_table WHERE \"id\" > 100");

    // Test with SELECT clause only
    where_clause = "";
    select_clause = "\"id\", \"name\"";
    result = SnowflakeQueryBuilder::ModifyQuery(original_query, select_clause, where_clause);
    std::cout << "SELECT only modification: " << result << std::endl;
    assert(result == "SELECT \"id\", \"name\" FROM my_table");

    // Test with both WHERE and SELECT clauses
    where_clause = "WHERE \"id\" > 100";
    select_clause = "\"id\", \"name\"";
    result = SnowflakeQueryBuilder::ModifyQuery(original_query, select_clause, where_clause);
    std::cout << "Both WHERE and SELECT modification: " << result << std::endl;
    assert(result == "SELECT \"id\", \"name\" FROM my_table WHERE \"id\" > 100");

    std::cout << "ModifyQuery tests passed!" << std::endl;
}

int main() {
    std::cout << "Starting Snowflake Predicate Pushdown Unit Tests..." << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        testBuildWhereClause();
        std::cout << std::endl;

        testBuildSelectClause();
        std::cout << std::endl;

        testModifyQuery();
        std::cout << std::endl;

        std::cout << "==================================================" << std::endl;
        std::cout << "All unit tests passed successfully!" << std::endl;
        std::cout << "==================================================" << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown test failure occurred" << std::endl;
        return 1;
    }
}
