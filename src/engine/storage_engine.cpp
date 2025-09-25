#include "storage_engine.h"
#include "../catalog/catalog_manager.h"

namespace latticedb {

// RowStoreEngine
bool RowStoreEngine::insert(table_oid_t table_oid, const Tuple& tuple, RID* rid, Transaction* txn) {
  ensure_table_heap(table_oid);
  return table_heaps_[table_oid]->insert_tuple(tuple, rid, txn);
}

bool RowStoreEngine::update(table_oid_t table_oid, const RID& rid, const Tuple& tuple,
                            Transaction* txn) {
  ensure_table_heap(table_oid);
  return table_heaps_[table_oid]->update_tuple(tuple, rid, txn);
}

bool RowStoreEngine::delete_tuple(table_oid_t table_oid, const RID& rid, Transaction* txn) {
  ensure_table_heap(table_oid);
  return table_heaps_[table_oid]->mark_delete(rid, txn);
}

bool RowStoreEngine::select(table_oid_t table_oid, const RID& rid, Tuple* tuple, Transaction* txn) {
  ensure_table_heap(table_oid);
  return table_heaps_[table_oid]->get_tuple(rid, tuple, txn);
}

std::unique_ptr<StorageEngine::Iterator> RowStoreEngine::scan(table_oid_t table_oid,
                                                              Transaction* txn) {
  ensure_table_heap(table_oid);
  class It : public StorageEngine::Iterator {
    TableHeap* heap_;
    latticedb::TableIterator iter_;
    Transaction* txn_;
    RID last_rid_;

  public:
    It(TableHeap* h, latticedb::TableIterator it, Transaction* txn)
        : heap_(h), iter_(it), txn_(txn), last_rid_() {}
    bool has_next() override {
      return !iter_.is_end();
    }
    Tuple next() override {
      Tuple t = *iter_;
      last_rid_ = iter_.get_rid();
      ++iter_;
      return t;
    }
    RID get_rid() override {
      return last_rid_;
    }
  };
  return std::make_unique<It>(table_heaps_[table_oid].get(), table_heaps_[table_oid]->begin(txn),
                              txn);
}

std::unique_ptr<StorageEngine::Iterator>
RowStoreEngine::index_scan(index_oid_t /*index_oid*/, const Tuple& /*key*/, Transaction* /*txn*/) {
  return nullptr;
}

bool RowStoreEngine::create_table(const std::string& table_name, const Schema& schema,
                                  Transaction* txn) {
  auto oid = catalog_->create_table(table_name, schema, txn);
  ensure_table_heap(oid);
  return true;
}

bool RowStoreEngine::drop_table(const std::string& table_name, Transaction* txn) {
  auto* meta = catalog_->get_table(table_name);
  if (!meta)
    return false;
  table_heaps_.erase(meta->get_oid());
  return catalog_->drop_table(table_name, txn);
}

bool RowStoreEngine::create_index(const std::string& /*index_name*/,
                                  const std::string& /*table_name*/,
                                  const std::vector<std::string>& /*column_names*/,
                                  Transaction* /*txn*/) {
  return false;
}
bool RowStoreEngine::drop_index(const std::string& /*index_name*/, Transaction* /*txn*/) {
  return false;
}
void RowStoreEngine::flush() {}

TableHeap* RowStoreEngine::get_table_heap(table_oid_t table_oid) {
  ensure_table_heap(table_oid);
  return table_heaps_[table_oid].get();
}

void RowStoreEngine::ensure_table_heap(table_oid_t table_oid) {
  if (table_heaps_.count(table_oid) == 0) {
    table_heaps_[table_oid] =
        std::make_unique<TableHeap>(buffer_pool_manager_, lock_manager_, log_manager_, table_oid);
  }
}

// ColumnStoreEngine (not implemented; return false/no-op)
bool ColumnStoreEngine::insert(table_oid_t, const Tuple&, RID*, Transaction*) {
  return false;
}
bool ColumnStoreEngine::update(table_oid_t, const RID&, const Tuple&, Transaction*) {
  return false;
}
bool ColumnStoreEngine::delete_tuple(table_oid_t, const RID&, Transaction*) {
  return false;
}
bool ColumnStoreEngine::select(table_oid_t, const RID&, Tuple*, Transaction*) {
  return false;
}
std::unique_ptr<StorageEngine::Iterator> ColumnStoreEngine::scan(table_oid_t, Transaction*) {
  return nullptr;
}
std::unique_ptr<StorageEngine::Iterator> ColumnStoreEngine::index_scan(index_oid_t, const Tuple&,
                                                                       Transaction*) {
  return nullptr;
}
bool ColumnStoreEngine::create_table(const std::string&, const Schema&, Transaction*) {
  return false;
}
bool ColumnStoreEngine::drop_table(const std::string&, Transaction*) {
  return false;
}
bool ColumnStoreEngine::create_index(const std::string&, const std::string&,
                                     const std::vector<std::string>&, Transaction*) {
  return false;
}
bool ColumnStoreEngine::drop_index(const std::string&, Transaction*) {
  return false;
}
void ColumnStoreEngine::flush() {}

} // namespace latticedb
