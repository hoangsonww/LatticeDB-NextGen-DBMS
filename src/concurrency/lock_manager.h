#pragma once

#include "../common/config.h"
#include "../transaction/transaction.h"
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace latticedb {

class LockRequest {
public:
  LockRequest(txn_id_t txn_id, LockMode lock_mode, table_oid_t table_oid)
      : txn_id_(txn_id), lock_mode_(lock_mode), oid_(table_oid), granted_(false) {}

  LockRequest(txn_id_t txn_id, LockMode lock_mode, table_oid_t table_oid, RID rid)
      : txn_id_(txn_id), lock_mode_(lock_mode), oid_(table_oid), rid_(rid), granted_(false) {}

  txn_id_t txn_id_;
  LockMode lock_mode_;
  table_oid_t oid_;
  RID rid_;
  bool granted_;
};

class LockRequestQueue {
public:
  std::list<std::shared_ptr<LockRequest>> request_queue_;
  std::condition_variable cv_;
  txn_id_t upgrading_;
  std::mutex latch_;

  LockRequestQueue() : upgrading_(INVALID_TXN_ID) {}
};

class LockManager {
private:
  std::mutex table_lock_map_latch_;
  std::mutex row_lock_map_latch_;

  std::unordered_map<table_oid_t, std::shared_ptr<LockRequestQueue>> table_lock_map_;
  std::unordered_map<RID, std::shared_ptr<LockRequestQueue>> row_lock_map_;

  std::unordered_map<txn_id_t, std::unordered_set<table_oid_t>> txn_table_locks_;
  std::unordered_map<txn_id_t, std::unordered_set<RID>> txn_row_locks_;

public:
  LockManager();
  ~LockManager();

  enum class LockRequestType { LOCK, UNLOCK };

  bool lock_table(Transaction* txn, LockMode lock_mode, table_oid_t table_oid);

  bool unlock_table(Transaction* txn, table_oid_t table_oid);

  bool lock_row(Transaction* txn, LockMode lock_mode, table_oid_t table_oid, RID rid);

  bool unlock_row(Transaction* txn, table_oid_t table_oid, RID rid, bool force = false);

  void add_edge(txn_id_t t1, txn_id_t t2);

  void remove_edge(txn_id_t t1, txn_id_t t2);

  bool has_cycle(txn_id_t* txn_id);

  std::vector<std::pair<txn_id_t, txn_id_t>> get_edge_list();

  void run_cycle_detection();

private:
  std::mutex waits_for_latch_;
  std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> waits_for_;
  bool enable_cycle_detection_;
  std::thread cycle_detection_thread_;

  bool are_locks_compatible(LockMode l1, LockMode l2);

  bool can_lock_upgrade(LockMode curr_lock_mode, LockMode requested_lock_mode);

  void grant_lock(std::shared_ptr<LockRequest> lock_request,
                  std::shared_ptr<LockRequestQueue> lock_request_queue);

  bool can_txn_take_lock(Transaction* txn, LockMode lock_mode);

  void unlock_table_helper(Transaction* txn, table_oid_t table_oid);

  void unlock_row_helper(Transaction* txn, table_oid_t table_oid, RID rid);

  std::shared_ptr<LockRequestQueue> get_table_lock_request_queue(table_oid_t table_oid);

  std::shared_ptr<LockRequestQueue> get_row_lock_request_queue(RID rid);

  bool dfs(txn_id_t start_txn_id, txn_id_t current_txn_id, std::unordered_set<txn_id_t>& visited,
           std::unordered_set<txn_id_t>& on_path, txn_id_t* abort_txn_id);

  void build_wait_for_graph();
  void build_edges_for_queue(std::shared_ptr<LockRequestQueue> queue);
  void remove_transaction_edges(txn_id_t txn_id);
};

} // namespace latticedb
