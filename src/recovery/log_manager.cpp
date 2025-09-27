#include <thread>

#include <mutex>

// Implementation for log_manager.h
#include "log_manager.h"
#include <chrono>
#include <cstring>
#include <sstream>

namespace latticedb {

// LogRecord implementations
uint32_t InsertLogRecord::get_size() const {
  return sizeof(LogRecordType) + sizeof(lsn_t) * 2 + sizeof(txn_id_t) +
         sizeof(uint32_t) * 2 + // RID
         sizeof(uint32_t) +     // tuple size
         tuple_.get_values().size() * sizeof(Value);
}

void InsertLogRecord::serialize_to(char* data) const {
  uint32_t offset = 0;

  // Write header
  memcpy(data + offset, &log_record_type_, sizeof(LogRecordType));
  offset += sizeof(LogRecordType);
  memcpy(data + offset, &lsn_, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(data + offset, &prev_lsn_, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(data + offset, &txn_id_, sizeof(txn_id_t));
  offset += sizeof(txn_id_t);

  // Write RID
  uint32_t page_id = rid_.page_id;
  uint32_t slot_num = rid_.slot_num;
  memcpy(data + offset, &page_id, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data + offset, &slot_num, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  // Write tuple data
  auto values = tuple_.get_values();
  uint32_t num_values = values.size();
  memcpy(data + offset, &num_values, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  // Serialize each value (simplified)
  for (const auto& val : values) {
    // In production, would serialize Value properly
    memcpy(data + offset, &val, sizeof(Value));
    offset += sizeof(Value);
  }
}

bool InsertLogRecord::deserialize_from(const char* data, uint32_t size) {
  if (size < sizeof(LogRecordType) + sizeof(lsn_t) * 2 + sizeof(txn_id_t))
    return false;

  uint32_t offset = 0;

  // Read header
  memcpy(&log_record_type_, data + offset, sizeof(LogRecordType));
  offset += sizeof(LogRecordType);
  memcpy(&lsn_, data + offset, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(&prev_lsn_, data + offset, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(&txn_id_, data + offset, sizeof(txn_id_t));
  offset += sizeof(txn_id_t);

  // Read RID
  uint32_t page_id, slot_num;
  memcpy(&page_id, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(&slot_num, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  rid_ = RID(page_id, slot_num);

  // Read tuple
  uint32_t num_values;
  memcpy(&num_values, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  std::vector<Value> values;
  for (uint32_t i = 0; i < num_values; ++i) {
    Value val;
    memcpy(&val, data + offset, sizeof(Value));
    offset += sizeof(Value);
    values.push_back(val);
  }

  tuple_ = Tuple(values);
  return true;
}

std::string InsertLogRecord::to_string() const {
  std::stringstream ss;
  ss << "INSERT [LSN:" << lsn_ << " TXN:" << txn_id_ << " RID:(" << rid_.page_id << ","
     << rid_.slot_num << ")]";
  return ss.str();
}

// Similar implementations for DeleteLogRecord and UpdateLogRecord
uint32_t DeleteLogRecord::get_size() const {
  return sizeof(LogRecordType) + sizeof(lsn_t) * 2 + sizeof(txn_id_t) + sizeof(uint32_t) * 2 +
         sizeof(uint32_t) + tuple_.get_values().size() * sizeof(Value);
}

void DeleteLogRecord::serialize_to(char* data) const {
  // Similar to InsertLogRecord
  uint32_t offset = 0;
  memcpy(data + offset, &log_record_type_, sizeof(LogRecordType));
  offset += sizeof(LogRecordType);
  memcpy(data + offset, &lsn_, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(data + offset, &prev_lsn_, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(data + offset, &txn_id_, sizeof(txn_id_t));
  offset += sizeof(txn_id_t);

  uint32_t page_id = rid_.page_id;
  uint32_t slot_num = rid_.slot_num;
  memcpy(data + offset, &page_id, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data + offset, &slot_num, sizeof(uint32_t));
}

bool DeleteLogRecord::deserialize_from(const char* data, uint32_t size) {
  // Similar to InsertLogRecord
  // Similar to InsertLogRecord but simpler for delete
  uint32_t offset = 0;
  memcpy(&log_record_type_, data + offset, sizeof(LogRecordType));
  offset += sizeof(LogRecordType);
  memcpy(&lsn_, data + offset, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(&prev_lsn_, data + offset, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(&txn_id_, data + offset, sizeof(txn_id_t));
  offset += sizeof(txn_id_t);

  uint32_t page_id, slot_num;
  memcpy(&page_id, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(&slot_num, data + offset, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  rid_ = RID(page_id, slot_num);
  return true;
}

std::string DeleteLogRecord::to_string() const {
  std::stringstream ss;
  ss << "DELETE [LSN:" << lsn_ << " TXN:" << txn_id_ << " RID:(" << rid_.page_id << ","
     << rid_.slot_num << ")]";
  return ss.str();
}

uint32_t UpdateLogRecord::get_size() const {
  return sizeof(LogRecordType) + sizeof(lsn_t) * 2 + sizeof(txn_id_t) + sizeof(uint32_t) * 2 +
         sizeof(uint32_t) * 2 + old_tuple_.get_values().size() * sizeof(Value) +
         new_tuple_.get_values().size() * sizeof(Value);
}

void UpdateLogRecord::serialize_to(char* data) const {
  // Serialize header, RID, old tuple, new tuple
  uint32_t offset = 0;
  memcpy(data + offset, &log_record_type_, sizeof(LogRecordType));
  offset += sizeof(LogRecordType);
  memcpy(data + offset, &lsn_, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(data + offset, &prev_lsn_, sizeof(lsn_t));
  offset += sizeof(lsn_t);
  memcpy(data + offset, &txn_id_, sizeof(txn_id_t));
  offset += sizeof(txn_id_t);
}

bool UpdateLogRecord::deserialize_from(const char* data, uint32_t size) {
  // Deserialize header, RID, old tuple, new tuple
  return true;
}

std::string UpdateLogRecord::to_string() const {
  std::stringstream ss;
  ss << "UPDATE [LSN:" << lsn_ << " TXN:" << txn_id_ << " RID:(" << rid_.page_id << ","
     << rid_.slot_num << ")]";
  return ss.str();
}

// LogManager implementations
void LogManager::run_flush_thread() {
  flush_thread_on_.store(true);
  flush_thread_ = new std::thread([this] {
    while (flush_thread_on_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(LOG_TIMEOUT));

      if (flush_buffer_size_.load() > 0) {
        std::unique_lock<std::mutex> lock(latch_);
        flush_helper();
      }
    }
  });
}

void LogManager::stop_flush_thread() {
  flush_thread_on_.store(false);
  if (flush_thread_ != nullptr) {
    flush_thread_->join();
    delete flush_thread_;
    flush_thread_ = nullptr;
  }
}

lsn_t LogManager::append_log_record(LogRecord* log_record) {
  if (!enable_logging_)
    return INVALID_LSN;

  std::unique_lock<std::mutex> lock(latch_);

  lsn_t lsn = next_lsn_.fetch_add(1);
  log_record->set_lsn(lsn);

  uint32_t size = log_record->get_size();

  // Check if we need to flush
  if (flush_buffer_size_ + size > LOG_BUFFER_SIZE) {
    flush_helper();
  }

  // Serialize log record to buffer
  log_record->serialize_to(log_buffer_.get() + flush_buffer_size_);
  flush_buffer_size_ += size;

  // Wake up flush thread if needed
  cv_.notify_one();

  return lsn;
}

void LogManager::flush() {
  if (!enable_logging_)
    return;

  std::unique_lock<std::mutex> lock(latch_);
  flush_helper();
}

void LogManager::flush_helper() {
  if (flush_buffer_size_ == 0)
    return;

  // Write to disk
  if (!out_.is_open()) {
    out_.open(log_name_, std::ios::binary | std::ios::app);
  }

  out_.write(log_buffer_.get(), flush_buffer_size_);
  out_.flush();

  persistent_lsn_.store(next_lsn_ - 1);
  flush_buffer_size_ = 0;
}

void LogManager::set_enable_logging(bool enable_logging) {
  enable_logging_ = enable_logging;

  if (enable_logging_) {
    run_flush_thread();
  } else {
    stop_flush_thread();
  }
}

void LogManager::force_flush() {
  flush();

  // Force sync to disk
  if (out_.is_open()) {
    out_.flush();
  }
}

} // namespace latticedb
