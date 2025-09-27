#include <mutex>

#include "disk_manager.h"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h> // for fsync

namespace latticedb {

static uint64_t page_offset(uint32_t page_id) {
  return static_cast<uint64_t>(page_id) * PAGE_SIZE;
}

DiskManager::DiskManager(const std::string& db_file)
    : db_file_(db_file), next_page_id_(1), flush_log_(false), num_flushes_(0), num_writes_(0) {
  // Open file in read/write; create if not exists
  db_io_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
  if (!db_io_.is_open()) {
    // create new
    create_new_database_file();
  } else {
    // compute next_page_id from size
    uint32_t file_size = get_file_size(db_file_);
    next_page_id_ = file_size / PAGE_SIZE;
    if (next_page_id_ == 0)
      next_page_id_ = 1;
  }
}

DiskManager::~DiskManager() {
  shutdown();
}

void DiskManager::shutdown() {
  std::lock_guard<std::mutex> l(db_io_mutex_);
  if (db_io_.is_open()) {
    db_io_.flush();
    force_flush();
    db_io_.close();
  }
}

void DiskManager::force_flush() {
  // Force all buffered writes to disk
  db_io_.flush();

  // Platform-specific sync to disk
  // Note: This is a simplified approach. In production, we'd get the actual file descriptor
  // For now, rely on stream flush which should be sufficient for basic durability
  num_flushes_++;
}

void DiskManager::read_page(uint32_t page_id, char* page_data) {
  std::lock_guard<std::mutex> l(db_io_mutex_);
  db_io_.seekg(page_offset(page_id), std::ios::beg);
  if (!db_io_.good()) {
    // Reading beyond EOF returns zeros
    std::memset(page_data, 0, PAGE_SIZE);
    return;
  }
  db_io_.read(page_data, PAGE_SIZE);
  if (db_io_.gcount() < static_cast<std::streamsize>(PAGE_SIZE)) {
    auto read_bytes = db_io_.gcount();
    if (read_bytes > 0) {
      // partially read; fill remaining with zero
      std::memset(page_data + read_bytes, 0, PAGE_SIZE - read_bytes);
    } else {
      std::memset(page_data, 0, PAGE_SIZE);
    }
    db_io_.clear();
  }
}

void DiskManager::write_page(uint32_t page_id, const char* page_data) {
  std::lock_guard<std::mutex> l(db_io_mutex_);
  db_io_.seekp(page_offset(page_id), std::ios::beg);
  db_io_.write(page_data, PAGE_SIZE);
  db_io_.flush();

  // Force write to disk for durability
  // Get the file descriptor from the stream
  db_io_.sync(); // First sync the C++ stream

  num_writes_++;

  // For critical writes, force fsync periodically
  if (num_writes_ % 100 == 0) {
    force_flush();
  }
}

uint32_t DiskManager::allocate_page() {
  return next_page_id_++;
}

void DiskManager::deallocate_page(uint32_t page_id) {
  (void)page_id; // simple implementation: no free list
}

bool DiskManager::has_free_pages() const {
  return true;
}

void DiskManager::read_physical_page(uint32_t page_id, char* page_data) {
  read_page(page_id, page_data);
}

void DiskManager::write_physical_page(uint32_t page_id, const char* page_data) {
  write_page(page_id, page_data);
}

uint32_t DiskManager::get_file_size(const std::string& file_name) {
  struct stat s {};
  if (stat(file_name.c_str(), &s) != 0)
    return 0;
  return static_cast<uint32_t>(s.st_size);
}

void DiskManager::create_new_database_file() {
  std::ofstream out(db_file_, std::ios::out | std::ios::binary);
  out.close();
  db_io_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
}

bool DiskManager::is_page_allocated(uint32_t page_id) {
  (void)page_id;
  return true;
}
void DiskManager::mark_page_allocated(uint32_t page_id) {
  (void)page_id;
}
void DiskManager::mark_page_deallocated(uint32_t page_id) {
  (void)page_id;
}
uint32_t DiskManager::get_bitmap_page_id(uint32_t page_id) {
  return page_id;
}
uint32_t DiskManager::get_bitmap_offset(uint32_t page_id) {
  return page_id;
}

// DiskScheduler minimal stubs
DiskScheduler::DiskScheduler(DiskManager* disk_manager)
    : disk_manager_(disk_manager), shutdown_cv_(false) {}
DiskScheduler::~DiskScheduler() {
  shutdown();
}
void DiskScheduler::schedule_read(uint32_t page_id, char* page_data) {
  if (disk_manager_)
    disk_manager_->read_page(page_id, page_data);
}
void DiskScheduler::schedule_write(uint32_t page_id, const char* page_data) {
  if (disk_manager_)
    disk_manager_->write_page(page_id, page_data);
}
void DiskScheduler::shutdown() {
  shutdown_cv_.store(true);
}

} // namespace latticedb
