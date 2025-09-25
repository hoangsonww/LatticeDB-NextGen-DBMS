#pragma once

#include "../buffer/buffer_pool_manager.h"
#include "../catalog/catalog_manager.h"
#include "../catalog/table_heap.h"
#include "../transaction/transaction.h"
#include "../types/schema.h"
#include "../types/tuple.h"
#include <memory>
#include <string>
#include <vector>

namespace latticedb {

// No forward declarations to avoid name clashes with nested Iterator

enum class StorageEngineType { ROW_STORE, COLUMN_STORE, LSM_TREE, IN_MEMORY };

class StorageEngine {
public:
  virtual ~StorageEngine() = default;
  class Iterator; // forward declare nested iterator

  virtual bool insert(table_oid_t table_oid, const Tuple& tuple, RID* rid, Transaction* txn) = 0;

  virtual bool update(table_oid_t table_oid, const RID& rid, const Tuple& tuple,
                      Transaction* txn) = 0;

  virtual bool delete_tuple(table_oid_t table_oid, const RID& rid, Transaction* txn) = 0;

  virtual bool select(table_oid_t table_oid, const RID& rid, Tuple* tuple, Transaction* txn) = 0;

  virtual std::unique_ptr<Iterator> scan(table_oid_t table_oid, Transaction* txn) = 0;

  virtual std::unique_ptr<Iterator> index_scan(index_oid_t index_oid, const Tuple& key,
                                               Transaction* txn) = 0;

  virtual bool create_table(const std::string& table_name, const Schema& schema,
                            Transaction* txn) = 0;

  virtual bool drop_table(const std::string& table_name, Transaction* txn) = 0;

  virtual bool create_index(const std::string& index_name, const std::string& table_name,
                            const std::vector<std::string>& column_names, Transaction* txn) = 0;

  virtual bool drop_index(const std::string& index_name, Transaction* txn) = 0;

  virtual StorageEngineType get_type() const = 0;

  virtual void flush() = 0;

public:
  class Iterator {
  public:
    virtual ~Iterator() = default;
    virtual bool has_next() = 0;
    virtual Tuple next() = 0;
    virtual RID get_rid() = 0;
  };
};

class RowStoreEngine : public StorageEngine {
private:
  BufferPoolManager* buffer_pool_manager_;
  Catalog* catalog_;
  LockManager* lock_manager_;
  LogManager* log_manager_;

  std::unordered_map<table_oid_t, std::unique_ptr<TableHeap>> table_heaps_;

public:
  RowStoreEngine(BufferPoolManager* bpm, Catalog* catalog, LockManager* lock_manager,
                 LogManager* log_manager)
      : buffer_pool_manager_(bpm), catalog_(catalog), lock_manager_(lock_manager),
        log_manager_(log_manager) {}

  ~RowStoreEngine() override = default;

  bool insert(table_oid_t table_oid, const Tuple& tuple, RID* rid, Transaction* txn) override;

  bool update(table_oid_t table_oid, const RID& rid, const Tuple& tuple, Transaction* txn) override;

  bool delete_tuple(table_oid_t table_oid, const RID& rid, Transaction* txn) override;

  bool select(table_oid_t table_oid, const RID& rid, Tuple* tuple, Transaction* txn) override;

  std::unique_ptr<Iterator> scan(table_oid_t table_oid, Transaction* txn) override;

  std::unique_ptr<Iterator> index_scan(index_oid_t index_oid, const Tuple& key,
                                       Transaction* txn) override;

  bool create_table(const std::string& table_name, const Schema& schema, Transaction* txn) override;

  bool drop_table(const std::string& table_name, Transaction* txn) override;

  bool create_index(const std::string& index_name, const std::string& table_name,
                    const std::vector<std::string>& column_names, Transaction* txn) override;

  bool drop_index(const std::string& index_name, Transaction* txn) override;

  StorageEngineType get_type() const override {
    return StorageEngineType::ROW_STORE;
  }

  void flush() override;

private:
  TableHeap* get_table_heap(table_oid_t table_oid);

  void ensure_table_heap(table_oid_t table_oid);

  // Minimal implementation does not expose nested iterators in header
};

class ColumnStoreEngine : public StorageEngine {
public:
  ColumnStoreEngine(BufferPoolManager* bpm, Catalog* catalog) {
    (void)bpm;
    (void)catalog;
  }

  bool insert(table_oid_t table_oid, const Tuple& tuple, RID* rid, Transaction* txn) override;
  bool update(table_oid_t table_oid, const RID& rid, const Tuple& tuple, Transaction* txn) override;
  bool delete_tuple(table_oid_t table_oid, const RID& rid, Transaction* txn) override;
  bool select(table_oid_t table_oid, const RID& rid, Tuple* tuple, Transaction* txn) override;
  std::unique_ptr<Iterator> scan(table_oid_t table_oid, Transaction* txn) override;
  std::unique_ptr<Iterator> index_scan(index_oid_t index_oid, const Tuple& key,
                                       Transaction* txn) override;
  bool create_table(const std::string& table_name, const Schema& schema, Transaction* txn) override;
  bool drop_table(const std::string& table_name, Transaction* txn) override;
  bool create_index(const std::string& index_name, const std::string& table_name,
                    const std::vector<std::string>& column_names, Transaction* txn) override;
  bool drop_index(const std::string& index_name, Transaction* txn) override;
  StorageEngineType get_type() const override {
    return StorageEngineType::COLUMN_STORE;
  }
  void flush() override;
};

} // namespace latticedb
