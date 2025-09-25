#pragma once

#include "../buffer/buffer_pool_manager.h"
#include "../catalog/catalog_manager.h"
#include "../common/result.h"
#include "../concurrency/lock_manager.h"
#include "../query/query_executor.h"
#include "../query/query_planner.h"
#include "../query/sql_parser.h"
#include "../recovery/log_manager.h"
#include "../transaction/transaction.h"
#include "storage_engine.h"
#include <memory>
#include <string>

namespace latticedb {

class DatabaseEngine {
private:
  std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
  std::unique_ptr<DiskManager> disk_manager_;
  std::unique_ptr<LockManager> lock_manager_;
  std::unique_ptr<LogManager> log_manager_;
  std::unique_ptr<Catalog> catalog_;
  std::unique_ptr<StorageEngine> storage_engine_;
  std::unique_ptr<QueryPlanner> query_planner_;
  std::unique_ptr<QueryExecutor> query_executor_;
  std::unique_ptr<SQLParser> sql_parser_;
  std::unique_ptr<TransactionContext> transaction_context_;

  std::string database_file_;
  bool enable_logging_;

  // Simple temporal snapshots per table: tx_id -> table snapshot
  int current_tx_id_ = 0;
  std::unordered_map<std::string, std::unordered_map<int, std::vector<Tuple>>> table_snapshots_;

public:
  explicit DatabaseEngine(const std::string& database_file = DEFAULT_DB_FILE,
                          bool enable_logging = true,
                          StorageEngineType storage_type = StorageEngineType::ROW_STORE);

  ~DatabaseEngine();

  bool initialize();

  void shutdown();

  QueryResult execute_sql(const std::string& sql, Transaction* txn = nullptr);

  Transaction* begin_transaction(IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED);

  bool commit_transaction(Transaction* txn);

  bool abort_transaction(Transaction* txn);

  bool create_table(const std::string& table_name, const Schema& schema,
                    Transaction* txn = nullptr);

  bool drop_table(const std::string& table_name, Transaction* txn = nullptr);

  bool create_index(const std::string& index_name, const std::string& table_name,
                    const std::vector<std::string>& column_names, Transaction* txn = nullptr);

  bool drop_index(const std::string& index_name, Transaction* txn = nullptr);

  bool insert_tuple(const std::string& table_name, const Tuple& tuple, Transaction* txn = nullptr);

  bool update_tuple(const std::string& table_name, const RID& rid, const Tuple& tuple,
                    Transaction* txn = nullptr);

  bool delete_tuple(const std::string& table_name, const RID& rid, Transaction* txn = nullptr);

  std::vector<Tuple> select_tuples(const std::string& table_name, Transaction* txn = nullptr);

  std::vector<std::string> get_table_names() const;

  TableMetadata* get_table_info(const std::string& table_name);

  BufferPoolManager* get_buffer_pool_manager() {
    return buffer_pool_manager_.get();
  }
  Catalog* get_catalog() {
    return catalog_.get();
  }
  LockManager* get_lock_manager() {
    return lock_manager_.get();
  }
  LogManager* get_log_manager() {
    return log_manager_.get();
  }
  StorageEngine* get_storage_engine() {
    return storage_engine_.get();
  }
  TransactionContext* get_transaction_context() {
    return transaction_context_.get();
  }

  void enable_logging(bool enable) {
    enable_logging_ = enable;
  }
  void flush_logs() {
    if (log_manager_)
      log_manager_->flush();
  }
  void checkpoint();

private:
  void create_storage_engine(StorageEngineType storage_type);

  QueryResult execute_parsed_query(const ParsedQuery& query, Transaction* txn);

  void cleanup_transaction(Transaction* txn);

  bool validate_transaction_state(Transaction* txn);

  void snapshot_table(const std::string& table_name);
};

} // namespace latticedb
