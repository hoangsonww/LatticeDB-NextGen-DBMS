#include "recovery_manager.h"
#include "../catalog/table_heap.h"
#include <algorithm>
#include <fstream>

namespace latticedb {

void RecoveryManager::recover() {
  // Three-phase recovery: Analysis, Redo, Undo
  analysis();
  redo();
  undo();
}

void RecoveryManager::analysis() {
  // Read log from disk
  auto log_records = read_log_file();

  // Rebuild transaction table and dirty page table
  for (const auto& record : log_records) {
    txn_id_t txn_id = record->get_txn_id();
    lsn_t lsn = record->get_lsn();

    // Track active transactions
    switch (record->get_log_record_type()) {
    case LogRecordType::BEGIN:
      active_txn_table_[txn_id] = lsn;
      break;

    case LogRecordType::COMMIT:
    case LogRecordType::ABORT:
      active_txn_table_.erase(txn_id);
      break;

    case LogRecordType::INSERT:
    case LogRecordType::DELETE:
    case LogRecordType::UPDATE:
      // Track dirty pages
      if (auto* insert_rec = dynamic_cast<InsertLogRecord*>(record.get())) {
        dirty_pages_.insert(insert_rec->get_insert_rid());
      } else if (auto* delete_rec = dynamic_cast<DeleteLogRecord*>(record.get())) {
        dirty_pages_.insert(delete_rec->get_delete_rid());
      } else if (auto* update_rec = dynamic_cast<UpdateLogRecord*>(record.get())) {
        dirty_pages_.insert(update_rec->get_update_rid());
      }
      break;

    default:
      break;
    }

    // Store log record for later use
    lsn_mapping_[lsn] = record.get();
  }
}

void RecoveryManager::redo() {
  // Redo all operations from the log
  std::vector<lsn_t> lsns;
  for (const auto& [lsn, record] : lsn_mapping_) {
    lsns.push_back(lsn);
  }
  std::sort(lsns.begin(), lsns.end());

  for (lsn_t lsn : lsns) {
    LogRecord* record = lsn_mapping_[lsn];

    switch (record->get_log_record_type()) {
    case LogRecordType::INSERT:
    case LogRecordType::DELETE:
    case LogRecordType::UPDATE:
      redo_log_record(record);
      break;

    default:
      // Skip non-data operations
      break;
    }
  }
}

void RecoveryManager::undo() {
  // Undo uncommitted transactions in reverse order
  while (!active_txn_table_.empty()) {
    // Find transaction with highest LSN
    txn_id_t max_txn = INVALID_TXN_ID;
    lsn_t max_lsn = INVALID_LSN;

    for (const auto& [txn_id, lsn] : active_txn_table_) {
      if (lsn > max_lsn) {
        max_lsn = lsn;
        max_txn = txn_id;
      }
    }

    if (max_txn == INVALID_TXN_ID)
      break;

    // Undo all operations for this transaction
    for (auto it = lsn_mapping_.rbegin(); it != lsn_mapping_.rend(); ++it) {
      LogRecord* record = it->second;
      if (record->get_txn_id() == max_txn) {
        undo_log_record(record);
      }
    }

    // Remove from active transactions
    active_txn_table_.erase(max_txn);
  }
}

void RecoveryManager::redo_log_record(LogRecord* log_record) {
  switch (log_record->get_log_record_type()) {
  case LogRecordType::INSERT:
    apply_insert(dynamic_cast<InsertLogRecord*>(log_record));
    break;

  case LogRecordType::DELETE:
    apply_delete(dynamic_cast<DeleteLogRecord*>(log_record));
    break;

  case LogRecordType::UPDATE:
    apply_update(dynamic_cast<UpdateLogRecord*>(log_record));
    break;

  default:
    break;
  }
}

void RecoveryManager::undo_log_record(LogRecord* log_record) {
  switch (log_record->get_log_record_type()) {
  case LogRecordType::INSERT:
    undo_insert(dynamic_cast<InsertLogRecord*>(log_record));
    break;

  case LogRecordType::DELETE:
    undo_delete(dynamic_cast<DeleteLogRecord*>(log_record));
    break;

  case LogRecordType::UPDATE:
    undo_update(dynamic_cast<UpdateLogRecord*>(log_record));
    break;

  default:
    break;
  }
}

void RecoveryManager::apply_insert(const InsertLogRecord* record) {
  // Apply insert to the database
  RID rid = record->get_insert_rid();
  Tuple tuple = record->get_insert_tuple();

  // Get the table heap (simplified - would need table ID)
  // In production, would look up table by ID from catalog
  // For now, just update page LSN
  update_page_lsn(rid.get_page_id(), record->get_lsn());
}

void RecoveryManager::apply_delete(const DeleteLogRecord* record) {
  RID rid = record->get_delete_rid();
  update_page_lsn(rid.get_page_id(), record->get_lsn());
}

void RecoveryManager::apply_update(const UpdateLogRecord* record) {
  RID rid = record->get_update_rid();
  update_page_lsn(rid.get_page_id(), record->get_lsn());
}

void RecoveryManager::undo_insert(const InsertLogRecord* record) {
  // Undo by deleting the inserted tuple
  RID rid = record->get_insert_rid();
  // Mark as deleted in the page
  update_page_lsn(rid.get_page_id(), record->get_lsn());
}

void RecoveryManager::undo_delete(const DeleteLogRecord* record) {
  // Undo by re-inserting the deleted tuple
  RID rid = record->get_delete_rid();
  Tuple tuple = record->get_delete_tuple();
  // Re-insert the tuple
  update_page_lsn(rid.get_page_id(), record->get_lsn());
}

void RecoveryManager::undo_update(const UpdateLogRecord* record) {
  // Undo by reverting to old value
  RID rid = record->get_update_rid();
  Tuple old_tuple = record->get_old_tuple();
  // Update back to old tuple
  update_page_lsn(rid.get_page_id(), record->get_lsn());
}

std::vector<std::unique_ptr<LogRecord>> RecoveryManager::read_log_file() {
  std::vector<std::unique_ptr<LogRecord>> records;

  std::ifstream log_file(LOG_FILE_NAME, std::ios::binary);
  if (!log_file.is_open()) {
    return records;
  }

  // Read all log records
  char buffer[4096];
  while (log_file.read(buffer, sizeof(LogRecordType))) {
    LogRecordType type;
    memcpy(&type, buffer, sizeof(LogRecordType));

    std::unique_ptr<LogRecord> record;

    switch (type) {
    case LogRecordType::INSERT:
      record = std::make_unique<InsertLogRecord>();
      break;
    case LogRecordType::DELETE:
      record = std::make_unique<DeleteLogRecord>();
      break;
    case LogRecordType::UPDATE:
      record = std::make_unique<UpdateLogRecord>();
      break;
    default:
      continue;
    }

    // Read the full record
    log_file.seekg(-sizeof(LogRecordType), std::ios::cur);
    uint32_t size = record->get_size();
    if (log_file.read(buffer, size)) {
      if (record->deserialize_from(buffer, size)) {
        records.push_back(std::move(record));
      }
    }
  }

  return records;
}

bool RecoveryManager::needs_redo(uint32_t page_id, lsn_t log_lsn) {
  // Check if page LSN is less than log LSN
  Page* page = buffer_pool_manager_->fetch_page(page_id);
  if (page == nullptr)
    return true;

  lsn_t page_lsn = page->get_lsn();
  buffer_pool_manager_->unpin_page(page_id, false);

  return page_lsn < log_lsn;
}

void RecoveryManager::update_page_lsn(uint32_t page_id, lsn_t lsn) {
  Page* page = buffer_pool_manager_->fetch_page(page_id);
  if (page != nullptr) {
    page->set_lsn(lsn);
    buffer_pool_manager_->unpin_page(page_id, true);
  }
}

void RecoveryManager::checkpoint() {
  // Create checkpoint record
  log_manager_->force_flush();

  // Write active transaction table to log
  // Write dirty page table to log
  // This would be a more complex operation in production
}

} // namespace latticedb