// Implementation for b_plus_tree_page.h
namespace latticedb {

// Provide template definitions with minimal behavior. These are not explicitly instantiated
// here to avoid coupling to specific Key/Comparator types, but keep this file non-empty
// and ready for future explicit instantiations.

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::init(page_id_t page_id,
                                                                page_id_t parent_id, int max_size) {
  set_page_type(IndexPageType::LEAF_PAGE);
  set_page_id(page_id);
  set_parent_page_id(parent_id);
  set_max_size(max_size);
  set_size(0);
  set_next_page_id(INVALID_PAGE_ID);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
int BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::key_index(
    const KeyType& key, const KeyComparator& comparator) const {
  // Linear search minimal implementation
  for (int i = 0; i < get_size(); ++i) {
    if (comparator(key_at(i), key) >= 0)
      return i;
  }
  return get_size();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
int BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::insert(const KeyType& key,
                                                                 const ValueType& value,
                                                                 const KeyComparator& comparator) {
  int idx = key_index(key, comparator);
  // shift right
  for (int i = get_size(); i > idx; --i)
    array_[i] = array_[i - 1];
  array_[idx] = {key, value};
  increase_size(1);
  return get_size();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::lookup(
    const KeyType& key, ValueType* value, const KeyComparator& comparator) const {
  for (int i = 0; i < get_size(); ++i) {
    if (comparator(array_[i].first, key) == 0) {
      if (value)
        *value = array_[i].second;
      return true;
    }
  }
  return false;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
int BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::remove_and_delete_record(
    const KeyType& key, const KeyComparator& comparator) {
  for (int i = 0; i < get_size(); ++i) {
    if (comparator(array_[i].first, key) == 0) {
      for (int j = i; j < get_size() - 1; ++j)
        array_[j] = array_[j + 1];
      set_size(get_size() - 1);
      return get_size();
    }
  }
  return get_size();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::move_half_to(
    BPlusTreeLeafPage* recipient) {
  int half = get_size() / 2;
  int start = get_size() - half;
  for (int i = 0; i < half; ++i)
    recipient->array_[recipient->get_size() + i] = array_[start + i];
  recipient->set_size(recipient->get_size() + half);
  set_size(start);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::move_all_to(
    BPlusTreeLeafPage* recipient) {
  int n = get_size();
  for (int i = 0; i < n; ++i)
    recipient->array_[recipient->get_size() + i] = array_[i];
  recipient->set_size(recipient->get_size() + n);
  set_size(0);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::move_first_to_end_of(
    BPlusTreeLeafPage* recipient) {
  if (get_size() == 0)
    return;
  recipient->array_[recipient->get_size()] = array_[0];
  recipient->increase_size(1);
  for (int i = 0; i < get_size() - 1; ++i)
    array_[i] = array_[i + 1];
  set_size(get_size() - 1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>::move_last_to_front_of(
    BPlusTreeLeafPage* recipient) {
  if (recipient->get_size() == 0)
    return;
  for (int i = recipient->get_size(); i > 0; --i)
    recipient->array_[i] = recipient->array_[i - 1];
  recipient->array_[0] = array_[get_size() - 1];
  recipient->increase_size(1);
  set_size(get_size() - 1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::init(page_id_t page_id,
                                                                    page_id_t parent_id,
                                                                    int max_size) {
  set_page_type(IndexPageType::INTERNAL_PAGE);
  set_page_id(page_id);
  set_parent_page_id(parent_id);
  set_max_size(max_size);
  set_size(0);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
ValueType BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::lookup(
    const KeyType& key, const KeyComparator& comparator) const {
  int i = 1;
  while (i < get_size() && comparator(key_at(i), key) <= 0)
    i++;
  return value_at(i - 1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::populate_new_root(
    const ValueType& old_value, const KeyType& new_key, const ValueType& new_value) {
  array_[0] = {KeyType{}, old_value};
  array_[1] = {new_key, new_value};
  set_size(2);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
int BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::insert_node_after(
    const ValueType& old_value, const KeyType& new_key, const ValueType& new_value) {
  int i = 0;
  while (i < get_size() && value_at(i) != old_value)
    i++;
  for (int j = get_size(); j > i + 1; --j)
    array_[j] = array_[j - 1];
  array_[i + 1] = {new_key, new_value};
  increase_size(1);
  return get_size();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::remove(int index) {
  for (int i = index; i < get_size() - 1; ++i)
    array_[i] = array_[i + 1];
  set_size(get_size() - 1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
ValueType BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::remove_and_return_only_child() {
  ValueType v = value_at(0);
  set_size(0);
  return v;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::move_half_to(
    BPlusTreeInternalPage* recipient, BufferPoolManager*) {
  int half = get_size() / 2;
  int start = get_size() - half;
  for (int i = 0; i < half; ++i)
    recipient->array_[recipient->get_size() + i] = array_[start + i];
  recipient->set_size(recipient->get_size() + half);
  set_size(start);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::move_all_to(
    BPlusTreeInternalPage* recipient, const KeyType& middle_key, BufferPoolManager*) {
  int n = get_size();
  recipient->array_[recipient->get_size()] = {middle_key, value_at(0)};
  recipient->increase_size(1);
  for (int i = 1; i < n; ++i)
    recipient->array_[recipient->get_size() + i - 1] = array_[i];
  recipient->set_size(recipient->get_size() + n - 1);
  set_size(0);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::move_first_to_end_of(
    BPlusTreeInternalPage* recipient, const KeyType& middle_key, BufferPoolManager*) {
  recipient->array_[recipient->get_size()] = {middle_key, value_at(0)};
  recipient->increase_size(1);
  for (int i = 0; i < get_size() - 1; ++i)
    array_[i] = array_[i + 1];
  set_size(get_size() - 1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>::move_last_to_front_of(
    BPlusTreeInternalPage* recipient, const KeyType& middle_key, BufferPoolManager*) {
  for (int i = recipient->get_size(); i > 0; --i)
    recipient->array_[i] = recipient->array_[i - 1];
  recipient->array_[0] = {middle_key, value_at(get_size() - 1)};
  recipient->increase_size(1);
  set_size(get_size() - 1);
}

} // namespace latticedb
