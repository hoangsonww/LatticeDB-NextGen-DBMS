#pragma once

#include "../common/config.h"
#include "../types/tuple.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>

namespace latticedb {

enum class LogRecordType {
  INVALID = 0,
  INSERT,
  DELETE,
  UPDATE,
  BEGIN,
  COMMIT,
  ABORT,
  NEWPAGE,
  CLR
};

class LogRecord {
public:
  LogRecord() = default;

  LogRecord(lsn_t lsn, lsn_t prev_lsn, txn_id_t txn_id, LogRecordType log_record_type)
      : lsn_(lsn), prev_lsn_(prev_lsn), txn_id_(txn_id), log_record_type_(log_record_type) {}

  virtual ~LogRecord() = default;

  lsn_t get_lsn() const {
    return lsn_;
  }
  lsn_t get_prev_lsn() const {
    return prev_lsn_;
  }
  txn_id_t get_txn_id() const {
    return txn_id_;
  }
  LogRecordType get_log_record_type() const {
    return log_record_type_;
  }

  virtual uint32_t get_size() const = 0;
  virtual void serialize_to(char* data) const = 0;
  virtual bool deserialize_from(const char* data, uint32_t size) = 0;
  virtual std::string to_string() const = 0;

  void set_lsn(lsn_t lsn) {
    lsn_ = lsn;
  }

protected:
  lsn_t lsn_{INVALID_LSN};
  lsn_t prev_lsn_{INVALID_LSN};
  txn_id_t txn_id_{INVALID_TXN_ID};
  LogRecordType log_record_type_{LogRecordType::INVALID};
};

class InsertLogRecord : public LogRecord {
public:
  InsertLogRecord() = default;

  InsertLogRecord(lsn_t lsn, lsn_t prev_lsn, txn_id_t txn_id, RID rid, const Tuple& tuple)
      : LogRecord(lsn, prev_lsn, txn_id, LogRecordType::INSERT), rid_(rid), tuple_(tuple) {}

  RID get_insert_rid() const {
    return rid_;
  }
  Tuple get_insert_tuple() const {
    return tuple_;
  }

  uint32_t get_size() const override;
  void serialize_to(char* data) const override;
  bool deserialize_from(const char* data, uint32_t size) override;
  std::string to_string() const override;

private:
  RID rid_;
  Tuple tuple_;
};

class DeleteLogRecord : public LogRecord {
public:
  DeleteLogRecord() = default;

  DeleteLogRecord(lsn_t lsn, lsn_t prev_lsn, txn_id_t txn_id, RID rid, const Tuple& tuple)
      : LogRecord(lsn, prev_lsn, txn_id, LogRecordType::DELETE), rid_(rid), tuple_(tuple) {}

  RID get_delete_rid() const {
    return rid_;
  }
  Tuple get_delete_tuple() const {
    return tuple_;
  }

  uint32_t get_size() const override;
  void serialize_to(char* data) const override;
  bool deserialize_from(const char* data, uint32_t size) override;
  std::string to_string() const override;

private:
  RID rid_;
  Tuple tuple_;
};

class UpdateLogRecord : public LogRecord {
public:
  UpdateLogRecord() = default;

  UpdateLogRecord(lsn_t lsn, lsn_t prev_lsn, txn_id_t txn_id, RID rid, const Tuple& old_tuple,
                  const Tuple& new_tuple)
      : LogRecord(lsn, prev_lsn, txn_id, LogRecordType::UPDATE), rid_(rid), old_tuple_(old_tuple),
        new_tuple_(new_tuple) {}

  RID get_update_rid() const {
    return rid_;
  }
  Tuple get_old_tuple() const {
    return old_tuple_;
  }
  Tuple get_new_tuple() const {
    return new_tuple_;
  }

  uint32_t get_size() const override;
  void serialize_to(char* data) const override;
  bool deserialize_from(const char* data, uint32_t size) override;
  std::string to_string() const override;

private:
  RID rid_;
  Tuple old_tuple_;
  Tuple new_tuple_;
};

class LogManager {
private:
  std::atomic<lsn_t> next_lsn_;
  std::atomic<lsn_t> persistent_lsn_;

  std::unique_ptr<char[]> log_buffer_;
  std::atomic<int> flush_buffer_size_;
  std::mutex latch_;
  std::condition_variable cv_;

  std::string log_name_;
  std::ofstream out_;

  bool enable_logging_;
  std::thread* flush_thread_;
  std::atomic<bool> flush_thread_on_;

public:
  explicit LogManager(std::string log_name, size_t log_buffer_size = LOG_BUFFER_SIZE)
      : next_lsn_(0), persistent_lsn_(INVALID_LSN),
        log_buffer_(std::make_unique<char[]>(log_buffer_size)), flush_buffer_size_(0),
        log_name_(std::move(log_name)), enable_logging_(false), flush_thread_(nullptr),
        flush_thread_on_(false) {}

  ~LogManager() {
    // Destructor simplified to avoid mutex issues during shutdown
    // The explicit shutdown() in DatabaseEngine already handles cleanup
  }

  void run_flush_thread();

  void stop_flush_thread();

  lsn_t append_log_record(LogRecord* log_record);

  void flush();

  void set_enable_logging(bool enable_logging);

  bool is_enabled() const {
    return enable_logging_;
  }

  lsn_t get_next_lsn() const {
    return next_lsn_;
  }

  lsn_t get_persistent_lsn() const {
    return persistent_lsn_;
  }

  void force_flush();

private:
  void flush_helper();

  static constexpr size_t FLUSH_BUFFER_SIZE = LOG_BUFFER_SIZE;
  std::unique_ptr<char[]> flush_buffer_;
};

} // namespace latticedb