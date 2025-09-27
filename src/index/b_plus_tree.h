#pragma once
#include <mutex>

#include "../buffer/buffer_pool_manager.h"
#include "../concurrency/lock_manager.h"
#include "../storage/page.h"
#include "../transaction/transaction.h"
#include "../types/value.h"
#include <queue>
#include <vector>

namespace latticedb {

// Forward declarations
class BPlusTreePage;
template <typename KeyType, typename PageType, typename KeyComparator> class BPlusTreeInternalPage;
template <typename KeyType, typename ValueType, typename KeyComparator> class BPlusTreeLeafPage;

// Constants for B+ tree
static constexpr int LEAF_PAGE_SIZE = 63;
static constexpr int INTERNAL_PAGE_SIZE = 127;

template <typename KeyType, typename ValueType, typename KeyComparator> class BPlusTree {
public:
  using MappingType = std::pair<KeyType, ValueType>;
  class Iterator;
  explicit BPlusTree(std::string name, BufferPoolManager* buffer_pool_manager,
                     const KeyComparator& comparator, int leaf_max_size = LEAF_PAGE_SIZE,
                     int internal_max_size = INTERNAL_PAGE_SIZE);

  bool is_empty() const;

  bool insert(const KeyType& key, const ValueType& value, Transaction* transaction = nullptr);

  void remove(const KeyType& key, Transaction* transaction = nullptr);

  bool get_value(const KeyType& key, std::vector<ValueType>* result,
                 Transaction* transaction = nullptr);

  bool get_value(const KeyType& key, ValueType* value, Transaction* transaction = nullptr);

  void begin(Iterator* iter) const;

  void begin(const KeyType& key, Iterator* iter) const;

  std::string to_string();

  void draw(BufferPoolManager* bpm, const std::string& out_filename) const;

  bool check();

private:
  std::string index_name_;
  page_id_t root_page_id_;
  BufferPoolManager* buffer_pool_manager_;
  KeyComparator comparator_;
  int leaf_max_size_;
  int internal_max_size_;
  std::mutex root_page_id_mutex_;
  mutable std::mutex latch_;

  page_id_t get_root_page_id(bool lock_root_page_id_latch = true);

  void set_root_page_id(page_id_t root_page_id);

  void update_root_page_id(int insert_record = 0);

  template <typename N> Page* fetch_page(page_id_t page_id);

  template <typename N> N* reinterpret_cast_page(Page* page);

  template <typename N> void unpin_page(N* page, bool is_dirty);

  void start_new_tree(const KeyType& key, const ValueType& value);

  bool insert_into_leaf(const KeyType& key, const ValueType& value, Transaction* transaction);

  void insert_into_parent(BPlusTreePage* old_node, const KeyType& key, BPlusTreePage* new_node,
                          Transaction* transaction);

  template <typename N> N* split(N* node);

  bool coalesce_or_redistribute(BPlusTreePage* node, Transaction* transaction);

  template <typename N>
  bool coalesce(N** neighbor_node, N** node,
                BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>** parent, int index,
                Transaction* transaction);

  template <typename N> void redistribute(N* neighbor_node, N* node, int index);

  bool adjust_root(BPlusTreePage* root_node);

  void update_root_page_id_in_header_page(page_id_t root_page_id);

  page_id_t get_root_page_id_from_header_page();

public:
  class Iterator {
  private:
    BufferPoolManager* buffer_pool_manager_;
    BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>* leaf_page_;
    int index_;
    page_id_t page_id_;

  public:
    Iterator();
    Iterator(BufferPoolManager* buffer_pool_manager, page_id_t page_id, int index);
    ~Iterator();

    bool is_end();
    const MappingType& operator*();
    Iterator& operator++();
    bool operator==(const Iterator& itr) const;
    bool operator!=(const Iterator& itr) const;

  private:
    void unpin_page() {
      if (leaf_page_ != nullptr) {
        buffer_pool_manager_->unpin_page(page_id_, false);
        leaf_page_ = nullptr;
      }
    }
  };
};

} // namespace latticedb