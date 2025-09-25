// Implementation for table_page.h
#include "table_page.h"
#include "../concurrency/lock_manager.h"
#include "../recovery/log_manager.h"
#include "../transaction/transaction.h"
#include <cstring>

namespace latticedb {

// Helpers to access the slot directory at the end of page
static inline uint32_t* slot_ptr(char* base, uint32_t slot_num) {
  return reinterpret_cast<uint32_t*>(base + PAGE_SIZE - (slot_num + 1) * sizeof(uint32_t));
}

// Compute the number of allocated slots (used + deleted)
static inline uint32_t slot_high_water(const uint32_t num_tuples, const uint32_t num_deleted) {
  return num_tuples + num_deleted;
}

void TablePage::init(uint32_t page_id, uint32_t /*page_size*/, uint32_t /*prev_page_id*/,
                     LogManager* /*log_manager*/, Transaction* /*txn*/) {
  set_page_id(page_id);
  *GetPageStart() = OFFSET_FREE_SPACE;
  *GetNextTupleOffset() = OFFSET_FREE_SPACE;
  *GetNumTuples() = 0;
  *GetNumDeletedTuples() = 0;
}

uint32_t TablePage::get_free_space_remaining() const {
  auto next_off = *const_cast<TablePage*>(this)->GetNextTupleOffset();
  auto num_slots = slot_high_water(*const_cast<TablePage*>(this)->GetNumTuples(),
                                   *const_cast<TablePage*>(this)->GetNumDeletedTuples());
  uint32_t slot_bytes = num_slots * static_cast<uint32_t>(sizeof(uint32_t));
  if (PAGE_SIZE < slot_bytes)
    return 0;
  if (PAGE_SIZE - slot_bytes < next_off)
    return 0;
  return (PAGE_SIZE - slot_bytes) - next_off;
}

bool TablePage::is_tuple_deleted(uint32_t tuple_offset) const {
  uint32_t header = *reinterpret_cast<const uint32_t*>(get_data() + tuple_offset);
  return (header & TUPLE_DELETED_MASK) != 0;
}

void TablePage::set_tuple_deleted(uint32_t tuple_offset) {
  uint32_t* header = reinterpret_cast<uint32_t*>(get_data() + tuple_offset);
  *header |= TUPLE_DELETED_MASK;
}

uint32_t TablePage::get_tuple_size(uint32_t tuple_offset) const {
  uint32_t header = *reinterpret_cast<const uint32_t*>(get_data() + tuple_offset);
  return header & TUPLE_SIZE_MASK;
}

void TablePage::set_tuple_size(uint32_t tuple_offset, uint32_t tuple_size) {
  uint32_t* header = reinterpret_cast<uint32_t*>(get_data() + tuple_offset);
  uint32_t deleted = (*header) & TUPLE_DELETED_MASK;
  *header = (tuple_size & TUPLE_SIZE_MASK) | deleted;
}

uint32_t TablePage::get_tuple_offset_at_slot(uint32_t slot_num) const {
  auto off = *slot_ptr(const_cast<char*>(get_data()), slot_num);
  return off;
}

void TablePage::set_tuple_offset_at_slot(uint32_t slot_num, uint32_t tuple_offset) {
  *slot_ptr(get_data(), slot_num) = tuple_offset;
}

uint32_t TablePage::find_free_slot() {
  uint32_t total = slot_high_water(*GetNumTuples(), *GetNumDeletedTuples());
  for (uint32_t i = 0; i < total; ++i) {
    if (get_tuple_offset_at_slot(i) == 0)
      return i;
  }
  return total;
}

bool TablePage::has_enough_space(uint32_t tuple_size) const {
  uint32_t need = sizeof(uint32_t) /*slot*/ + sizeof(uint32_t) /*header*/ + tuple_size;
  return get_free_space_remaining() >= need;
}

bool TablePage::insert_tuple(const Tuple& tuple, RID* rid, Transaction* /*txn*/,
                             LockManager* /*lock_manager*/, LogManager* /*log_manager*/) {
  uint32_t data_size = static_cast<uint32_t>(tuple.size());
  if (!has_enough_space(data_size))
    return false;

  uint32_t slot = find_free_slot();
  uint32_t next_off = *GetNextTupleOffset();

  // Write tuple header and data
  set_tuple_offset_at_slot(slot, next_off);
  uint32_t header = (data_size & TUPLE_SIZE_MASK); // not deleted
  *reinterpret_cast<uint32_t*>(get_data() + next_off) = header;
  tuple.serialize_to(reinterpret_cast<uint8_t*>(get_data() + next_off + sizeof(uint32_t)));
  *GetNextTupleOffset() = next_off + sizeof(uint32_t) + data_size;
  (*GetNumTuples())++;

  if (rid) {
    rid->page_id = get_page_id();
    rid->slot_num = slot;
  }
  return true;
}

bool TablePage::mark_delete(const RID& rid, Transaction* /*txn*/, LockManager* /*lock_manager*/,
                            LogManager* /*log_manager*/) {
  uint32_t off = get_tuple_offset_at_slot(rid.slot_num);
  if (off == 0)
    return false;
  if (!is_tuple_deleted(off)) {
    set_tuple_deleted(off);
    (*GetNumDeletedTuples())++;
    uint32_t n = *GetNumTuples();
    if (n > 0)
      (*GetNumTuples()) = n - 1;
  }
  return true;
}

bool TablePage::update_tuple(const Tuple& new_tuple, Tuple* old_tuple, const RID& rid,
                             Transaction* /*txn*/, LockManager* /*lock_manager*/,
                             LogManager* /*log_manager*/) {
  uint32_t off = get_tuple_offset_at_slot(rid.slot_num);
  if (off == 0)
    return false;
  if (is_tuple_deleted(off))
    return false;
  // Return old tuple if requested
  if (old_tuple) {
    uint32_t old_sz = get_tuple_size(off);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(get_data() + off + sizeof(uint32_t));
    *old_tuple = Tuple::deserialize_from(p, old_sz);
  }
  uint32_t new_sz = static_cast<uint32_t>(new_tuple.size());
  uint32_t old_sz2 = get_tuple_size(off);
  if (new_sz == old_sz2) {
    new_tuple.serialize_to(reinterpret_cast<uint8_t*>(get_data() + off + sizeof(uint32_t)));
    set_tuple_size(off, new_sz);
    return true;
  }
  // Try to append at end and repoint slot
  if (!has_enough_space(new_sz))
    return false;
  uint32_t next_off = *GetNextTupleOffset();
  *reinterpret_cast<uint32_t*>(get_data() + next_off) = (new_sz & TUPLE_SIZE_MASK);
  new_tuple.serialize_to(reinterpret_cast<uint8_t*>(get_data() + next_off + sizeof(uint32_t)));
  *GetNextTupleOffset() = next_off + sizeof(uint32_t) + new_sz;
  set_tuple_offset_at_slot(rid.slot_num, next_off);
  return true;
}

bool TablePage::get_tuple(const RID& rid, Tuple* tuple, Transaction* txn, LockManager* lock_manager,
                          LogManager* /*log_manager*/) {
  return get_tuple(rid, tuple, txn, lock_manager);
}

bool TablePage::get_tuple(const RID& rid, Tuple* tuple, Transaction* /*txn*/,
                          LockManager* /*lock_manager*/) {
  uint32_t off = get_tuple_offset_at_slot(rid.slot_num);
  if (off == 0)
    return false;
  if (is_tuple_deleted(off))
    return false;
  uint32_t sz = get_tuple_size(off);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(get_data() + off + sizeof(uint32_t));
  if (tuple)
    *tuple = Tuple::deserialize_from(p, sz);
  return true;
}

bool TablePage::get_first_tuple_rid(RID* first_rid) {
  uint32_t total = slot_high_water(*GetNumTuples(), *GetNumDeletedTuples());
  for (uint32_t i = 0; i < total; ++i) {
    uint32_t off = get_tuple_offset_at_slot(i);
    if (off != 0 && !is_tuple_deleted(off)) {
      if (first_rid) {
        first_rid->page_id = get_page_id();
        first_rid->slot_num = i;
      }
      return true;
    }
  }
  return false;
}

bool TablePage::get_next_tuple_rid(const RID& cur_rid, RID* next_rid) {
  uint32_t total = slot_high_water(*GetNumTuples(), *GetNumDeletedTuples());
  for (uint32_t i = cur_rid.slot_num + 1; i < total; ++i) {
    uint32_t off = get_tuple_offset_at_slot(i);
    if (off != 0 && !is_tuple_deleted(off)) {
      if (next_rid) {
        next_rid->page_id = get_page_id();
        next_rid->slot_num = i;
      }
      return true;
    }
  }
  return false;
}

void TablePage::rollback_delete(const RID& rid, Transaction* /*txn*/, LockManager* /*lock_manager*/,
                                LogManager* /*log_manager*/) {
  uint32_t off = get_tuple_offset_at_slot(rid.slot_num);
  if (off == 0)
    return;
  if (is_tuple_deleted(off)) {
    // Clear the deleted flag
    uint32_t* header = reinterpret_cast<uint32_t*>(get_data() + off);
    *header &= ~TUPLE_DELETED_MASK;
    (*GetNumTuples())++;
    uint32_t num_deleted = *GetNumDeletedTuples();
    if (num_deleted > 0) {
      (*GetNumDeletedTuples()) = num_deleted - 1;
    }
  }
}

} // namespace latticedb
