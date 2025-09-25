#pragma once

#include "../common/config.h"
#include "../common/logger.h"
#include "../storage/disk_manager.h"
#include "../storage/page.h"
#include "replacer.h"
#include <list>
#include <mutex>
#include <unordered_map>

namespace latticedb {

class BufferPoolManager {
private:
  uint32_t pool_size_;
  std::unique_ptr<Page[]> pages_;
  DiskManager* disk_manager_;
  std::unique_ptr<Replacer> replacer_;
  std::list<frame_id_t> free_list_;
  std::unordered_map<page_id_t, frame_id_t> page_table_;
  std::mutex latch_;

public:
  explicit BufferPoolManager(uint32_t pool_size, DiskManager* disk_manager);

  ~BufferPoolManager();

  Page* new_page(page_id_t* page_id);

  PageGuard fetch_page(page_id_t page_id);

  PageGuard new_page_guarded(page_id_t* page_id);

  bool unpin_page(page_id_t page_id, bool is_dirty, AccessType access_type = AccessType::Unknown);

  bool flush_page(page_id_t page_id);

  void flush_all_pages();

  bool delete_page(page_id_t page_id);

  uint32_t get_pool_size() const {
    return pool_size_;
  }

  uint32_t get_free_list_size();

private:
  Page* fetch_page_impl(page_id_t page_id);

  bool find_replacement(frame_id_t* frame_id);

  void update_replacer_on_pin(frame_id_t frame_id);

  void reset_memory();

  bool validate_page_id(page_id_t page_id) const;

  void record_access(frame_id_t frame_id, AccessType access_type);
};

} // namespace latticedb
