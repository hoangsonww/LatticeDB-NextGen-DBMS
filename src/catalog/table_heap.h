#pragma once

#include "../buffer/buffer_pool_manager.h"
#include "../concurrency/lock_manager.h"
#include "../recovery/log_manager.h"
#include "../storage/table_page.h"
#include "../transaction/transaction.h"

namespace latticedb {

class TableIterator; // forward declare iterator type used below

class TableHeap {
private:
  BufferPoolManager* buffer_pool_manager_;
  page_id_t first_page_id_;
  LockManager* lock_manager_;
  LogManager* log_manager_;

public:
  TableHeap(BufferPoolManager* buffer_pool_manager, LockManager* lock_manager,
            LogManager* log_manager, page_id_t first_page_id);

  ~TableHeap() = default;

  bool insert_tuple(const Tuple& tuple, RID* rid, Transaction* txn);

  bool mark_delete(const RID& rid, Transaction* txn);

  bool update_tuple(const Tuple& tuple, const RID& rid, Transaction* txn);

  void apply_delete(const RID& rid, Transaction* txn);

  void rollback_delete(const RID& rid, Transaction* txn);

  bool get_tuple(const RID& rid, Tuple* tuple, Transaction* txn);

  page_id_t get_first_page_id() const {
    return first_page_id_;
  }

  BufferPoolManager* get_buffer_pool_manager() const {
    return buffer_pool_manager_;
  }

  TableIterator begin(Transaction* txn);

  TableIterator end();

private:
  page_id_t get_next_page_id(page_id_t page_id, BufferPoolManager* buffer_pool_manager,
                             Transaction* txn, LockManager* lock_manager);

  void update_tuple_in_place(TablePage* page, const Tuple& tuple, const RID& rid, Transaction* txn,
                             LockManager* lock_manager, LogManager* log_manager);

  bool insert_tuple_into_page(TablePage* page, const Tuple& tuple, RID* rid, Transaction* txn,
                              LockManager* lock_manager, LogManager* log_manager);
};

class TableIterator {
private:
  TableHeap* table_heap_;
  RID rid_;

public:
  TableIterator(TableHeap* table_heap, RID rid) : table_heap_(table_heap), rid_(rid) {}

  TableIterator(const TableIterator& other) = default;

  ~TableIterator() = default;

  bool operator==(const TableIterator& itr) const;

  bool operator!=(const TableIterator& itr) const;

  Tuple operator*();

  TableIterator& operator++();

  bool is_end() const;

  RID get_rid() const;
};

} // namespace latticedb
