#pragma once

#include "../buffer/buffer_pool_manager.h"
#include "../index/b_plus_tree.h"
#include "../transaction/transaction.h"
#include "../types/schema.h"
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace latticedb {

// Forward declarations
class Index;
class LogManager;
class IndexIterator;
class KeyComparator;
template <size_t N> class GenericKey;
template <size_t N> class GenericComparator;

// Index types
enum class IndexType { BPlusTreeType, HashTableType };
using column_oid_t = uint32_t;

class TableMetadata {
private:
  std::shared_ptr<Schema> schema_;
  std::string name_;
  table_oid_t oid_;
  page_id_t first_page_id_;

public:
  TableMetadata(std::shared_ptr<Schema> schema, std::string name, table_oid_t oid,
                page_id_t first_page_id)
      : schema_(std::move(schema)), name_(std::move(name)), oid_(oid),
        first_page_id_(first_page_id) {}

  const Schema& get_schema() const {
    return *schema_;
  }
  std::shared_ptr<Schema> get_schema_ptr() const {
    return schema_;
  }
  const std::string& get_name() const {
    return name_;
  }
  table_oid_t get_oid() const {
    return oid_;
  }
  page_id_t get_first_page_id() const {
    return first_page_id_;
  }
  void set_first_page_id(page_id_t first_page_id) {
    first_page_id_ = first_page_id;
  }
};

class IndexInfo {
private:
  std::shared_ptr<Schema> key_schema_;
  std::string name_;
  std::unique_ptr<Index> index_;
  index_oid_t index_oid_;
  table_oid_t table_oid_;
  std::vector<uint32_t> key_attrs_;

public:
  IndexInfo(std::shared_ptr<Schema> key_schema, std::string name, std::unique_ptr<Index> index,
            index_oid_t index_oid, table_oid_t table_oid, std::vector<uint32_t> key_attrs)
      : key_schema_(std::move(key_schema)), name_(std::move(name)), index_(std::move(index)),
        index_oid_(index_oid), table_oid_(table_oid), key_attrs_(std::move(key_attrs)) {}

  const Schema& get_key_schema() const {
    return *key_schema_;
  }
  std::shared_ptr<Schema> get_key_schema_ptr() const {
    return key_schema_;
  }
  const std::string& get_name() const {
    return name_;
  }
  Index* get_index() const {
    return index_.get();
  }
  index_oid_t get_index_oid() const {
    return index_oid_;
  }
  table_oid_t get_table_oid() const {
    return table_oid_;
  }
  const std::vector<uint32_t>& get_key_attrs() const {
    return key_attrs_;
  }
};

class Index {
public:
  virtual ~Index() = default;

  virtual void insert_entry(const Tuple& key, RID rid, Transaction* txn) = 0;

  virtual void delete_entry(const Tuple& key, RID rid, Transaction* txn) = 0;

  virtual void scan_key(const Tuple& key, std::vector<RID>* result, Transaction* txn) = 0;

  virtual IndexIterator scan_all() = 0;

  virtual IndexIterator scan_key(const Tuple& key) = 0;

  virtual IndexIterator scan_range(const Tuple& lower_key, const Tuple& upper_key,
                                   bool lower_inclusive = true, bool upper_inclusive = true) = 0;

protected:
  std::string index_name_;
  BufferPoolManager* buffer_pool_manager_;
  // KeyComparator comparator_; // not used in minimal implementation
  std::shared_ptr<Schema> key_schema_;
  table_oid_t table_oid_;
};

class BPlusTreeIndex : public Index {
private:
  std::unique_ptr<BPlusTree<GenericKey<8>, RID, GenericComparator<8>>> container_;

public:
  BPlusTreeIndex(std::shared_ptr<Schema> key_schema, BufferPoolManager* buffer_pool_manager);

  void insert_entry(const Tuple& key, RID rid, Transaction* txn) override;

  void delete_entry(const Tuple& key, RID rid, Transaction* txn) override;

  void scan_key(const Tuple& key, std::vector<RID>* result, Transaction* txn) override;

  IndexIterator scan_all() override;

  IndexIterator scan_key(const Tuple& key) override;

  IndexIterator scan_range(const Tuple& lower_key, const Tuple& upper_key,
                           bool lower_inclusive = true, bool upper_inclusive = true) override;

private:
  GenericKey<8> tuple_to_key(const Tuple& tuple);
};

class Catalog {
private:
  std::unordered_map<std::string, table_oid_t> table_names_;
  std::unordered_map<table_oid_t, std::unique_ptr<TableMetadata>> tables_;
  std::unordered_map<std::string, index_oid_t> index_names_;
  std::unordered_map<index_oid_t, std::unique_ptr<IndexInfo>> indexes_;

  std::atomic<table_oid_t> next_table_oid_;
  std::atomic<index_oid_t> next_index_oid_;

  BufferPoolManager* bpm_;
  LockManager* lock_manager_;
  LogManager* log_manager_;

  mutable std::shared_mutex catalog_mutex_;

public:
  Catalog(BufferPoolManager* bpm, LockManager* lock_manager, LogManager* log_manager)
      : next_table_oid_(0), next_index_oid_(0), bpm_(bpm), lock_manager_(lock_manager),
        log_manager_(log_manager) {
    // Load existing catalog from disk if it exists
    load_catalog();
  }

  ~Catalog() = default;

  table_oid_t create_table(const std::string& table_name, const Schema& schema, Transaction* txn);

  TableMetadata* get_table(const std::string& table_name);
  TableMetadata* get_table(table_oid_t table_oid);

  index_oid_t create_index(const std::string& index_name, const std::string& table_name,
                           const Schema& key_schema, const std::vector<uint32_t>& key_attrs,
                           Transaction* txn, IndexType index_type = IndexType::BPlusTreeType);

  IndexInfo* get_index(const std::string& index_name);
  IndexInfo* get_index(index_oid_t index_oid);

  std::vector<IndexInfo*> get_table_indexes(const std::string& table_name);
  std::vector<IndexInfo*> get_table_indexes(table_oid_t table_oid);

  bool drop_table(const std::string& table_name, Transaction* txn);

  bool drop_index(const std::string& index_name, Transaction* txn);

  std::vector<std::string> get_table_names() const;

private:
  table_oid_t get_next_table_oid() {
    return next_table_oid_++;
  }
  index_oid_t get_next_index_oid() {
    return next_index_oid_++;
  }

  void create_table_heap(table_oid_t table_oid);

  std::unique_ptr<Index> create_index_instance(IndexType index_type,
                                               const std::shared_ptr<Schema>& key_schema);

  // Catalog persistence
  void persist_catalog();
  void load_catalog();
};

} // namespace latticedb
