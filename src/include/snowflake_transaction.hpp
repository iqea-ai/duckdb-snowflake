#pragma once

#include "duckdb/transaction/transaction_manager.hpp"
#include "duckdb/transaction/transaction.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include <mutex>

namespace duckdb {
namespace snowflake {

class SnowflakeTransaction : public Transaction {
public:
	SnowflakeTransaction(TransactionManager &manager, ClientContext &context);
	~SnowflakeTransaction() override = default;
};

class SnowflakeTransactionManager : public TransactionManager {
public:
	explicit SnowflakeTransactionManager(AttachedDatabase &db);
	~SnowflakeTransactionManager() override = default;

	Transaction &StartTransaction(ClientContext &context) override;
	ErrorData CommitTransaction(ClientContext &context, Transaction &transaction) override;
	void RollbackTransaction(Transaction &transaction) override;
	void Checkpoint(ClientContext &context, bool force = false) override;

private:
	mutex transaction_lock;
	reference_map_t<Transaction, unique_ptr<Transaction>> transactions;
};

// Storage extension transaction manager factory function
unique_ptr<TransactionManager> SnowflakeCreateTransactionManager(StorageExtensionInfo *storage_info,
                                                                 AttachedDatabase &db, Catalog &catalog);

} // namespace snowflake
} // namespace duckdb
