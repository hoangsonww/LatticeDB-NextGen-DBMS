#pragma once

#include "query_planner.h"
#include "../engine/storage_engine.h"
#include "../transaction/transaction.h"
#include <memory>

namespace latticedb {

class ExecutionContext {
public:
    Transaction* transaction;
    Catalog* catalog;
    BufferPoolManager* buffer_pool_manager;
    LockManager* lock_manager;
    LogManager* log_manager;

    ExecutionContext(Transaction* txn, Catalog* cat, BufferPoolManager* bpm,
                    LockManager* lock_mgr, LogManager* log_mgr)
        : transaction(txn), catalog(cat), buffer_pool_manager(bpm),
          lock_manager(lock_mgr), log_manager(log_mgr) {}
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
    std::unique_ptr<StorageEngine::Iterator> iterator_;

public:
    TableScanExecutor(ExecutionContext* context, const TableScanPlanNode* plan);

    void init() override;
    bool next(Tuple* tuple, RID* rid) override;
    const Schema& get_output_schema() const override;

private:
    bool evaluate_predicate(const Tuple& tuple);
};

class IndexScanExecutor : public Executor {
private:
    const IndexScanPlanNode* plan_;
    std::unique_ptr<StorageEngine::Iterator> iterator_;

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

class QueryExecutor {
private:
    ExecutionContext* context_;

public:
    explicit QueryExecutor(ExecutionContext* context) : context_(context) {}

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

    std::vector<std::string> get_column_names(const Schema& schema);
};

}