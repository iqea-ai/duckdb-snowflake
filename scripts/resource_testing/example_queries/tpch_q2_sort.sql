-- Large sort operation
-- Memory tag: ORDER_BY (for sort buffers)
SELECT * FROM orders
ORDER BY o_totalprice DESC, o_orderdate, o_custkey
LIMIT 1000000;









