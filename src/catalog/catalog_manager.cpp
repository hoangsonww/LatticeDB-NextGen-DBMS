#include <shared_mutex>

#include "catalog_manager.h"
#include "../common/exception.h"
#include "table_heap.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace latticedb {

// A simple non-templated index implementation (string-keyed)
// Provide the opaque IndexIterator type declared in the header
class IndexIterator {};

class SimpleIndex : public Index {
private:
  std::unordered_map<std::string, std::vector<RID>> map_;

  std::string tuple_to_key(const Tuple& tuple) const {
    std::string key;
    const auto& vals = tuple.get_values();
    for (size_t i = 0; i < vals.size(); ++i) {
      if (i)
        key.push_back('|');
      key += vals[i].to_string();
    }
    return key;
  }

public:
  SimpleIndex(std::shared_ptr<Schema> key_schema, BufferPoolManager* bpm) {
    key_schema_ = std::move(key_schema);
    buffer_pool_manager_ = bpm;
  }

  void insert_entry(const Tuple& key, RID rid, Transaction*) override {
    map_[tuple_to_key(key)].push_back(rid);
  }
  void delete_entry(const Tuple& key, RID rid, Transaction*) override {
    auto it = map_.find(tuple_to_key(key));
    if (it == map_.end())
      return;
    auto& v = it->second;
    v.erase(std::remove(v.begin(), v.end(), rid), v.end());
    if (v.empty())
      map_.erase(it);
  }
  void scan_key(const Tuple& key, std::vector<RID>* result, Transaction*) override {
    auto it = map_.find(tuple_to_key(key));
    if (it != map_.end() && result)
      *result = it->second;
  }

  latticedb::IndexIterator scan_all() override {
    return {};
  }
  latticedb::IndexIterator scan_key(const Tuple&) override {
    return {};
  }
  latticedb::IndexIterator scan_range(const Tuple&, const Tuple&, bool, bool) override {
    return {};
  }
};

table_oid_t Catalog::create_table(const std::string& table_name, const Schema& schema,
                                  Transaction* txn) {
  std::unique_lock<std::shared_mutex> lk(catalog_mutex_);
  if (table_names_.count(table_name)) {
    throw CatalogException("Table already exists: " + table_name);
  }

  // Create a TableHeap to allocate the first page
  TableHeap temp_heap(bpm_, lock_manager_, log_manager_, INVALID_PAGE_ID);
  page_id_t first_page_id = temp_heap.get_first_page_id();

  table_oid_t oid = get_next_table_oid();
  auto meta = std::make_unique<TableMetadata>(std::make_shared<Schema>(schema), table_name, oid,
                                              first_page_id);
  table_names_[table_name] = oid;
  tables_[oid] = std::move(meta);

  // Persist catalog changes
  persist_catalog();

  return oid;
}

TableMetadata* Catalog::get_table(const std::string& table_name) {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  auto itn = table_names_.find(table_name);
  if (itn == table_names_.end())
    return nullptr;
  auto it = tables_.find(itn->second);
  return it == tables_.end() ? nullptr : it->second.get();
}

TableMetadata* Catalog::get_table(table_oid_t table_oid) {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  auto it = tables_.find(table_oid);
  return it == tables_.end() ? nullptr : it->second.get();
}

index_oid_t Catalog::create_index(const std::string& index_name, const std::string& table_name,
                                  const Schema& key_schema, const std::vector<uint32_t>& key_attrs,
                                  Transaction* /*txn*/, IndexType /*index_type*/) {
  std::unique_lock<std::shared_mutex> lk(catalog_mutex_);
  if (index_names_.count(index_name)) {
    throw CatalogException("Index already exists: " + index_name);
  }
  auto itn = table_names_.find(table_name);
  if (itn == table_names_.end())
    throw CatalogException("Table not found for index: " + table_name);
  index_oid_t oid = get_next_index_oid();
  index_names_[index_name] = oid;
  auto key_schema_ptr = std::make_shared<Schema>(key_schema);
  std::unique_ptr<Index> index_ptr(new SimpleIndex(key_schema_ptr, bpm_));
  indexes_[oid] = std::make_unique<IndexInfo>(key_schema_ptr, index_name, std::move(index_ptr), oid,
                                              itn->second, key_attrs);
  return oid;
}

IndexInfo* Catalog::get_index(const std::string& index_name) {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  auto it = index_names_.find(index_name);
  if (it == index_names_.end())
    return nullptr;
  auto ii = indexes_.find(it->second);
  return ii == indexes_.end() ? nullptr : ii->second.get();
}

IndexInfo* Catalog::get_index(index_oid_t index_oid) {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  auto ii = indexes_.find(index_oid);
  return ii == indexes_.end() ? nullptr : ii->second.get();
}

std::vector<IndexInfo*> Catalog::get_table_indexes(const std::string& table_name) {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  std::vector<IndexInfo*> v;
  auto itn = table_names_.find(table_name);
  if (itn == table_names_.end())
    return v;
  for (auto& kv : indexes_)
    if (kv.second->get_table_oid() == itn->second)
      v.push_back(kv.second.get());
  return v;
}

std::vector<IndexInfo*> Catalog::get_table_indexes(table_oid_t table_oid) {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  std::vector<IndexInfo*> v;
  for (auto& kv : indexes_)
    if (kv.second->get_table_oid() == table_oid)
      v.push_back(kv.second.get());
  return v;
}

bool Catalog::drop_table(const std::string& table_name, Transaction* /*txn*/) {
  std::unique_lock<std::shared_mutex> lk(catalog_mutex_);
  auto itn = table_names_.find(table_name);
  if (itn == table_names_.end())
    return false;
  tables_.erase(itn->second);
  table_names_.erase(itn);

  // Persist catalog changes
  persist_catalog();

  return true;
}

bool Catalog::drop_index(const std::string& index_name, Transaction* /*txn*/) {
  std::unique_lock<std::shared_mutex> lk(catalog_mutex_);
  auto it = index_names_.find(index_name);
  if (it == index_names_.end())
    return false;
  indexes_.erase(it->second);
  index_names_.erase(it);
  return true;
}

std::vector<std::string> Catalog::get_table_names() const {
  std::shared_lock<std::shared_mutex> lk(catalog_mutex_);
  std::vector<std::string> v;
  v.reserve(table_names_.size());
  for (auto& kv : table_names_)
    v.push_back(kv.first);
  std::sort(v.begin(), v.end());
  return v;
}

void Catalog::persist_catalog() {
  // Page 0 is reserved for catalog metadata
  // Format: [num_tables][table1_data][table2_data]...
  // Table data: [name_len][name][oid][first_page_id][schema_data]

  // Try to fetch catalog page first
  auto page_guard = bpm_->fetch_page(CATALOG_PAGE_ID);
  if (!page_guard.is_valid()) {
    // If doesn't exist, allocate it
    page_id_t new_page_id;
    page_guard = bpm_->new_page_guarded(&new_page_id);
    if (!page_guard.is_valid()) {
      return;
    }
  }

  Page* catalog_page = page_guard.get();

  char* data = catalog_page->get_data();
  uint32_t offset = 0;

  // Write number of tables
  uint32_t num_tables = tables_.size();
  memcpy(data + offset, &num_tables, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  // Write each table's metadata
  for (const auto& [oid, table_meta] : tables_) {
    // Write table name
    std::string name = table_meta->get_name();
    uint32_t name_len = name.length();
    memcpy(data + offset, &name_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(data + offset, name.c_str(), name_len);
    offset += name_len;

    // Write table OID
    memcpy(data + offset, &oid, sizeof(table_oid_t));
    offset += sizeof(table_oid_t);

    // Write first page ID
    page_id_t first_page = table_meta->get_first_page_id();
    memcpy(data + offset, &first_page, sizeof(page_id_t));
    offset += sizeof(page_id_t);

    // Write schema (simplified - just column count and basic info)
    const Schema& schema = table_meta->get_schema();
    uint32_t col_count = schema.column_count();
    memcpy(data + offset, &col_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    for (uint32_t i = 0; i < col_count; i++) {
      const Column& col = schema.get_column(i);

      // Column name
      std::string col_name = col.name();
      uint32_t col_name_len = col_name.length();
      memcpy(data + offset, &col_name_len, sizeof(uint32_t));
      offset += sizeof(uint32_t);
      memcpy(data + offset, col_name.c_str(), col_name_len);
      offset += col_name_len;

      // Column type
      ValueType type = col.type();
      memcpy(data + offset, &type, sizeof(ValueType));
      offset += sizeof(ValueType);

      // Column length (for VARCHAR)
      uint32_t length = col.length();
      memcpy(data + offset, &length, sizeof(uint32_t));
      offset += sizeof(uint32_t);

      // Nullable flag
      bool nullable = col.is_nullable();
      memcpy(data + offset, &nullable, sizeof(bool));
      offset += sizeof(bool);
    }

    if (offset >= PAGE_SIZE - 1024) {
      // Too much metadata for one page - would need multi-page catalog
      break;
    }
  }

  // Write next OIDs
  table_oid_t next_table = next_table_oid_.load();
  memcpy(data + PAGE_SIZE - 16, &next_table, sizeof(table_oid_t));

  index_oid_t next_index = next_index_oid_.load();
  memcpy(data + PAGE_SIZE - 8, &next_index, sizeof(index_oid_t));

  page_guard.mark_dirty();
}

void Catalog::load_catalog() {
  // Try to load catalog from page 0
  PageGuard page_guard;
  try {
    page_guard = bpm_->fetch_page(CATALOG_PAGE_ID);
    if (!page_guard.is_valid()) {
      // No catalog exists yet
      return;
    }
  } catch (...) {
    // Ignore errors during catalog loading on startup
    return;
  }

  Page* catalog_page = page_guard.get();

  char* data = catalog_page->get_data();
  uint32_t offset = 0;

  // Read number of tables
  uint32_t num_tables;
  memcpy(&num_tables, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  if (num_tables == 0 || num_tables > 1000) {
    // Likely uninitialized page
    return;
  }

  // Read each table's metadata
  for (uint32_t t = 0; t < num_tables; t++) {
    // Read table name
    uint32_t name_len;
    memcpy(&name_len, data + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (name_len > 256)
      break; // Sanity check

    std::string name(data + offset, name_len);
    offset += name_len;

    // Read table OID
    table_oid_t oid;
    memcpy(&oid, data + offset, sizeof(table_oid_t));
    offset += sizeof(table_oid_t);

    // Read first page ID
    page_id_t first_page;
    memcpy(&first_page, data + offset, sizeof(page_id_t));
    offset += sizeof(page_id_t);

    // Read schema
    uint32_t col_count;
    memcpy(&col_count, data + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    std::vector<Column> columns;
    for (uint32_t i = 0; i < col_count; i++) {
      // Column name
      uint32_t col_name_len;
      memcpy(&col_name_len, data + offset, sizeof(uint32_t));
      offset += sizeof(uint32_t);

      std::string col_name(data + offset, col_name_len);
      offset += col_name_len;

      // Column type
      ValueType type;
      memcpy(&type, data + offset, sizeof(ValueType));
      offset += sizeof(ValueType);

      // Column length
      uint32_t length;
      memcpy(&length, data + offset, sizeof(uint32_t));
      offset += sizeof(uint32_t);

      // Nullable flag
      bool nullable;
      memcpy(&nullable, data + offset, sizeof(bool));
      offset += sizeof(bool);

      columns.emplace_back(col_name, type, length, nullable);
    }

    // Create table metadata
    auto schema = std::make_shared<Schema>(columns);
    auto meta = std::make_unique<TableMetadata>(schema, name, oid, first_page);

    table_names_[name] = oid;
    tables_[oid] = std::move(meta);
  }

  // Read next OIDs
  table_oid_t next_table;
  memcpy(&next_table, data + PAGE_SIZE - 16, sizeof(table_oid_t));
  next_table_oid_ = next_table;

  index_oid_t next_index;
  memcpy(&next_index, data + PAGE_SIZE - 8, sizeof(index_oid_t));
  next_index_oid_ = next_index;
}

} // namespace latticedb
