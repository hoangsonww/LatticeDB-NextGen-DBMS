// Implementation for index_iterator.h
#include "index_iterator.h"

namespace latticedb {

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>::IndexIterator()
    : buffer_pool_manager_(nullptr), page_id_(INVALID_PAGE_ID), index_(0), leaf_page_(nullptr) {}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>::IndexIterator(
    BufferPoolManager* buffer_pool_manager, page_id_t page_id, int index)
    : buffer_pool_manager_(buffer_pool_manager), page_id_(page_id), index_(index),
      leaf_page_(nullptr) {
  fetch_current_page();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>::~IndexIterator() {
  unpin_current_page();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>::IndexIterator(IndexIterator&& other) noexcept
    : buffer_pool_manager_(other.buffer_pool_manager_), page_id_(other.page_id_),
      index_(other.index_), leaf_page_(other.leaf_page_) {
  other.buffer_pool_manager_ = nullptr;
  other.page_id_ = INVALID_PAGE_ID;
  other.index_ = 0;
  other.leaf_page_ = nullptr;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>&
IndexIterator<KeyType, ValueType, KeyComparator>::operator=(IndexIterator&& other) noexcept {
  if (this != &other) {
    unpin_current_page();
    buffer_pool_manager_ = other.buffer_pool_manager_;
    page_id_ = other.page_id_;
    index_ = other.index_;
    leaf_page_ = other.leaf_page_;
    other.buffer_pool_manager_ = nullptr;
    other.page_id_ = INVALID_PAGE_ID;
    other.index_ = 0;
    other.leaf_page_ = nullptr;
  }
  return *this;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto IndexIterator<KeyType, ValueType, KeyComparator>::operator*() const -> const MappingType& {
  return leaf_page_->get_item_at(index_);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto IndexIterator<KeyType, ValueType, KeyComparator>::operator->() const -> const MappingType* {
  return &leaf_page_->get_item_at(index_);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>&
IndexIterator<KeyType, ValueType, KeyComparator>::operator++() {
  move_to_next();
  return *this;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>
IndexIterator<KeyType, ValueType, KeyComparator>::operator++(int) {
  IndexIterator tmp = std::move(*this);
  move_to_next();
  return tmp;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool IndexIterator<KeyType, ValueType, KeyComparator>::operator==(
    const IndexIterator& other) const {
  return page_id_ == other.page_id_ && index_ == other.index_ && leaf_page_ == other.leaf_page_;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool IndexIterator<KeyType, ValueType, KeyComparator>::operator!=(
    const IndexIterator& other) const {
  return !(*this == other);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool IndexIterator<KeyType, ValueType, KeyComparator>::is_end() const {
  return leaf_page_ == nullptr || page_id_ == INVALID_PAGE_ID;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void IndexIterator<KeyType, ValueType, KeyComparator>::move_to_next() {
  if (leaf_page_ == nullptr)
    return;
  index_++;
  if (index_ >= leaf_page_->get_size()) {
    auto next = leaf_page_->get_next_page_id();
    unpin_current_page();
    page_id_ = next;
    index_ = 0;
    fetch_current_page();
  }
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void IndexIterator<KeyType, ValueType, KeyComparator>::unpin_current_page() {
  if (buffer_pool_manager_ && page_id_ != INVALID_PAGE_ID) {
    buffer_pool_manager_->unpin_page(page_id_, false);
  }
  leaf_page_ = nullptr;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void IndexIterator<KeyType, ValueType, KeyComparator>::fetch_current_page() {
  if (!buffer_pool_manager_ || page_id_ == INVALID_PAGE_ID) {
    leaf_page_ = nullptr;
    return;
  }
  auto guard = buffer_pool_manager_->fetch_page(page_id_);
  if (!guard.is_valid()) {
    leaf_page_ = nullptr;
    return;
  }
  leaf_page_ = reinterpret_cast<LeafPage*>(guard.get());
}

// Explicit instantiation for the common integer key index iterator
using IntegerKeyType = GenericKey<8>;
using IntegerValueType = RID;
using IntegerComparatorType = GenericComparator<8>;

template class IndexIterator<IntegerKeyType, IntegerValueType, IntegerComparatorType>;

} // namespace latticedb
