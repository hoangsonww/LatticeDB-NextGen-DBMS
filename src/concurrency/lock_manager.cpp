#include <thread>

#include <mutex>

#include "lock_manager.h"
#include <algorithm>
#include <chrono>

namespace latticedb {

LockManager::LockManager() : enable_cycle_detection_(false) {
  // Disabled cycle detection thread to avoid shutdown issues
  // cycle_detection_thread_ = std::thread(&LockManager::run_cycle_detection, this);
}

LockManager::~LockManager() {
  // Simplified destructor to avoid mutex issues during shutdown
}

bool LockManager::lock_table(Transaction* txn, LockMode lock_mode, table_oid_t table_oid) {
  if (!txn || txn->get_state() == TransactionState::ABORTED) {
    return false;
  }

  if (txn->get_state() == TransactionState::SHRINKING) {
    txn->set_state(TransactionState::ABORTED);
    return false;
  }

  auto queue = get_table_lock_request_queue(table_oid);
  std::unique_lock<std::mutex> lock(queue->latch_);

  // Check for existing lock or upgrade
  for (auto& req : queue->request_queue_) {
    if (req->txn_id_ == txn->get_transaction_id()) {
      if (req->lock_mode_ == lock_mode) {
        return true;
      }
      if (can_lock_upgrade(req->lock_mode_, lock_mode)) {
        if (queue->upgrading_ != INVALID_TXN_ID) {
          txn->set_state(TransactionState::ABORTED);
          return false;
        }
        queue->upgrading_ = txn->get_transaction_id();
        req->lock_mode_ = lock_mode;
        req->granted_ = false;

        while (!req->granted_) {
          if (txn->get_state() == TransactionState::ABORTED) {
            queue->upgrading_ = INVALID_TXN_ID;
            return false;
          }
          queue->cv_.wait(lock);
        }
        queue->upgrading_ = INVALID_TXN_ID;
        return true;
      }
      txn->set_state(TransactionState::ABORTED);
      return false;
    }
  }

  // Create new lock request
  auto request = std::make_shared<LockRequest>(txn->get_transaction_id(), lock_mode, table_oid);
  queue->request_queue_.push_back(request);

  // Try to grant lock
  grant_lock(request, queue);

  // Wait if not granted
  while (!request->granted_) {
    if (txn->get_state() == TransactionState::ABORTED) {
      queue->request_queue_.erase(
          std::remove(queue->request_queue_.begin(), queue->request_queue_.end(), request),
          queue->request_queue_.end());
      queue->cv_.notify_all();
      return false;
    }
    queue->cv_.wait(lock);
  }

  // Record lock in transaction
  if (lock_mode == LockMode::EXCLUSIVE || lock_mode == LockMode::INTENTION_EXCLUSIVE ||
      lock_mode == LockMode::SHARED_INTENTION_EXCLUSIVE) {
    txn->get_exclusive_table_lock_set()->insert(table_oid);
  } else {
    txn->get_shared_table_lock_set()->insert(table_oid);
  }

  return true;
}

bool LockManager::unlock_table(Transaction* txn, table_oid_t table_oid) {
  if (!txn)
    return false;

  auto queue = get_table_lock_request_queue(table_oid);
  std::unique_lock<std::mutex> lock(queue->latch_);

  // Find and remove lock request
  auto it = std::find_if(queue->request_queue_.begin(), queue->request_queue_.end(),
                         [txn](const auto& req) {
                           return req->txn_id_ == txn->get_transaction_id() && req->granted_;
                         });

  if (it == queue->request_queue_.end()) {
    return false;
  }

  // Update transaction state
  if (txn->get_state() == TransactionState::GROWING) {
    txn->set_state(TransactionState::SHRINKING);
  }

  // Remove from transaction's lock sets
  txn->get_exclusive_table_lock_set()->erase(table_oid);
  txn->get_shared_table_lock_set()->erase(table_oid);

  // Remove lock request
  queue->request_queue_.erase(it);

  // Grant waiting locks
  for (auto& req : queue->request_queue_) {
    if (!req->granted_) {
      grant_lock(req, queue);
    }
  }

  queue->cv_.notify_all();
  return true;
}

bool LockManager::lock_row(Transaction* txn, LockMode lock_mode, table_oid_t table_oid, RID rid) {
  if (!txn || txn->get_state() == TransactionState::ABORTED) {
    return false;
  }

  if (txn->get_state() == TransactionState::SHRINKING) {
    txn->set_state(TransactionState::ABORTED);
    return false;
  }

  // Check table-level lock
  bool has_table_lock = false;
  if (lock_mode == LockMode::EXCLUSIVE) {
    has_table_lock = txn->is_table_exclusive_locked(table_oid) ||
                     txn->is_table_intention_exclusive_locked(table_oid) ||
                     txn->is_table_shared_intention_exclusive_locked(table_oid);
  } else {
    has_table_lock = txn->is_table_shared_locked(table_oid) ||
                     txn->is_table_intention_shared_locked(table_oid) ||
                     txn->is_table_shared_intention_exclusive_locked(table_oid);
  }

  if (!has_table_lock) {
    txn->set_state(TransactionState::ABORTED);
    return false;
  }

  auto queue = get_row_lock_request_queue(rid);
  std::unique_lock<std::mutex> lock(queue->latch_);

  // Create lock request
  auto request =
      std::make_shared<LockRequest>(txn->get_transaction_id(), lock_mode, table_oid, rid);
  queue->request_queue_.push_back(request);

  // Try to grant lock
  grant_lock(request, queue);

  // Wait if not granted
  while (!request->granted_) {
    if (txn->get_state() == TransactionState::ABORTED) {
      queue->request_queue_.erase(
          std::remove(queue->request_queue_.begin(), queue->request_queue_.end(), request),
          queue->request_queue_.end());
      queue->cv_.notify_all();
      return false;
    }
    queue->cv_.wait(lock);
  }

  // Record lock in transaction
  if (lock_mode == LockMode::EXCLUSIVE) {
    (*txn->get_exclusive_row_lock_set())[table_oid].insert(rid);
  } else {
    (*txn->get_shared_row_lock_set())[table_oid].insert(rid);
  }

  return true;
}

bool LockManager::unlock_row(Transaction* txn, table_oid_t table_oid, RID rid, bool force) {
  if (!txn)
    return false;

  auto queue = get_row_lock_request_queue(rid);
  std::unique_lock<std::mutex> lock(queue->latch_);

  // Find and remove lock request
  auto it = std::find_if(queue->request_queue_.begin(), queue->request_queue_.end(),
                         [txn, table_oid, rid](const auto& req) {
                           return req->txn_id_ == txn->get_transaction_id() &&
                                  req->oid_ == table_oid && req->rid_ == rid && req->granted_;
                         });

  if (it == queue->request_queue_.end()) {
    return false;
  }

  // Update transaction state
  if (!force && txn->get_state() == TransactionState::GROWING) {
    txn->set_state(TransactionState::SHRINKING);
  }

  // Remove from transaction's lock sets
  auto& ex_set = (*txn->get_exclusive_row_lock_set())[table_oid];
  auto& sh_set = (*txn->get_shared_row_lock_set())[table_oid];
  ex_set.erase(rid);
  sh_set.erase(rid);

  // Remove lock request
  queue->request_queue_.erase(it);

  // Grant waiting locks
  for (auto& req : queue->request_queue_) {
    if (!req->granted_) {
      grant_lock(req, queue);
    }
  }

  queue->cv_.notify_all();
  return true;
}

void LockManager::add_edge(txn_id_t t1, txn_id_t t2) {
  std::lock_guard<std::mutex> lock(waits_for_latch_);
  waits_for_[t1].insert(t2);
}

void LockManager::remove_edge(txn_id_t t1, txn_id_t t2) {
  std::lock_guard<std::mutex> lock(waits_for_latch_);
  if (waits_for_.count(t1)) {
    waits_for_[t1].erase(t2);
    if (waits_for_[t1].empty()) {
      waits_for_.erase(t1);
    }
  }
}

bool LockManager::has_cycle(txn_id_t* txn_id) {
  std::lock_guard<std::mutex> lock(waits_for_latch_);
  std::unordered_set<txn_id_t> visited;
  std::unordered_set<txn_id_t> rec_stack;

  for (const auto& [txn, _] : waits_for_) {
    if (visited.find(txn) == visited.end()) {
      if (dfs(txn, txn, visited, rec_stack, txn_id)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<std::pair<txn_id_t, txn_id_t>> LockManager::get_edge_list() {
  std::lock_guard<std::mutex> lock(waits_for_latch_);
  std::vector<std::pair<txn_id_t, txn_id_t>> edges;
  for (const auto& [from, to_set] : waits_for_) {
    for (txn_id_t to : to_set) {
      edges.emplace_back(from, to);
    }
  }
  return edges;
}

void LockManager::run_cycle_detection() {
  while (enable_cycle_detection_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    txn_id_t victim;
    if (has_cycle(&victim)) {
      // Build wait-for graph from lock request queues
      build_wait_for_graph();

      // If cycle still exists after building graph, abort victim
      if (has_cycle(&victim)) {
        // Mark victim transaction for abort
        // This would need to coordinate with transaction manager
        // For now, just remove edges involving victim
        remove_transaction_edges(victim);
      }
    }
  }
}

void LockManager::build_wait_for_graph() {
  std::lock_guard<std::mutex> lock(waits_for_latch_);
  waits_for_.clear();

  // Build from table lock requests
  {
    std::lock_guard<std::mutex> table_lock(table_lock_map_latch_);
    for (const auto& [table_id, queue] : table_lock_map_) {
      std::lock_guard<std::mutex> queue_lock(queue->latch_);
      build_edges_for_queue(queue);
    }
  }

  // Build from row lock requests
  {
    std::lock_guard<std::mutex> row_lock(row_lock_map_latch_);
    for (const auto& [rid, queue] : row_lock_map_) {
      std::lock_guard<std::mutex> queue_lock(queue->latch_);
      build_edges_for_queue(queue);
    }
  }
}

void LockManager::build_edges_for_queue(std::shared_ptr<LockRequestQueue> queue) {
  // Find waiting transactions
  for (const auto& waiter : queue->request_queue_) {
    if (!waiter->granted_) {
      // This transaction is waiting
      // Find who it's waiting for (granted locks)
      for (const auto& holder : queue->request_queue_) {
        if (holder->granted_ && holder->txn_id_ != waiter->txn_id_) {
          // Check if locks are incompatible
          if (!are_locks_compatible(holder->lock_mode_, waiter->lock_mode_)) {
            // waiter is waiting for holder
            waits_for_[waiter->txn_id_].insert(holder->txn_id_);
          }
        }
      }
    }
  }
}

void LockManager::remove_transaction_edges(txn_id_t txn_id) {
  std::lock_guard<std::mutex> lock(waits_for_latch_);
  // Remove all edges from this transaction
  waits_for_.erase(txn_id);
  // Remove all edges to this transaction
  for (auto& [from, to_set] : waits_for_) {
    to_set.erase(txn_id);
  }
}

bool LockManager::are_locks_compatible(LockMode mode1, LockMode mode2) {
  if (mode1 == LockMode::SHARED) {
    return mode2 == LockMode::SHARED || mode2 == LockMode::INTENTION_SHARED;
  }
  if (mode1 == LockMode::EXCLUSIVE) {
    return false;
  }
  if (mode1 == LockMode::INTENTION_SHARED) {
    return mode2 != LockMode::EXCLUSIVE;
  }
  if (mode1 == LockMode::INTENTION_EXCLUSIVE) {
    return mode2 == LockMode::INTENTION_SHARED || mode2 == LockMode::INTENTION_EXCLUSIVE;
  }
  if (mode1 == LockMode::SHARED_INTENTION_EXCLUSIVE) {
    return mode2 == LockMode::INTENTION_SHARED;
  }
  return false;
}

bool LockManager::can_lock_upgrade(LockMode current, LockMode target) {
  if (current == LockMode::INTENTION_SHARED) {
    return target == LockMode::SHARED || target == LockMode::EXCLUSIVE ||
           target == LockMode::INTENTION_EXCLUSIVE ||
           target == LockMode::SHARED_INTENTION_EXCLUSIVE;
  }
  if (current == LockMode::SHARED) {
    return target == LockMode::EXCLUSIVE || target == LockMode::SHARED_INTENTION_EXCLUSIVE;
  }
  if (current == LockMode::INTENTION_EXCLUSIVE) {
    return target == LockMode::EXCLUSIVE || target == LockMode::SHARED_INTENTION_EXCLUSIVE;
  }
  if (current == LockMode::SHARED_INTENTION_EXCLUSIVE) {
    return target == LockMode::EXCLUSIVE;
  }
  return false;
}

void LockManager::grant_lock(std::shared_ptr<LockRequest> request,
                             std::shared_ptr<LockRequestQueue> queue) {
  // Check if lock can be granted
  bool can_grant = true;

  // Handle upgrade case
  if (queue->upgrading_ != INVALID_TXN_ID && request->txn_id_ != queue->upgrading_) {
    can_grant = false;
  }

  // Check compatibility with granted locks
  if (can_grant) {
    for (const auto& req : queue->request_queue_) {
      if (req->granted_ && req->txn_id_ != request->txn_id_) {
        if (!are_locks_compatible(req->lock_mode_, request->lock_mode_)) {
          can_grant = false;
          break;
        }
      }
    }
  }

  // Check for earlier waiting requests
  if (can_grant) {
    for (const auto& req : queue->request_queue_) {
      if (req == request)
        break;
      if (!req->granted_ && req->txn_id_ != request->txn_id_) {
        can_grant = false;
        break;
      }
    }
  }

  if (can_grant) {
    request->granted_ = true;
  }
}

bool LockManager::can_txn_take_lock(Transaction* txn, LockMode lock_mode) {
  if (!txn)
    return false;

  if (txn->get_state() == TransactionState::ABORTED) {
    return false;
  }

  if (txn->get_state() == TransactionState::SHRINKING) {
    if (lock_mode != LockMode::SHARED && lock_mode != LockMode::INTENTION_SHARED) {
      return false;
    }
  }

  return true;
}

void LockManager::unlock_table_helper(Transaction* txn, table_oid_t table_oid) {
  if (!txn)
    return;
  unlock_table(txn, table_oid);
}

void LockManager::unlock_row_helper(Transaction* txn, table_oid_t table_oid, RID rid) {
  if (!txn)
    return;
  unlock_row(txn, table_oid, rid, false);
}

std::shared_ptr<LockRequestQueue> LockManager::get_table_lock_request_queue(table_oid_t table_oid) {
  std::lock_guard<std::mutex> lock(table_lock_map_latch_);
  if (table_lock_map_.find(table_oid) == table_lock_map_.end()) {
    table_lock_map_[table_oid] = std::make_shared<LockRequestQueue>();
  }
  return table_lock_map_[table_oid];
}

std::shared_ptr<LockRequestQueue> LockManager::get_row_lock_request_queue(RID rid) {
  std::lock_guard<std::mutex> lock(row_lock_map_latch_);
  if (row_lock_map_.find(rid) == row_lock_map_.end()) {
    row_lock_map_[rid] = std::make_shared<LockRequestQueue>();
  }
  return row_lock_map_[rid];
}

bool LockManager::dfs(txn_id_t current, txn_id_t start, std::unordered_set<txn_id_t>& visited,
                      std::unordered_set<txn_id_t>& rec_stack, txn_id_t* victim) {
  visited.insert(current);
  rec_stack.insert(current);

  if (waits_for_.count(current)) {
    for (txn_id_t neighbor : waits_for_[current]) {
      if (rec_stack.find(neighbor) != rec_stack.end()) {
        // Found cycle - choose victim with highest txn_id
        *victim = std::max(current, neighbor);
        return true;
      }
      if (visited.find(neighbor) == visited.end()) {
        if (dfs(neighbor, start, visited, rec_stack, victim)) {
          return true;
        }
      }
    }
  }

  rec_stack.erase(current);
  return false;
}

} // namespace latticedb
