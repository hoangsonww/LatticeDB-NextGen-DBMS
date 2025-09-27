#include <mutex>

#include "buffer_pool_manager.h"

namespace latticedb {

BufferPoolManager::BufferPoolManager(uint32_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size), pages_(new Page[pool_size]), disk_manager_(disk_manager),
      replacer_(new LRUReplacer(pool_size)) {
  for (frame_id_t i = 0; i < pool_size_; ++i) {
    free_list_.push_back(i);
  }
}

BufferPoolManager::~BufferPoolManager() {
  try {
    flush_all_pages();
  } catch (...) {
    // Ignore exceptions during destruction
  }
}

void BufferPoolManager::reset_memory() {
  page_table_.clear();
  free_list_.clear();
  for (frame_id_t i = 0; i < pool_size_; ++i)
    free_list_.push_back(i);
}

bool BufferPoolManager::validate_page_id(page_id_t page_id) const {
  (void)page_id;
  return true;
}

void BufferPoolManager::record_access(frame_id_t frame_id, AccessType access_type) {
  replacer_->record_access(frame_id, access_type);
}

bool BufferPoolManager::find_replacement(frame_id_t* frame_id) {
  if (!free_list_.empty()) {
    *frame_id = free_list_.front();
    free_list_.pop_front();
    return true;
  }
  return replacer_->victim(frame_id);
}

Page* BufferPoolManager::fetch_page_impl(page_id_t page_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    frame_id_t fid = it->second;
    pages_[fid].pin();
    replacer_->pin(fid);
    return &pages_[fid];
  }
  frame_id_t fid;
  if (!find_replacement(&fid))
    return nullptr;

  // If frame had a valid page and dirty, flush
  if (pages_[fid].get_page_id() != INVALID_PAGE_ID && pages_[fid].is_dirty() && disk_manager_) {
    disk_manager_->write_page(pages_[fid].get_page_id(), pages_[fid].get_data());
    pages_[fid].set_dirty(false);
  }

  // Load page from disk
  pages_[fid].reset();
  if (disk_manager_)
    disk_manager_->read_page(page_id, pages_[fid].get_data());
  pages_[fid].set_page_id(page_id);
  pages_[fid].pin();
  page_table_[page_id] = fid;
  replacer_->pin(fid);
  return &pages_[fid];
}

Page* BufferPoolManager::new_page(page_id_t* page_id) {
  std::lock_guard<std::mutex> l(latch_);
  frame_id_t fid;
  if (!find_replacement(&fid))
    return nullptr;
  pages_[fid].reset();
  if (disk_manager_) {
    *page_id = disk_manager_->allocate_page();
  } else {
    static page_id_t next_id = 1;
    *page_id = next_id++;
  }
  pages_[fid].set_page_id(*page_id);
  pages_[fid].pin();
  page_table_[*page_id] = fid;
  replacer_->pin(fid);
  return &pages_[fid];
}

PageGuard BufferPoolManager::fetch_page(page_id_t page_id) {
  return PageGuard(fetch_page_impl(page_id));
}

PageGuard BufferPoolManager::new_page_guarded(page_id_t* page_id) {
  return PageGuard(new_page(page_id));
}

bool BufferPoolManager::unpin_page(page_id_t page_id, bool is_dirty, AccessType access_type) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = page_table_.find(page_id);
  if (it == page_table_.end())
    return false;
  frame_id_t fid = it->second;
  if (is_dirty)
    pages_[fid].set_dirty(true);
  pages_[fid].unpin();
  replacer_->unpin(fid);
  replacer_->record_access(fid, access_type);
  return true;
}

bool BufferPoolManager::flush_page(page_id_t page_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = page_table_.find(page_id);
  if (it == page_table_.end())
    return false;
  frame_id_t fid = it->second;
  if (pages_[fid].is_dirty() && disk_manager_) {
    disk_manager_->write_page(page_id, pages_[fid].get_data());
    pages_[fid].set_dirty(false);
  }
  return true;
}

void BufferPoolManager::flush_all_pages() {
  std::lock_guard<std::mutex> l(latch_);
  for (auto& kv : page_table_) {
    frame_id_t fid = kv.second;
    if (pages_[fid].is_dirty() && disk_manager_) {
      disk_manager_->write_page(pages_[fid].get_page_id(), pages_[fid].get_data());
      pages_[fid].set_dirty(false);
    }
  }
}

bool BufferPoolManager::delete_page(page_id_t page_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = page_table_.find(page_id);
  if (it == page_table_.end())
    return true;
  frame_id_t fid = it->second;
  if (pages_[fid].is_pinned())
    return false;
  page_table_.erase(it);
  pages_[fid].reset();
  free_list_.push_back(fid);
  return true;
}

uint32_t BufferPoolManager::get_free_list_size() {
  std::lock_guard<std::mutex> l(latch_);
  return static_cast<uint32_t>(free_list_.size());
}

} // namespace latticedb
