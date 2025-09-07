#include "storage/snowflake_schema_entry.hpp"
#include "storage/snowflake_table_set.hpp"
#include "storage/snowflake_schema_entry.hpp"

namespace duckdb {
namespace snowflake {

SnowflakeSchemaEntry::SnowflakeSchemaEntry(Catalog &catalog, const string &schema_name, CreateSchemaInfo &info,
                                           const shared_ptr<SnowflakeClient> &client)
    : SchemaCatalogEntry(catalog, info), client(client) {
	name = schema_name;
	tables = make_uniq<SnowflakeTableSet>(*this, client, schema_name);
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::LookupEntry(CatalogTransaction transaction,
                                                             const EntryLookupInfo &lookup_info) {
	if (!CatalogTypeIsSupported(lookup_info.GetCatalogType())) {
		return nullptr;
	}

	return tables->GetEntry(transaction.GetContext(), lookup_info.GetEntryName());
}

void SnowflakeSchemaEntry::Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) {
	throw NotImplementedException("Snowflake does not support context-less scan");
}

void SnowflakeSchemaEntry::Scan(ClientContext &context, CatalogType type,
                                const std::function<void(CatalogEntry &)> &callback) {
	if (!CatalogTypeIsSupported(type)) {
		return;
	}

	tables->Scan(context, callback);
}

bool SnowflakeSchemaEntry::CatalogTypeIsSupported(CatalogType type) {
	switch (type) {
	case CatalogType::TABLE_ENTRY:
		return true;
	case CatalogType::INDEX_ENTRY:
	case CatalogType::TYPE_ENTRY:
	case CatalogType::VIEW_ENTRY:
	default:
		return false;
	}
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateIndex(CatalogTransaction transaction, CreateIndexInfo &info,
                                                             TableCatalogEntry &table) {
	throw NotImplementedException("CreateIndex is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateFunction(CatalogTransaction transaction,
                                                                CreateFunctionInfo &info) {
	throw NotImplementedException("CreateFunction is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateTable(CatalogTransaction transaction,
                                                             BoundCreateTableInfo &info) {
	throw NotImplementedException("CreateTable is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateView(CatalogTransaction transaction, CreateViewInfo &info) {
	throw NotImplementedException("CreateView is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateSequence(CatalogTransaction transaction,
                                                                CreateSequenceInfo &info) {
	throw NotImplementedException("CreateSequence is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateTableFunction(CatalogTransaction transaction,
                                                                     CreateTableFunctionInfo &info) {
	throw NotImplementedException("CreateTableFunction is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateCopyFunction(CatalogTransaction transaction,
                                                                    CreateCopyFunctionInfo &info) {
	throw NotImplementedException("CreateCopyFunction is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreatePragmaFunction(CatalogTransaction transaction,
                                                                      CreatePragmaFunctionInfo &info) {
	throw NotImplementedException("CreatePragmaFunction is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateCollation(CatalogTransaction transaction,
                                                                 CreateCollationInfo &info) {
	throw NotImplementedException("CreateCollation is not supported for Snowflake schemas");
}

optional_ptr<CatalogEntry> SnowflakeSchemaEntry::CreateType(CatalogTransaction transaction, CreateTypeInfo &info) {
	throw NotImplementedException("CreateType is not supported for Snowflake schemas");
}

void SnowflakeSchemaEntry::DropEntry(ClientContext &context, DropInfo &info) {
	throw NotImplementedException("DropEntry is not supported for Snowflake schemas");
}

void SnowflakeSchemaEntry::Alter(CatalogTransaction transaction, AlterInfo &info) {
	throw NotImplementedException("Alter is not supported for Snowflake schemas");
}

} // namespace snowflake
} // namespace duckdb
