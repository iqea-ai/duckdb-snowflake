#include "snowflake_transaction.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {
namespace snowflake {

SnowflakeTransaction::SnowflakeTransaction(TransactionManager &manager, ClientContext &context)
    : Transaction(manager, context) {
}

SnowflakeTransactionManager::SnowflakeTransactionManager(AttachedDatabase &db) : TransactionManager(db) {
}

Transaction &SnowflakeTransactionManager::StartTransaction(ClientContext &context) {
	// Create a new transaction object (avoid recursive call to Transaction::Get)
	auto transaction = make_uniq<SnowflakeTransaction>(*this, context);
	auto &result = *transaction;
	lock_guard<mutex> lock(transaction_lock);
	transactions[result] = std::move(transaction);
	return result;
}

ErrorData SnowflakeTransactionManager::CommitTransaction(ClientContext &context, Transaction &transaction) {
	// For read-only external database, remove the transaction from our map
	lock_guard<mutex> lock(transaction_lock);
	transactions.erase(transaction);
	return ErrorData();
}

void SnowflakeTransactionManager::RollbackTransaction(Transaction &transaction) {
	// For read-only external database, remove the transaction from our map
	lock_guard<mutex> lock(transaction_lock);
	transactions.erase(transaction);
}

void SnowflakeTransactionManager::Checkpoint(ClientContext &context, bool force) {
	// No local storage to checkpoint for external database
	// This is a no-op for Snowflake
}

unique_ptr<TransactionManager> SnowflakeCreateTransactionManager(optional_ptr<StorageExtensionInfo> storage_info,
                                                                 AttachedDatabase &db, Catalog &catalog) {
	return make_uniq<SnowflakeTransactionManager>(db);
}

} // namespace snowflake
} // namespace duckdb
