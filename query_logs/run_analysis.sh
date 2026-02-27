#!/bin/bash

# Snowflake Query Log Analysis Pipeline
# Runs all analysis steps and generates classification results

set -e  # Exit on error

echo "================================================"
echo "Snowflake Query Log Analysis for Caching Strategy"
echo "================================================"
echo ""

# Check for required environment variables
if [ -z "$SNOWFLAKE_ACCOUNT" ] || [ -z "$SNOWFLAKE_USER" ] || [ -z "$SNOWFLAKE_PASSWORD" ]; then
    echo "Error: Required environment variables not set"
    echo ""
    echo "Please set the following:"
    echo "  export SNOWFLAKE_ACCOUNT=your_account"
    echo "  export SNOWFLAKE_USER=your_user"
    echo "  export SNOWFLAKE_PASSWORD=your_password"
    echo "  export SNOWFLAKE_WAREHOUSE=your_warehouse  # Optional"
    echo ""
    exit 1
fi

# Set default warehouse if not specified
if [ -z "$SNOWFLAKE_WAREHOUSE" ]; then
    echo "Note: SNOWFLAKE_WAREHOUSE not set, using default 'COMPUTE_WH'"
    export SNOWFLAKE_WAREHOUSE="COMPUTE_WH"
fi

# Database file
DB_FILE="query_analysis.db"

echo "Using DuckDB database: $DB_FILE"
echo "Snowflake Account: $SNOWFLAKE_ACCOUNT"
echo "Snowflake User: $SNOWFLAKE_USER"
echo "Snowflake Warehouse: $SNOWFLAKE_WAREHOUSE"
echo ""

# Step 1: Extract query history
echo "Step 1/3: Extracting query history from Snowflake..."
echo "This may take several minutes depending on your query volume..."
duckdb "$DB_FILE" < 01_extract_query_history.sql
echo "✓ Query history extracted"
echo ""

# Step 2: Analyze queries
echo "Step 2/3: Analyzing query patterns and computing metrics..."
duckdb "$DB_FILE" < 02_analyze_queries.sql
echo "✓ Query metrics computed"
echo ""

# Step 3: Classify queries
echo "Step 3/3: Classifying queries into caching tiers..."
duckdb "$DB_FILE" < 03_classify_caching_strategy.sql
echo "✓ Query classification complete"
echo ""

# Summary
echo "================================================"
echo "Analysis Complete!"
echo "================================================"
echo ""
echo "Output files generated:"
echo "  - query_history.parquet         : Raw query history"
echo "  - query_metrics.parquet/csv     : Aggregated metrics per query pattern"
echo "  - query_classification.parquet/csv : Final tier assignments"
echo "  - $DB_FILE                      : DuckDB database with all tables"
echo ""
echo "To explore results interactively:"
echo "  duckdb $DB_FILE"
echo ""
echo "To view top cache candidates:"
echo "  duckdb $DB_FILE -c \"SELECT query_pattern, execution_count, ROUND(avg_execution_seconds, 2) as avg_sec, ROUND(estimated_result_mb, 2) as result_mb, ROUND(cache_priority_score, 2) as priority FROM query_classification WHERE caching_tier = 'CACHE' ORDER BY cache_priority_score DESC LIMIT 10;\""
echo ""
echo "To view classification summary:"
echo "  duckdb $DB_FILE -c \"SELECT caching_tier, COUNT(*) as patterns, SUM(execution_count) as executions, ROUND(SUM(total_warehouse_cost_credits), 2) as cost_credits, ROUND(SUM(potential_cache_savings_credits), 2) as savings FROM query_classification GROUP BY caching_tier;\""
echo ""
