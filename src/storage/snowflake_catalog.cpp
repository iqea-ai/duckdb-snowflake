#include "snowflake_debug.hpp"
#include "storage/snowflake_catalog.hpp"
#include "storage/snowflake_schema_entry.hpp"
#include "duckdb/storage/database_size.hpp"

namespace duckdb {
namespace snowflake {

SnowflakeCatalog::SnowflakeCatalog(AttachedDatabase &db_p, const SnowflakeConfig &config)
    : Catalog(db_p), client(SnowflakeClientManager::GetInstance().GetConnection(config)), schemas(*this, client) {
	DPRINT("SnowflakeCatalog constructor called\n");
	if (!client || !client->IsConnected()) {
		throw ConnectionException("Failed to connect to Snowflake");
	}
	DPRINT("SnowflakeCatalog connected successfully\n");
}

SnowflakeCatalog::~SnowflakeCatalog() {
	// Note: Consider adding option to allow connections to persist for multiple DETACH/ATTACH operations
	auto &client_manager = SnowflakeClientManager::GetInstance();
	client_manager.ReleaseConnection(client->GetConfig());
}

void SnowflakeCatalog::Initialize(bool load_builtin) {
	DPRINT("SnowflakeCatalog::Initialize called with load_builtin=%s\n", load_builtin ? "true" : "false");
}

void SnowflakeCatalog::ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) {
	DPRINT("SnowflakeCatalog::ScanSchemas called\n");
	schemas.Scan(context, [&](CatalogEntry &schema) {
		DPRINT("ScanSchemas callback for schema: %s\n", schema.name.c_str());
		callback(schema.Cast<SchemaCatalogEntry>());
	});
	DPRINT("SnowflakeCatalog::ScanSchemas completed\n");
}

optional_ptr<SchemaCatalogEntry> SnowflakeCatalog::LookupSchema(CatalogTransaction transaction,
                                                                const EntryLookupInfo &schema_lookup,
                                                                OnEntryNotFound if_not_found) {
	const auto &schema_name = schema_lookup.GetEntryName();

	auto found_entry = schemas.GetEntry(transaction.GetContext(), schema_name);
	if (!found_entry && if_not_found == OnEntryNotFound::THROW_EXCEPTION) {
		throw BinderException("Schema with name \"%s\" not found", schema_name);
	}
	return dynamic_cast<SchemaCatalogEntry *>(found_entry.get());
}

optional_ptr<CatalogEntry> SnowflakeCatalog::CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) {
	throw NotImplementedException("Snowflake catalog is read-only");
}

void SnowflakeCatalog::DropSchema(ClientContext &context, DropInfo &info) {
	throw NotImplementedException("Snowflake catalog is read-only");
}

DatabaseSize SnowflakeCatalog::GetDatabaseSize(ClientContext &context) {
	throw NotImplementedException("Snowflake catalog does not support getting database size");
}

bool SnowflakeCatalog::InMemory() {
	return false;
}

string SnowflakeCatalog::GetDBPath() {
	// Return a descriptive path for the Snowflake database
	const auto &config = client->GetConfig();
	return config.account + "." + config.database;
}

PhysicalOperator &SnowflakeCatalog::PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner,
                                                      LogicalCreateTable &op, PhysicalOperator &plan) {
	throw NotImplementedException("Snowflake catalog is read-only");
}

PhysicalOperator &SnowflakeCatalog::PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner,
                                               LogicalInsert &op, optional_ptr<PhysicalOperator> plan) {
	throw NotImplementedException("Snowflake catalog is read-only");
}

PhysicalOperator &SnowflakeCatalog::PlanDelete(ClientContext &context, PhysicalPlanGenerator &planner,
                                               LogicalDelete &op, PhysicalOperator &plan) {
	throw NotImplementedException("Snowflake catalog is read-only");
}

PhysicalOperator &SnowflakeCatalog::PlanUpdate(ClientContext &context, PhysicalPlanGenerator &planner,
                                               LogicalUpdate &op, PhysicalOperator &plan) {
	throw NotImplementedException("Snowflake catalog is read-only");
}

} // namespace snowflake
} // namespace duckdb
