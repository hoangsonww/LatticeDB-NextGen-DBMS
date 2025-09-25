#pragma once

#include "../catalog/table_heap.h"
#include "../common/result.h"
#include "../engine/storage_engine.h"
#include "../transaction/transaction.h"
#include "query_planner.h"
#include <memory>
#include <unordered_map>

namespace latticedb {

// Helper for hashing tuple values
struct TupleHasher {
  size_t operator()(const std::vector<Value>& values) const {
    size_t seed = 0;
    for (const auto& v : values) {
      seed ^= v.hash() + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

struct ValueHasher {
  size_t operator()(const Value& v) const {
    return v.hash();
  }
};

class ExecutionContext {
public:
  Transaction* transaction;
  Catalog* catalog;
  BufferPoolManager* buffer_pool_manager;
  LockManager* lock_manager;
  LogManager* log_manager;

  ExecutionContext(Transaction* txn, Catalog* cat, BufferPoolManager* bpm, LockManager* lock_mgr,
                   LogManager* log_mgr)
      : transaction(txn), catalog(cat), buffer_pool_manager(bpm), lock_manager(lock_mgr),
        log_manager(log_mgr) {}
};

class Executor {
public:
  explicit Executor(ExecutionContext* context) : context_(context) {}
  virtual ~Executor() = default;

  virtual void init() = 0;
  virtual bool next(Tuple* tuple, RID* rid) = 0;
  virtual const Schema& get_output_schema() const = 0;

protected:
  ExecutionContext* context_;
};

class TableScanExecutor : public Executor {
private:
  const TableScanPlanNode* plan_;
  std::unique_ptr<TableHeap> table_heap_;
  TableIterator table_iterator_;

public:
  TableScanExecutor(ExecutionContext* context, const TableScanPlanNode* plan);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  bool evaluate_predicate(const Tuple& tuple);
  bool evaluate_expression(const Expression& expr, const Tuple& tuple);
  Value evaluate_value(const Expression& expr, const Tuple& tuple);
};

class IndexScanExecutor : public Executor {
private:
  const IndexScanPlanNode* plan_;
  // iterator state handled by DatabaseEngine in minimal build

public:
  IndexScanExecutor(ExecutionContext* context, const IndexScanPlanNode* plan);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;
};

class ProjectionExecutor : public Executor {
private:
  const ProjectionPlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;

public:
  ProjectionExecutor(ExecutionContext* context, const ProjectionPlanNode* plan,
                     std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  Value evaluate_expression(const Expression& expr, const Tuple& input_tuple);
};

class FilterExecutor : public Executor {
private:
  const FilterPlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;

public:
  FilterExecutor(ExecutionContext* context, const FilterPlanNode* plan,
                 std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  bool evaluate_predicate(const Expression& expr, const Tuple& tuple);
  Value evaluate_expression(const Expression& expr, const Tuple& tuple);
};

class SortExecutor : public Executor {
private:
  const SortPlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;
  std::vector<Tuple> sorted_tuples_;
  size_t current_index_;

public:
  SortExecutor(ExecutionContext* context, const SortPlanNode* plan,
               std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  bool compare_tuples(const Tuple& left, const Tuple& right);
};

class LimitExecutor : public Executor {
private:
  const LimitPlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;
  int count_;

public:
  LimitExecutor(ExecutionContext* context, const LimitPlanNode* plan,
                std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;
};

class InsertExecutor : public Executor {
private:
  const InsertPlanNode* plan_;
  size_t current_index_;
  bool executed_;

public:
  InsertExecutor(ExecutionContext* context, const InsertPlanNode* plan);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;
};

class UpdateExecutor : public Executor {
private:
  const UpdatePlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;
  bool executed_;

public:
  UpdateExecutor(ExecutionContext* context, const UpdatePlanNode* plan,
                 std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;
};

class DeleteExecutor : public Executor {
private:
  const DeletePlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;
  bool executed_;

public:
  DeleteExecutor(ExecutionContext* context, const DeletePlanNode* plan,
                 std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;
};

class HashJoinExecutor : public Executor {
private:
  const HashJoinPlanNode* plan_;
  std::unique_ptr<Executor> left_executor_;
  std::unique_ptr<Executor> right_executor_;
  std::unordered_multimap<Value, Tuple, ValueHasher> hash_table_;
  std::vector<Tuple> output_buffer_;
  size_t buffer_index_;

public:
  HashJoinExecutor(ExecutionContext* context, const HashJoinPlanNode* plan,
                   std::unique_ptr<Executor> left, std::unique_ptr<Executor> right);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  void build_hash_table();
  Tuple combine_tuples(const Tuple& left, const Tuple& right);
};

class NestedLoopJoinExecutor : public Executor {
private:
  const NestedLoopJoinPlanNode* plan_;
  std::unique_ptr<Executor> left_executor_;
  std::unique_ptr<Executor> right_executor_;
  Tuple left_tuple_;
  bool has_left_tuple_;

public:
  NestedLoopJoinExecutor(ExecutionContext* context, const NestedLoopJoinPlanNode* plan,
                         std::unique_ptr<Executor> left, std::unique_ptr<Executor> right);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  bool evaluate_join_predicate(const Tuple& left, const Tuple& right);
  Tuple combine_tuples(const Tuple& left, const Tuple& right);
};

class AggregateExecutor : public Executor {
private:
  const AggregatePlanNode* plan_;
  std::unique_ptr<Executor> child_executor_;
  std::unordered_map<std::vector<Value>, std::vector<Value>, TupleHasher> groups_;
  std::unordered_map<std::vector<Value>, std::vector<Value>, TupleHasher>::iterator current_group_;
  bool executed_;

public:
  AggregateExecutor(ExecutionContext* context, const AggregatePlanNode* plan,
                    std::unique_ptr<Executor> child);

  void init() override;
  bool next(Tuple* tuple, RID* rid) override;
  const Schema& get_output_schema() const override;

private:
  std::vector<Value> get_group_key(const Tuple& tuple);
  void update_aggregates(const std::vector<Value>& key, const Tuple& tuple);
  Tuple make_output_tuple(const std::vector<Value>& key, const std::vector<Value>& aggregates);
};

class QueryExecutor {
public:
  explicit QueryExecutor(ExecutionContext* context) {
    (void)context;
  }

  std::unique_ptr<Executor> create_executor(const PlanNode* plan);

  std::vector<Tuple> execute_plan(const PlanNode* plan);

  QueryResult execute_query(const PlanNode* plan);

private:
  std::unique_ptr<Executor> create_table_scan_executor(const TableScanPlanNode* plan);
  std::unique_ptr<Executor> create_index_scan_executor(const IndexScanPlanNode* plan);
  std::unique_ptr<Executor> create_projection_executor(const ProjectionPlanNode* plan);
  std::unique_ptr<Executor> create_filter_executor(const FilterPlanNode* plan);
  std::unique_ptr<Executor> create_sort_executor(const SortPlanNode* plan);
  std::unique_ptr<Executor> create_limit_executor(const LimitPlanNode* plan);
  std::unique_ptr<Executor> create_insert_executor(const InsertPlanNode* plan);
  std::unique_ptr<Executor> create_update_executor(const UpdatePlanNode* plan);
  std::unique_ptr<Executor> create_delete_executor(const DeletePlanNode* plan);
  std::unique_ptr<Executor> create_hash_join_executor(const HashJoinPlanNode* plan);
  std::unique_ptr<Executor> create_nested_loop_join_executor(const NestedLoopJoinPlanNode* plan);
  std::unique_ptr<Executor> create_aggregate_executor(const AggregatePlanNode* plan);

  std::vector<std::string> get_column_names(const Schema& schema);
};

} // namespace latticedb
