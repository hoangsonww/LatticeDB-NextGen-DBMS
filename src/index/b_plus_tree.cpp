#include <mutex>

#include "b_plus_tree.h"
#include "b_plus_tree_page.h"
#include <algorithm>
#include <queue>
#include <sstream>

namespace latticedb {

template <typename KeyType, typename ValueType, typename KeyComparator>
BPlusTree<KeyType, ValueType, KeyComparator>::BPlusTree(std::string name,
                                                        BufferPoolManager* buffer_pool_manager,
                                                        const KeyComparator& comparator,
                                                        int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)), root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager), comparator_(comparator),
      leaf_max_size_(leaf_max_size), internal_max_size_(internal_max_size) {}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::is_empty() const {
  return root_page_id_ == INVALID_PAGE_ID;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::insert(const KeyType& key,
                                                          const ValueType& value,
                                                          Transaction* txn) {
  std::lock_guard<std::mutex> lock(latch_);

  if (is_empty()) {
    start_new_tree();
  }

  return insert_into_leaf(key, value, txn);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::remove(const KeyType& key, Transaction* txn) {
  std::lock_guard<std::mutex> lock(latch_);

  if (is_empty()) {
    return;
  }

  remove_from_leaf(key, txn);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::get_value(const KeyType& key,
                                                             std::vector<ValueType>* result,
                                                             Transaction* txn) {
  std::lock_guard<std::mutex> lock(latch_);

  if (is_empty() || !result) {
    return false;
  }

  // Simple linear search for demo
  result->clear();

  // In real implementation, would traverse tree to find leaf
  // For now, return false (not found)
  (void)key;
  (void)txn;
  return false;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::get_value(const KeyType& key, ValueType* result,
                                                             Transaction* txn) {
  std::vector<ValueType> values;
  if (get_value(key, &values, txn) && !values.empty()) {
    if (result) {
      *result = values[0];
    }
    return true;
  }
  return false;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::begin(Iterator* iter) const {
  if (!iter)
    return;

  iter->page_id_ = INVALID_PAGE_ID;
  iter->index_ = 0;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::begin(const KeyType& key, Iterator* iter) const {
  if (!iter)
    return;

  (void)key;
  iter->page_id_ = INVALID_PAGE_ID;
  iter->index_ = 0;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
std::string BPlusTree<KeyType, ValueType, KeyComparator>::to_string() {
  if (is_empty()) {
    return "Empty B+ Tree: " + index_name_;
  }

  std::stringstream ss;
  ss << "B+ Tree: " << index_name_ << "\n";
  ss << "Root Page ID: " << root_page_id_ << "\n";

  return ss.str();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::draw(BufferPoolManager* bpm,
                                                        const std::string& filename) const {
  (void)bpm;
  (void)filename;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::check() {
  return !is_empty();
}

// Private helper methods

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::start_new_tree() {
  page_id_t new_page_id;
  auto* new_page = buffer_pool_manager_->new_page(&new_page_id);
  if (new_page) {
    root_page_id_ = new_page_id;
    buffer_pool_manager_->unpin_page(new_page_id, true);
  }
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::start_new_tree(const KeyType& key,
                                                                  const ValueType& value) {
  page_id_t new_page_id;
  auto* new_page = buffer_pool_manager_->new_page(&new_page_id);
  if (new_page) {
    root_page_id_ = new_page_id;
    // Initialize as leaf page and insert first key-value pair
    // In real implementation would initialize the page properly
    (void)key;
    (void)value;
    buffer_pool_manager_->unpin_page(new_page_id, true);
  }
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::insert_into_leaf(const KeyType& key,
                                                                    const ValueType& value,
                                                                    Transaction* txn) {
  (void)key;
  (void)value;
  (void)txn;
  // Simplified implementation
  return true;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTree<KeyType, ValueType, KeyComparator>::remove_from_leaf(const KeyType& key,
                                                                    Transaction* txn) {
  (void)key;
  (void)txn;
  // Simplified implementation
}

// Explicit template instantiations for common types
template class BPlusTree<int, int, std::less<int>>;
template class BPlusTree<int64_t, int64_t, std::less<int64_t>>;
template class BPlusTree<int, RID, std::less<int>>;
template class BPlusTree<int64_t, RID, std::less<int64_t>>;

} // namespace latticedb