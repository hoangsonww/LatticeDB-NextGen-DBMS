#pragma once

#include "../buffer/buffer_pool_manager.h"
#include "b_plus_tree_page.h"

namespace latticedb {

template <typename KeyType, typename ValueType, typename KeyComparator> class IndexIterator {
private:
  using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;
  using MappingType = std::pair<KeyType, ValueType>;

  BufferPoolManager* buffer_pool_manager_;
  page_id_t page_id_;
  int index_;
  LeafPage* leaf_page_;

public:
  IndexIterator();

  IndexIterator(BufferPoolManager* buffer_pool_manager, page_id_t page_id, int index);

  ~IndexIterator();

  IndexIterator(const IndexIterator& other) = delete;
  IndexIterator& operator=(const IndexIterator& other) = delete;

  IndexIterator(IndexIterator&& other) noexcept;
  IndexIterator& operator=(IndexIterator&& other) noexcept;

  const MappingType& operator*() const;

  const MappingType* operator->() const;

  IndexIterator& operator++();

  IndexIterator operator++(int);

  bool operator==(const IndexIterator& other) const;

  bool operator!=(const IndexIterator& other) const;

  bool is_end() const;

private:
  void move_to_next();

  void unpin_current_page();

  void fetch_current_page();
};

using IntegerKeyType = GenericKey<8>;
using IntegerValueType = RID;
using IntegerComparatorType = GenericComparator<8>;

template class IndexIterator<IntegerKeyType, IntegerValueType, IntegerComparatorType>;

} // namespace latticedb