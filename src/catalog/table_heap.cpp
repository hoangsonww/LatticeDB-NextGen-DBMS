#include "table_heap.h"
#include "../storage/table_page.h"
#include <unordered_map>
#include <vector>

namespace latticedb {

// Simple in-memory storage that actually works
static std::unordered_map<page_id_t, std::vector<Tuple>> table_data_;
static std::unordered_map<page_id_t, std::vector<bool>> tuple_deleted_;
static std::unordered_map<page_id_t, page_id_t> next_rid_slot_;

TableHeap::TableHeap(BufferPoolManager* buffer_pool_manager, LockManager* lock_manager,
                     LogManager* log_manager, page_id_t first_page_id)
    : buffer_pool_manager_(buffer_pool_manager), first_page_id_(first_page_id),
      lock_manager_(lock_manager), log_manager_(log_manager) {

  if (first_page_id_ == INVALID_PAGE_ID) {
    // Allocate a new page for this table
    auto page_guard = buffer_pool_manager_->new_page_guarded(&first_page_id_);
    if (page_guard.is_valid()) {
      // Initialize the page as a TablePage
      auto* page = page_guard.get();
      auto* table_page = reinterpret_cast<TablePage*>(page);
      table_page->init(first_page_id_, PAGE_SIZE, INVALID_PAGE_ID, log_manager_, nullptr);

      // Initialize in-memory structures
      table_data_[first_page_id_] = std::vector<Tuple>();
      tuple_deleted_[first_page_id_] = std::vector<bool>();
      next_rid_slot_[first_page_id_] = 0;

      // Mark page as dirty to ensure it gets written
      page_guard.mark_dirty();
    }
  } else {
    // Initialize structures for existing page if needed
    if (table_data_.find(first_page_id_) == table_data_.end()) {
      table_data_[first_page_id_] = std::vector<Tuple>();
      tuple_deleted_[first_page_id_] = std::vector<bool>();
      next_rid_slot_[first_page_id_] = 0;
    }
  }
}

bool TableHeap::insert_tuple(const Tuple& tuple, RID* rid, Transaction* txn) {
  (void)txn;
  auto& tuples = table_data_[first_page_id_];
  auto& deleted = tuple_deleted_[first_page_id_];

  // Find a deleted slot to reuse
  for (size_t i = 0; i < deleted.size(); i++) {
    if (deleted[i]) {
      tuples[i] = tuple;
      deleted[i] = false;
      *rid = RID(first_page_id_, i);
      return true;
    }
  }

  // No deleted slot, append
  tuples.push_back(tuple);
  deleted.push_back(false);
  *rid = RID(first_page_id_, tuples.size() - 1);

  // Also write to the actual page for persistence
  auto page_guard = buffer_pool_manager_->fetch_page(first_page_id_);
  if (page_guard.is_valid()) {
    auto* table_page = reinterpret_cast<TablePage*>(page_guard.get());
    RID page_rid;
    table_page->insert_tuple(tuple, &page_rid, txn, lock_manager_, log_manager_);
    page_guard.mark_dirty();
  }

  return true;
}

bool TableHeap::mark_delete(const RID& rid, Transaction* txn) {
  (void)txn;
  if (rid.page_id != first_page_id_)
    return false;

  auto& deleted = tuple_deleted_[first_page_id_];
  if (rid.slot_num >= deleted.size())
    return false;
  if (deleted[rid.slot_num])
    return false;

  deleted[rid.slot_num] = true;

  // Also mark in the actual page
  auto page_guard = buffer_pool_manager_->fetch_page(first_page_id_);
  if (page_guard.is_valid()) {
    auto* table_page = reinterpret_cast<TablePage*>(page_guard.get());
    table_page->mark_delete(rid, txn, lock_manager_, log_manager_);
    page_guard.mark_dirty();
  }

  return true;
}

bool TableHeap::update_tuple(const Tuple& tuple, const RID& rid, Transaction* txn) {
  (void)txn;
  if (rid.page_id != first_page_id_)
    return false;

  auto& tuples = table_data_[first_page_id_];
  auto& deleted = tuple_deleted_[first_page_id_];

  if (rid.slot_num >= tuples.size())
    return false;
  if (deleted[rid.slot_num])
    return false;

  tuples[rid.slot_num] = tuple;

  // Also update in the actual page
  auto page_guard = buffer_pool_manager_->fetch_page(first_page_id_);
  if (page_guard.is_valid()) {
    auto* table_page = reinterpret_cast<TablePage*>(page_guard.get());
    table_page->update_tuple(tuple, nullptr, rid, txn, lock_manager_, log_manager_);
    page_guard.mark_dirty();
  }

  return true;
}

void TableHeap::apply_delete(const RID& rid, Transaction* txn) {
  mark_delete(rid, txn);
}

void TableHeap::rollback_delete(const RID& rid, Transaction* txn) {
  (void)txn;
  if (rid.page_id != first_page_id_)
    return;

  auto& deleted = tuple_deleted_[first_page_id_];
  if (rid.slot_num < deleted.size()) {
    deleted[rid.slot_num] = false;
  }

  // Also rollback in the actual page
  auto page_guard = buffer_pool_manager_->fetch_page(rid.page_id);
  if (page_guard.is_valid()) {
    auto* table_page = reinterpret_cast<TablePage*>(page_guard.get());
    table_page->rollback_delete(rid, txn, lock_manager_, log_manager_);
    page_guard.mark_dirty();
  }
}

bool TableHeap::get_tuple(const RID& rid, Tuple* tuple, Transaction* txn) {
  (void)txn;
  if (rid.page_id != first_page_id_)
    return false;

  auto& tuples = table_data_[first_page_id_];
  auto& deleted = tuple_deleted_[first_page_id_];

  if (rid.slot_num >= tuples.size())
    return false;
  if (deleted[rid.slot_num])
    return false;

  *tuple = tuples[rid.slot_num];
  return true;
}

TableIterator TableHeap::begin(Transaction* txn) {
  (void)txn;
  auto& tuples = table_data_[first_page_id_];
  auto& deleted = tuple_deleted_[first_page_id_];

  for (size_t i = 0; i < tuples.size(); i++) {
    if (!deleted[i]) {
      return TableIterator(this, RID(first_page_id_, i));
    }
  }

  return TableIterator(this, RID());
}

TableIterator TableHeap::end() {
  return TableIterator(this, RID());
}

// TableIterator implementation
TableIterator& TableIterator::operator++() {
  if (!table_heap_ || rid_.page_id == INVALID_PAGE_ID) {
    return *this;
  }

  auto& tuples = table_data_[rid_.page_id];
  auto& deleted = tuple_deleted_[rid_.page_id];

  for (size_t i = rid_.slot_num + 1; i < tuples.size(); i++) {
    if (!deleted[i]) {
      rid_.slot_num = i;
      return *this;
    }
  }

  rid_ = RID();
  return *this;
}

Tuple TableIterator::operator*() {
  if (rid_.page_id == INVALID_PAGE_ID) {
    return Tuple();
  }

  Tuple tuple;
  table_heap_->get_tuple(rid_, &tuple, nullptr);
  return tuple;
}

bool TableIterator::operator==(const TableIterator& itr) const {
  return table_heap_ == itr.table_heap_ && rid_ == itr.rid_;
}

bool TableIterator::operator!=(const TableIterator& itr) const {
  return !(*this == itr);
}

bool TableIterator::is_end() const {
  return rid_.page_id == INVALID_PAGE_ID;
}

RID TableIterator::get_rid() const {
  return rid_;
}

} // namespace latticedb