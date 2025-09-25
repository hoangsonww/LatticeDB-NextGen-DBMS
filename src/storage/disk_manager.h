#pragma once

#include "../common/config.h"
#include "../common/exception.h"
#include "page.h"
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

namespace latticedb {

class DiskManager {
private:
  std::string db_file_;
  std::fstream db_io_;
  std::atomic<uint32_t> next_page_id_;
  std::mutex db_io_mutex_;
  bool flush_log_;
  std::atomic<uint32_t> num_flushes_;
  std::atomic<uint32_t> num_writes_;

  static constexpr uint32_t BITMAP_PAGE_SIZE = PAGE_SIZE;
  static constexpr uint32_t BITMAP_PAGE_CAPACITY = BITMAP_PAGE_SIZE * 8;

public:
  explicit DiskManager(const std::string& db_file);

  ~DiskManager();

  void shutdown();

  void read_page(uint32_t page_id, char* page_data);

  void write_page(uint32_t page_id, const char* page_data);

  uint32_t allocate_page();

  void deallocate_page(uint32_t page_id);

  uint32_t get_num_pages() const {
    return next_page_id_;
  }

  bool has_free_pages() const;

  void set_flush_log(bool flush_log) {
    flush_log_ = flush_log;
  }

  uint32_t get_num_flushes() const {
    return num_flushes_;
  }

  uint32_t get_num_writes() const {
    return num_writes_;
  }

  void force_flush();

private:
  void read_physical_page(uint32_t page_id, char* page_data);

  void write_physical_page(uint32_t page_id, const char* page_data);

  uint32_t get_file_size(const std::string& file_name);

  void create_new_database_file();

  bool is_page_allocated(uint32_t page_id);

  void mark_page_allocated(uint32_t page_id);

  void mark_page_deallocated(uint32_t page_id);

  uint32_t get_bitmap_page_id(uint32_t page_id);

  uint32_t get_bitmap_offset(uint32_t page_id);
};

class DiskScheduler {
private:
  DiskManager* disk_manager_;
  std::atomic<bool> shutdown_cv_;

public:
  explicit DiskScheduler(DiskManager* disk_manager);

  ~DiskScheduler();

  void schedule_read(uint32_t page_id, char* page_data);

  void schedule_write(uint32_t page_id, const char* page_data);

  void shutdown();

private:
  struct DiskRequest {
    bool is_write;
    uint32_t page_id;
    char* data;

    DiskRequest(bool is_write, uint32_t page_id, char* data)
        : is_write(is_write), page_id(page_id), data(data) {}
  };
};

} // namespace latticedb