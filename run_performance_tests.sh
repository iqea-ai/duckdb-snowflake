#!/bin/bash
cd /Users/sathvikkurapati/Downloads/duckdb-snowflake
export SNOWFLAKE_ACCOUNT='wdbniqg-ct63033'
export SNOWFLAKE_USERNAME='ptandra'
export SNOWFLAKE_PASSWORD='LM7qsuw6XGbuBQK'
export SNOWFLAKE_DATABASE='SNOWFLAKE_SAMPLE_DATA'

echo "=== PERFORMANCE TESTING STARTED AT $(date) ===" > performance_results.log
echo "Testing on TPCH_SF100.LINEITEM dataset (600M rows, 16.6GB)" >> performance_results.log
echo "" >> performance_results.log

# Test 1: WITH PUSHDOWN - Equality Filter
echo "Test 1: WITH PUSHDOWN - Equality Filter" >> performance_results.log
echo "Query: SELECT COUNT(*) FROM LINEITEM WHERE L_ORDERKEY = 1" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY = 1;" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Test 2: WITH PUSHDOWN - Range Filter
echo "Test 2: WITH PUSHDOWN - Range Filter" >> performance_results.log
echo "Query: SELECT COUNT(*) FROM LINEITEM WHERE L_ORDERKEY BETWEEN 1 AND 100" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY BETWEEN 1 AND 100;" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Test 3: WITH PUSHDOWN - IN Filter
echo "Test 3: WITH PUSHDOWN - IN Filter" >> performance_results.log
echo "Query: SELECT COUNT(*) FROM LINEITEM WHERE L_ORDERKEY IN (1, 2, 3, 4, 5)" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY IN (1, 2, 3, 4, 5);" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Test 4: WITH PUSHDOWN - Projection
echo "Test 4: WITH PUSHDOWN - Projection" >> performance_results.log
echo "Query: SELECT L_ORDERKEY, L_PARTKEY, L_SUPPKEY FROM LINEITEM WHERE L_ORDERKEY = 1" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT L_ORDERKEY, L_PARTKEY, L_SUPPKEY FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY = 1;" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Now test WITHOUT PUSHDOWN
export SNOWFLAKE_DISABLE_PUSHDOWN='true'
echo "=== TESTING WITHOUT PUSHDOWN ===" >> performance_results.log
echo "Pushdown disabled via SNOWFLAKE_DISABLE_PUSHDOWN=true" >> performance_results.log
echo "" >> performance_results.log

# Test 5: WITHOUT PUSHDOWN - Equality Filter
echo "Test 5: WITHOUT PUSHDOWN - Equality Filter" >> performance_results.log
echo "Query: SELECT COUNT(*) FROM LINEITEM WHERE L_ORDERKEY = 1" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY = 1;" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Test 6: WITHOUT PUSHDOWN - Range Filter
echo "Test 6: WITHOUT PUSHDOWN - Range Filter" >> performance_results.log
echo "Query: SELECT COUNT(*) FROM LINEITEM WHERE L_ORDERKEY BETWEEN 1 AND 100" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY BETWEEN 1 AND 100;" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Test 7: WITHOUT PUSHDOWN - IN Filter
echo "Test 7: WITHOUT PUSHDOWN - IN Filter" >> performance_results.log
echo "Query: SELECT COUNT(*) FROM LINEITEM WHERE L_ORDERKEY IN (1, 2, 3, 4, 5)" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT COUNT(*) FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY IN (1, 2, 3, 4, 5);" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

# Test 8: WITHOUT PUSHDOWN - Projection
echo "Test 8: WITHOUT PUSHDOWN - Projection" >> performance_results.log
echo "Query: SELECT L_ORDERKEY, L_PARTKEY, L_SUPPKEY FROM LINEITEM WHERE L_ORDERKEY = 1" >> performance_results.log
echo "Starting at $(date)" >> performance_results.log
time (echo "LOAD snowflake; CREATE SECRET snowflake_secret (TYPE snowflake, ACCOUNT 'wdbniqg-ct63033', USER 'ptandra', PASSWORD 'LM7qsuw6XGbuBQK', DATABASE 'SNOWFLAKE_SAMPLE_DATA', WAREHOUSE 'COMPUTE_WH'); SELECT L_ORDERKEY, L_PARTKEY, L_SUPPKEY FROM snowflake_scan('SELECT * FROM SNOWFLAKE_SAMPLE_DATA.TPCH_SF100.LINEITEM', 'snowflake_secret') WHERE L_ORDERKEY = 1;" | ./build/debug/duckdb > /dev/null 2>&1) 2>> performance_results.log
echo "Completed at $(date)" >> performance_results.log
echo "" >> performance_results.log

echo "=== PERFORMANCE TESTING COMPLETED AT $(date) ===" >> performance_results.log
echo "Results saved to performance_results.log" >> performance_results.log
