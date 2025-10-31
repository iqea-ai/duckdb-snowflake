#include "snowflake_debug.hpp"
#include "storage/snowflake_catalog.hpp"
#include "storage/snowflake_schema_entry.hpp"
#include "duckdb/storage/database_size.hpp"

namespace duckdb {
namespace snowflake {

SnowflakeCatalog::SnowflakeCatalog(AttachedDatabase &db_p,
                                   const SnowflakeConfig &config,
                                   const SnowflakeOptions &options_p)
    : Catalog(db_p),
      client(SnowflakeClientManager::GetInstance().GetConnection(config)),
      schemas(*this, client), options(options_p) {
  DPRINT("SnowflakeCatalog constructor called\n");
  if (!client || !client->IsConnected()) {
    throw ConnectionException("Failed to connect to Snowflake");
  }
  DPRINT("SnowflakeCatalog connected successfully with enable_pushdown=%s\n",
         options.enable_pushdown ? "true" : "false");
}

SnowflakeCatalog::~SnowflakeCatalog() {
  // TODO consider adding option to allow connections to persist if user wants
  // to DETACH and ATTACH multiple times
  auto &client_manager = SnowflakeClientManager::GetInstance();
  client_manager.ReleaseConnection(client->GetConfig());
}

void SnowflakeCatalog::Initialize(bool load_builtin) {
  DPRINT("SnowflakeCatalog::Initialize called with load_builtin=%s\n",
         load_builtin ? "true" : "false");
}

void SnowflakeCatalog::ScanSchemas(
    ClientContext &context,
    std::function<void(SchemaCatalogEntry &)> callback) {
  DPRINT("SnowflakeCatalog::ScanSchemas called\n");
  schemas.Scan(context, [&](CatalogEntry &schema) {
    DPRINT("ScanSchemas callback for schema: %s\n", schema.name.c_str());
    callback(schema.Cast<SchemaCatalogEntry>());
  });
  DPRINT("SnowflakeCatalog::ScanSchemas completed\n");
}

optional_ptr<SchemaCatalogEntry>
SnowflakeCatalog::LookupSchema(CatalogTransaction transaction,
                               const EntryLookupInfo &schema_lookup,
                               OnEntryNotFound if_not_found) {
  const auto &schema_name = schema_lookup.GetEntryName();

  // If the schema name contains a dot, the user likely attempted a 4-part path
  // e.g., sf.ANALYTICS_DATABASE.information_schema.tables.
  // With ATTACH, the database is fixed by the secret; queries must be
  // catalog.schema.table.
  if (schema_name.find('.') != string::npos) {
    const auto &attached_db = client->GetConfig().database;
    const auto &alias = GetName();
    throw BinderException(
        "Invalid path: you are trying to reference '%s' while a database is "
        "already attached (\"%s\").\n"
        "Use exactly three parts in SELECT statements: catalog.schema.table, "
        "where:\n"
        "  - catalog: your ATTACH alias (e.g., '%s')\n"
        "  - schema: the Snowflake schema\n"
        "  - table:  the table name\n"
        "Example: SELECT * FROM %s.information_schema.tables;",
        schema_name.c_str(), attached_db.c_str(), alias.c_str(), alias.c_str());
  }

  auto found_entry = schemas.GetEntry(transaction.GetContext(), schema_name);
  if (!found_entry && if_not_found == OnEntryNotFound::THROW_EXCEPTION) {
    const auto &attached_db = client->GetConfig().database;
    throw BinderException(
        "Schema '%s' not found in attached database '%s'. To query a different "
        "database, create a separate ATTACH or use snowflake_query().",
        schema_name.c_str(), attached_db.c_str());
  }
  return dynamic_cast<SchemaCatalogEntry *>(found_entry.get());
}

optional_ptr<CatalogEntry>
SnowflakeCatalog::CreateSchema(CatalogTransaction transaction,
                               CreateSchemaInfo &info) {
  throw NotImplementedException("Snowflake catalog is read-only");
}

void SnowflakeCatalog::DropSchema(ClientContext &context, DropInfo &info) {
  throw NotImplementedException("Snowflake catalog is read-only");
}

DatabaseSize SnowflakeCatalog::GetDatabaseSize(ClientContext &context) {
  throw NotImplementedException(
      "Snowflake catalog does not support getting database size");
}

bool SnowflakeCatalog::InMemory() { return false; }

string SnowflakeCatalog::GetDBPath() {
  // Return a descriptive path for the Snowflake database
  const auto &config = client->GetConfig();
  return config.account + "." + config.database;
}

PhysicalOperator &SnowflakeCatalog::PlanCreateTableAs(
    ClientContext &context, PhysicalPlanGenerator &planner,
    LogicalCreateTable &op, PhysicalOperator &plan) {
  throw NotImplementedException("Snowflake catalog is read-only");
}

PhysicalOperator &
SnowflakeCatalog::PlanInsert(ClientContext &context,
                             PhysicalPlanGenerator &planner, LogicalInsert &op,
                             optional_ptr<PhysicalOperator> plan) {
  throw NotImplementedException("Snowflake catalog is read-only");
}

PhysicalOperator &SnowflakeCatalog::PlanDelete(ClientContext &context,
                                               PhysicalPlanGenerator &planner,
                                               LogicalDelete &op,
                                               PhysicalOperator &plan) {
  throw NotImplementedException("Snowflake catalog is read-only");
}

PhysicalOperator &SnowflakeCatalog::PlanUpdate(ClientContext &context,
                                               PhysicalPlanGenerator &planner,
                                               LogicalUpdate &op,
                                               PhysicalOperator &plan) {
  throw NotImplementedException("Snowflake catalog is read-only");
}

} // namespace snowflake
} // namespace duckdb
