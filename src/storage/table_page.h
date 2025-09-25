#pragma once

#include "../types/schema.h"
#include "../types/tuple.h"
#include "page.h"
#include <vector>

namespace latticedb {

class Transaction;
class LockManager;
class LogManager;

class TablePage : public Page {
public:
  static constexpr size_t OFFSET_PAGE_START = 0;
  static constexpr size_t OFFSET_NEXT_TUPLE_OFFSET = sizeof(uint32_t);
  static constexpr size_t OFFSET_NUM_TUPLES = sizeof(uint32_t) + sizeof(uint32_t);
  static constexpr size_t OFFSET_NUM_DELETED_TUPLES =
      sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);
  static constexpr size_t SIZE_TABLE_PAGE_HEADER =
      sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);

  static constexpr size_t OFFSET_FREE_SPACE = SIZE_TABLE_PAGE_HEADER;
  static constexpr size_t SIZE_TUPLE = sizeof(uint32_t) + sizeof(uint16_t);

private:
  uint32_t* GetPageStart() {
    return reinterpret_cast<uint32_t*>(get_data() + OFFSET_PAGE_START);
  }
  uint32_t* GetNextTupleOffset() {
    return reinterpret_cast<uint32_t*>(get_data() + OFFSET_NEXT_TUPLE_OFFSET);
  }
  uint32_t* GetNumTuples() {
    return reinterpret_cast<uint32_t*>(get_data() + OFFSET_NUM_TUPLES);
  }
  const uint32_t* GetNumTuples() const {
    return reinterpret_cast<const uint32_t*>(get_data() + OFFSET_NUM_TUPLES);
  }
  uint32_t* GetNumDeletedTuples() {
    return reinterpret_cast<uint32_t*>(get_data() + OFFSET_NUM_DELETED_TUPLES);
  }
  const uint32_t* GetNumDeletedTuples() const {
    return reinterpret_cast<const uint32_t*>(get_data() + OFFSET_NUM_DELETED_TUPLES);
  }

public:
  void init(uint32_t page_id, uint32_t page_size, uint32_t prev_page_id, LogManager* log_manager,
            Transaction* txn);

  bool insert_tuple(const Tuple& tuple, RID* rid, Transaction* txn, LockManager* lock_manager,
                    LogManager* log_manager);

  bool mark_delete(const RID& rid, Transaction* txn, LockManager* lock_manager,
                   LogManager* log_manager);

  bool update_tuple(const Tuple& new_tuple, Tuple* old_tuple, const RID& rid, Transaction* txn,
                    LockManager* lock_manager, LogManager* log_manager);

  bool get_tuple(const RID& rid, Tuple* tuple, Transaction* txn, LockManager* lock_manager,
                 LogManager* log_manager);

  bool get_tuple(const RID& rid, Tuple* tuple, Transaction* txn, LockManager* lock_manager);

  void rollback_delete(const RID& rid, Transaction* txn, LockManager* lock_manager,
                       LogManager* log_manager);

  bool get_first_tuple_rid(RID* first_rid);

  bool get_next_tuple_rid(const RID& cur_rid, RID* next_rid);

  uint32_t get_tuple_count() const {
    return *GetNumTuples();
  }

  uint32_t get_deleted_tuple_count() const {
    return *GetNumDeletedTuples();
  }

  uint32_t get_free_space_remaining() const;

private:
  static constexpr uint32_t TUPLE_DELETED_MASK = (1U << 31);
  static constexpr uint32_t TUPLE_SIZE_MASK = ((1U << 31) - 1);

  bool is_tuple_deleted(uint32_t tuple_offset) const;

  void set_tuple_deleted(uint32_t tuple_offset);

  uint32_t get_tuple_size(uint32_t tuple_offset) const;

  void set_tuple_size(uint32_t tuple_offset, uint32_t tuple_size);

  uint32_t get_tuple_offset_at_slot(uint32_t slot_num) const;

  void set_tuple_offset_at_slot(uint32_t slot_num, uint32_t tuple_offset);

  uint32_t find_free_slot();

  bool has_enough_space(uint32_t tuple_size) const;
};

class TablePageIterator {
private:
  TablePage* table_page_;
  RID rid_;
  Transaction* txn_;
  LockManager* lock_manager_;

public:
  TablePageIterator(TablePage* table_page, RID rid, Transaction* txn, LockManager* lock_manager);

  TablePageIterator(const TablePageIterator& other) = default;
  TablePageIterator& operator=(const TablePageIterator&) = delete;

  bool operator==(const TablePageIterator& other) const;
  bool operator!=(const TablePageIterator& other) const;

  const RID& operator*();
  TablePageIterator& operator++();

  bool is_end();
};

} // namespace latticedb
