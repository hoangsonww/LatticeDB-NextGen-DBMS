// Implementation for transaction.h
#include "transaction.h"
#include <mutex>

namespace latticedb {

Transaction* TransactionContext::begin(Transaction* txn, IsolationLevel isolation_level) {
  std::lock_guard<std::mutex> l(txn_map_mutex_);
  txn_id_t id = get_next_txn_id();
  auto t = std::make_unique<Transaction>(id, isolation_level);
  Transaction* ptr = t.get();
  running_txns_[id] = std::move(t);
  (void)txn;
  return ptr;
}

void TransactionContext::commit(Transaction* txn) {
  if (!txn)
    return;
  txn->set_state(TransactionState::COMMITTED);
  remove_txn(txn->get_transaction_id());
}

void TransactionContext::abort(Transaction* txn) {
  if (!txn)
    return;
  txn->set_state(TransactionState::ABORTED);
  remove_txn(txn->get_transaction_id());
}

void TransactionContext::block_all_transactions() {}
void TransactionContext::resume_all_transactions() {}

Transaction* TransactionContext::get_transaction(txn_id_t txn_id) {
  auto it = running_txns_.find(txn_id);
  if (it == running_txns_.end())
    return nullptr;
  return it->second.get();
}

void TransactionContext::remove_txn(txn_id_t txn_id) {
  std::lock_guard<std::mutex> l(txn_map_mutex_);
  running_txns_.erase(txn_id);
}

} // namespace latticedb
