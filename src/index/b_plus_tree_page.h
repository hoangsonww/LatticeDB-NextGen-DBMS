#pragma once

#include "../common/config.h"
#include "../storage/page.h"

namespace latticedb {

enum class IndexPageType { INVALID_INDEX_PAGE = 0, LEAF_PAGE, INTERNAL_PAGE };

class BPlusTreePage : public Page {
public:
  bool is_leaf_page() const {
    return page_type_ == IndexPageType::LEAF_PAGE;
  }
  bool is_internal_page() const {
    return page_type_ == IndexPageType::INTERNAL_PAGE;
  }
  bool is_root_page() const {
    return parent_page_id_ == INVALID_PAGE_ID;
  }

  void set_page_type(IndexPageType page_type) {
    page_type_ = page_type;
    set_dirty(true);
  }

  int get_size() const {
    return size_;
  }
  void set_size(int size) {
    size_ = size;
    set_dirty(true);
  }
  void increase_size(int amount) {
    size_ += amount;
    set_dirty(true);
  }

  int get_max_size() const {
    return max_size_;
  }
  void set_max_size(int size) {
    max_size_ = size;
    set_dirty(true);
  }
  int get_min_size() const {
    return max_size_ / 2;
  }

  page_id_t get_parent_page_id() const {
    return parent_page_id_;
  }
  void set_parent_page_id(page_id_t parent_page_id) {
    parent_page_id_ = parent_page_id;
    set_dirty(true);
  }

  page_id_t get_page_id() const {
    return page_id_;
  }
  void set_page_id(page_id_t page_id) {
    page_id_ = page_id;
    set_dirty(true);
  }

  void set_lsn(lsn_t lsn = INVALID_LSN) {
    lsn_ = lsn;
    set_dirty(true);
  }

protected:
  IndexPageType page_type_ __attribute__((__unused__));
  lsn_t lsn_ __attribute__((__unused__));
  int size_ __attribute__((__unused__));
  int max_size_ __attribute__((__unused__));
  page_id_t parent_page_id_ __attribute__((__unused__));
  page_id_t page_id_ __attribute__((__unused__));
};

template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTreeLeafPage : public BPlusTreePage {
public:
  void init(page_id_t page_id, page_id_t parent_id, int max_size);

  page_id_t get_next_page_id() const {
    return next_page_id_;
  }
  void set_next_page_id(page_id_t next_page_id) {
    next_page_id_ = next_page_id;
    set_dirty(true);
  }

  KeyType key_at(int index) const {
    return array_[index].first;
  }

  ValueType value_at(int index) const {
    return array_[index].second;
  }

  std::pair<KeyType, ValueType> get_item(int index) {
    return array_[index];
  }

  int key_index(const KeyType& key, const KeyComparator& comparator) const;

  const MappingType& get_item_at(int index) const {
    return array_[index];
  }

  void set_item_at(int index, const KeyType& key, const ValueType& value) {
    array_[index] = {key, value};
    set_dirty(true);
  }

  int insert(const KeyType& key, const ValueType& value, const KeyComparator& comparator);

  bool lookup(const KeyType& key, ValueType* value, const KeyComparator& comparator) const;

  int remove_and_delete_record(const KeyType& key, const KeyComparator& comparator);

  void move_half_to(BPlusTreeLeafPage* recipient);

  void move_all_to(BPlusTreeLeafPage* recipient);

  void move_first_to_end_of(BPlusTreeLeafPage* recipient);

  void move_last_to_front_of(BPlusTreeLeafPage* recipient);

private:
  using MappingType = std::pair<KeyType, ValueType>;

  page_id_t next_page_id_;
  MappingType array_[1];
};

template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTreeInternalPage : public BPlusTreePage {
public:
  void init(page_id_t page_id, page_id_t parent_id, int max_size);

  KeyType key_at(int index) const {
    return array_[index].first;
  }

  void set_key_at(int index, const KeyType& key) {
    array_[index].first = key;
    set_dirty(true);
  }

  ValueType value_at(int index) const {
    return array_[index].second;
  }

  void set_value_at(int index, const ValueType& value) {
    array_[index].second = value;
    set_dirty(true);
  }

  ValueType lookup(const KeyType& key, const KeyComparator& comparator) const;

  void populate_new_root(const ValueType& old_value, const KeyType& new_key,
                         const ValueType& new_value);

  int insert_node_after(const ValueType& old_value, const KeyType& new_key,
                        const ValueType& new_value);

  void remove(int index);

  ValueType remove_and_return_only_child();

  void move_half_to(BPlusTreeInternalPage* recipient, BufferPoolManager* buffer_pool_manager);

  void move_all_to(BPlusTreeInternalPage* recipient, const KeyType& middle_key,
                   BufferPoolManager* buffer_pool_manager);

  void move_first_to_end_of(BPlusTreeInternalPage* recipient, const KeyType& middle_key,
                            BufferPoolManager* buffer_pool_manager);

  void move_last_to_front_of(BPlusTreeInternalPage* recipient, const KeyType& middle_key,
                             BufferPoolManager* buffer_pool_manager);

private:
  using MappingType = std::pair<KeyType, ValueType>;

  MappingType array_[1];

  void copy_n_from(MappingType* items, int size) {
    std::copy(items, items + size, array_);
    increase_size(size);
    set_dirty(true);
  }

  void copy_last_from(const MappingType& pair) {
    array_[get_size()] = pair;
    increase_size(1);
    set_dirty(true);
  }

  void copy_first_from(const MappingType& pair, BufferPoolManager* buffer_pool_manager) {
    std::memmove(array_ + 1, array_, get_size() * sizeof(MappingType));
    array_[0] = pair;
    increase_size(1);
    set_dirty(true);
  }
};

} // namespace latticedb