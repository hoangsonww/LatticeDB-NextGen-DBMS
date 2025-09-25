#pragma once

#include "../common/config.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

namespace latticedb {

enum class PageType {
  INVALID = 0,
  TABLE_PAGE,
  INDEX_PAGE,
  HASH_TABLE_BUCKET_PAGE,
  HASH_TABLE_DIRECTORY_PAGE,
  HEADER_PAGE,
  B_PLUS_TREE_INTERNAL_PAGE,
  B_PLUS_TREE_LEAF_PAGE
};

class Page {
private:
  static_assert(sizeof(std::atomic<bool>) == sizeof(bool));

  char data_[PAGE_SIZE];
  uint32_t page_id_;
  std::atomic<int> pin_count_;
  std::atomic<bool> is_dirty_;
  std::atomic<bool> is_deleted_;

public:
  Page() {
    reset();
  }

  ~Page() = default;

  Page(const Page& other) = delete;
  Page& operator=(const Page& other) = delete;

  inline char* get_data() {
    return data_;
  }

  inline const char* get_data() const {
    return data_;
  }

  inline uint32_t get_page_id() const {
    return page_id_;
  }

  inline void set_page_id(uint32_t page_id) {
    page_id_ = page_id;
  }

  inline int get_pin_count() const {
    return pin_count_;
  }

  inline void pin() {
    pin_count_++;
  }

  inline void unpin() {
    int old_pin_count = pin_count_--;
    if (old_pin_count <= 0) {
      pin_count_ = 0;
    }
  }

  inline bool is_pinned() const {
    return pin_count_ > 0;
  }

  inline bool is_dirty() const {
    return is_dirty_;
  }

  inline void set_dirty(bool dirty) {
    is_dirty_ = dirty;
  }

  inline bool is_deleted() const {
    return is_deleted_;
  }

  inline void set_deleted(bool deleted) {
    is_deleted_ = deleted;
  }

  void reset() {
    memset(data_, 0, PAGE_SIZE);
    page_id_ = INVALID_PAGE_ID;
    pin_count_ = 0;
    is_dirty_ = false;
    is_deleted_ = false;
  }

protected:
  template <typename T> inline T read_at(size_t offset) const {
    return *reinterpret_cast<const T*>(data_ + offset);
  }

  template <typename T> inline void write_at(size_t offset, const T& value) {
    *reinterpret_cast<T*>(data_ + offset) = value;
    set_dirty(true);
  }
};

class PageGuard {
private:
  Page* page_;
  bool is_dirty_;

public:
  PageGuard() : page_(nullptr), is_dirty_(false) {}

  PageGuard(Page* page, bool is_dirty = false) : page_(page), is_dirty_(is_dirty) {
    if (page_ != nullptr) {
      page_->pin();
    }
  }

  ~PageGuard() {
    if (page_ != nullptr) {
      if (is_dirty_) {
        page_->set_dirty(true);
      }
      page_->unpin();
    }
  }

  PageGuard(const PageGuard&) = delete;
  PageGuard& operator=(const PageGuard&) = delete;

  PageGuard(PageGuard&& other) noexcept : page_(other.page_), is_dirty_(other.is_dirty_) {
    other.page_ = nullptr;
    other.is_dirty_ = false;
  }

  PageGuard& operator=(PageGuard&& other) noexcept {
    if (this != &other) {
      if (page_ != nullptr) {
        if (is_dirty_) {
          page_->set_dirty(true);
        }
        page_->unpin();
      }
      page_ = other.page_;
      is_dirty_ = other.is_dirty_;
      other.page_ = nullptr;
      other.is_dirty_ = false;
    }
    return *this;
  }

  Page* get() const {
    return page_;
  }
  Page* operator->() const {
    return page_;
  }
  Page& operator*() const {
    return *page_;
  }

  bool is_valid() const {
    return page_ != nullptr;
  }

  void mark_dirty() {
    is_dirty_ = true;
  }

  PageGuard upgrade() && {
    return PageGuard(page_, true);
  }
};

} // namespace latticedb