#pragma once

#include "../engine/storage_engine.h"
#include "../transaction/transaction.h"
#include "log_manager.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace latticedb {

class RecoveryManager {
private:
  LogManager* log_manager_;
  StorageEngine* storage_engine_;
  BufferPoolManager* buffer_pool_manager_;

  // Recovery state
  std::unordered_map<txn_id_t, lsn_t> active_txn_table_;
  std::unordered_map<lsn_t, LogRecord*> lsn_mapping_;
  std::unordered_set<RID> dirty_pages_;

public:
  RecoveryManager(LogManager* log_manager, StorageEngine* storage_engine,
                  BufferPoolManager* buffer_pool_manager)
      : log_manager_(log_manager), storage_engine_(storage_engine),
        buffer_pool_manager_(buffer_pool_manager) {}

  ~RecoveryManager() = default;

  // Perform crash recovery
  void recover();

  // Checkpoint the database
  void checkpoint();

  // Redo phase of recovery
  void redo();

  // Undo phase of recovery
  void undo();

private:
  // Analysis phase to rebuild state
  void analysis();

  // Apply a log record during redo
  void redo_log_record(LogRecord* log_record);

  // Undo a log record during undo
  void undo_log_record(LogRecord* log_record);

  // Read log from disk
  std::vector<std::unique_ptr<LogRecord>> read_log_file();

  // Parse log record from buffer
  std::unique_ptr<LogRecord> parse_log_record(const char* data, uint32_t size);

  // Helper to apply insert
  void apply_insert(const InsertLogRecord* record);

  // Helper to apply delete
  void apply_delete(const DeleteLogRecord* record);

  // Helper to apply update
  void apply_update(const UpdateLogRecord* record);

  // Helper to undo insert
  void undo_insert(const InsertLogRecord* record);

  // Helper to undo delete
  void undo_delete(const DeleteLogRecord* record);

  // Helper to undo update
  void undo_update(const UpdateLogRecord* record);

  // Check if page needs redo
  bool needs_redo(uint32_t page_id, lsn_t log_lsn);

  // Update page LSN after redo
  void update_page_lsn(uint32_t page_id, lsn_t lsn);
};

} // namespace latticedb