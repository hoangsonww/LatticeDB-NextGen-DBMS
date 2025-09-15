#pragma once

#include "../storage/table_page.h"
#include "../buffer/buffer_pool_manager.h"
#include "../concurrency/lock_manager.h"
#include "../recovery/log_manager.h"
#include "../transaction/transaction.h"

namespace latticedb {

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

    page_id_t get_first_page_id() const { return first_page_id_; }

    TableIterator begin(Transaction* txn);

    TableIterator end();

private:
    page_id_t get_next_page_id(page_id_t page_id, BufferPoolManager* buffer_pool_manager,
                              Transaction* txn, LockManager* lock_manager);

    void update_tuple_in_place(TablePage* page, const Tuple& tuple, const RID& rid,
                              Transaction* txn, LockManager* lock_manager, LogManager* log_manager);

    bool insert_tuple_into_page(TablePage* page, const Tuple& tuple, RID* rid,
                               Transaction* txn, LockManager* lock_manager, LogManager* log_manager);
};

class TableIterator {
private:
    TableHeap* table_heap_;
    RID rid_;
    Transaction* txn_;

public:
    TableIterator(TableHeap* table_heap, RID rid, Transaction* txn)
        : table_heap_(table_heap), rid_(rid), txn_(txn) {}

    TableIterator(const TableIterator& other) = default;

    ~TableIterator() = default;

    bool operator==(const TableIterator& itr) const {
        return table_heap_ == itr.table_heap_ && rid_.page_id == itr.rid_.page_id &&
               rid_.slot_num == itr.rid_.slot_num;
    }

    bool operator!=(const TableIterator& itr) const { return !(*this == itr); }

    const RID& operator*() { return rid_; }

    RID* operator->() { return &rid_; }

    TableIterator& operator++();

private:
    BufferPoolManager* buffer_pool_manager_;
    LockManager* lock_manager_;
};

}