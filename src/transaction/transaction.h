#pragma once

#include "../common/config.h"
#include "../types/tuple.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace latticedb {

using txn_id_t = uint32_t;
using lsn_t = uint32_t;

enum class TransactionState {
  GROWING,   // acquiring locks
  SHRINKING, // releasing locks
  COMMITTED, // committed
  ABORTED    // aborted
};

class Transaction {
private:
  txn_id_t txn_id_;
  TransactionState state_;
  IsolationLevel isolation_level_;

  std::unordered_set<RID> shared_lock_set_;
  std::unordered_set<RID> exclusive_lock_set_;
  std::unordered_set<page_id_t> page_set_;
  std::unordered_set<table_oid_t> table_write_set_;
  std::unordered_set<table_oid_t> table_read_set_;

  // Table-level locks
  std::unordered_set<table_oid_t> shared_table_lock_set_;
  std::unordered_set<table_oid_t> exclusive_table_lock_set_;
  std::unordered_set<table_oid_t> intention_shared_table_lock_set_;
  std::unordered_set<table_oid_t> intention_exclusive_table_lock_set_;
  std::unordered_set<table_oid_t> shared_intention_exclusive_table_lock_set_;

  // Row-level locks by table
  std::unordered_map<table_oid_t, std::unordered_set<RID>> shared_row_lock_set_;
  std::unordered_map<table_oid_t, std::unordered_set<RID>> exclusive_row_lock_set_;

  lsn_t prev_lsn_;
  std::atomic<bool> deleted_;

  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  AbortReason abort_reason_;

public:
  explicit Transaction(txn_id_t txn_id,
                       IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED)
      : txn_id_(txn_id), state_(TransactionState::GROWING), isolation_level_(isolation_level),
        prev_lsn_(INVALID_LSN), deleted_(false), start_time_(std::chrono::steady_clock::now()),
        abort_reason_(AbortReason::USER_ABORT) {}

  ~Transaction() = default;

  DISALLOW_COPY_AND_MOVE(Transaction)

  txn_id_t get_transaction_id() const {
    return txn_id_;
  }

  TransactionState get_state() const {
    return state_;
  }
  void set_state(TransactionState state) {
    state_ = state;
  }

  IsolationLevel get_isolation_level() const {
    return isolation_level_;
  }

  const std::unordered_set<RID>& get_shared_lock_set() const {
    return shared_lock_set_;
  }
  const std::unordered_set<RID>& get_exclusive_lock_set() const {
    return exclusive_lock_set_;
  }

  void add_to_shared_lock_set(const RID& rid) {
    shared_lock_set_.insert(rid);
  }
  void add_to_exclusive_lock_set(const RID& rid) {
    exclusive_lock_set_.insert(rid);
  }

  void remove_from_shared_lock_set(const RID& rid) {
    shared_lock_set_.erase(rid);
  }
  void remove_from_exclusive_lock_set(const RID& rid) {
    exclusive_lock_set_.erase(rid);
  }

  bool is_shared_locked(const RID& rid) const {
    return shared_lock_set_.count(rid) != 0;
  }
  bool is_exclusive_locked(const RID& rid) const {
    return exclusive_lock_set_.count(rid) != 0;
  }

  const std::unordered_set<page_id_t>& get_page_set() const {
    return page_set_;
  }
  void add_to_page_set(page_id_t page_id) {
    page_set_.insert(page_id);
  }

  const std::unordered_set<table_oid_t>& get_write_set() const {
    return table_write_set_;
  }
  const std::unordered_set<table_oid_t>& get_read_set() const {
    return table_read_set_;
  }

  void add_to_write_set(table_oid_t table_oid) {
    table_write_set_.insert(table_oid);
  }
  void add_to_read_set(table_oid_t table_oid) {
    table_read_set_.insert(table_oid);
  }

  lsn_t get_prev_lsn() const {
    return prev_lsn_;
  }
  void set_prev_lsn(lsn_t prev_lsn) {
    prev_lsn_ = prev_lsn;
  }

  bool is_deleted() const {
    return deleted_;
  }
  void set_deleted(bool deleted) {
    deleted_ = deleted;
  }

  std::chrono::time_point<std::chrono::steady_clock> get_start_time() const {
    return start_time_;
  }

  AbortReason get_abort_reason() const {
    return abort_reason_;
  }
  void set_abort_reason(AbortReason reason) {
    abort_reason_ = reason;
  }

  // Table-level lock accessors
  std::unordered_set<table_oid_t>* get_shared_table_lock_set() {
    return &shared_table_lock_set_;
  }
  std::unordered_set<table_oid_t>* get_exclusive_table_lock_set() {
    return &exclusive_table_lock_set_;
  }
  std::unordered_set<table_oid_t>* get_intention_shared_table_lock_set() {
    return &intention_shared_table_lock_set_;
  }
  std::unordered_set<table_oid_t>* get_intention_exclusive_table_lock_set() {
    return &intention_exclusive_table_lock_set_;
  }
  std::unordered_set<table_oid_t>* get_shared_intention_exclusive_table_lock_set() {
    return &shared_intention_exclusive_table_lock_set_;
  }

  // Row-level lock accessors
  std::unordered_map<table_oid_t, std::unordered_set<RID>>* get_shared_row_lock_set() {
    return &shared_row_lock_set_;
  }
  std::unordered_map<table_oid_t, std::unordered_set<RID>>* get_exclusive_row_lock_set() {
    return &exclusive_row_lock_set_;
  }

  // Check table lock status
  bool is_table_shared_locked(table_oid_t oid) const {
    return shared_table_lock_set_.count(oid) > 0;
  }
  bool is_table_exclusive_locked(table_oid_t oid) const {
    return exclusive_table_lock_set_.count(oid) > 0;
  }
  bool is_table_intention_shared_locked(table_oid_t oid) const {
    return intention_shared_table_lock_set_.count(oid) > 0;
  }
  bool is_table_intention_exclusive_locked(table_oid_t oid) const {
    return intention_exclusive_table_lock_set_.count(oid) > 0;
  }
  bool is_table_shared_intention_exclusive_locked(table_oid_t oid) const {
    return shared_intention_exclusive_table_lock_set_.count(oid) > 0;
  }
};

class TransactionContext {
private:
  std::unordered_map<txn_id_t, std::unique_ptr<Transaction>> running_txns_;
  std::atomic<txn_id_t> next_txn_id_;
  std::mutex txn_map_mutex_;

public:
  TransactionContext() : next_txn_id_(0) {}

  ~TransactionContext() = default;

  Transaction* begin(Transaction* txn = nullptr,
                     IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED);

  void commit(Transaction* txn);

  void abort(Transaction* txn);

  void block_all_transactions();

  void resume_all_transactions();

  Transaction* get_transaction(txn_id_t txn_id);

private:
  txn_id_t get_next_txn_id() {
    return next_txn_id_++;
  }

  void remove_txn(txn_id_t txn_id);
};

} // namespace latticedb
