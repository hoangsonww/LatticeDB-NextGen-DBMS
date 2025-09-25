#pragma once

#include "../catalog/catalog_manager.h"
#include "../query/query_planner.h"
#include "../statistics/table_statistics.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace latticedb {

enum class OptimizerHint {
  USE_INDEX,
  NO_INDEX,
  FORCE_NESTED_LOOP_JOIN,
  FORCE_HASH_JOIN,
  FORCE_MERGE_JOIN,
  PARALLEL_EXECUTION,
  NO_PARALLEL,
  MATERIALIZE_CTE,
  NO_MATERIALIZE_CTE
};

struct QueryHints {
  std::vector<OptimizerHint> hints;
  std::unordered_map<std::string, std::string> parameters;
};

class CostModel {
public:
  struct CostEstimate {
    double cpu_cost = 0.0;
    double io_cost = 0.0;
    double memory_cost = 0.0;
    double network_cost = 0.0;
    double total_cost = 0.0;
    double selectivity = 1.0;
    uint64_t estimated_rows = 0;
  };

  static constexpr double CPU_COST_PER_TUPLE = 0.01;
  static constexpr double IO_COST_PER_PAGE = 1.0;
  static constexpr double MEMORY_COST_PER_MB = 0.1;
  static constexpr double NETWORK_COST_PER_MB = 0.5;

  CostEstimate estimate_table_scan(const TableMetadata& table,
                                   const Expression* predicate = nullptr);
  CostEstimate estimate_index_scan(const IndexInfo& index, const Expression* predicate = nullptr);
  CostEstimate estimate_nested_loop_join(const CostEstimate& left, const CostEstimate& right);
  CostEstimate estimate_hash_join(const CostEstimate& left, const CostEstimate& right);
  CostEstimate estimate_merge_join(const CostEstimate& left, const CostEstimate& right);
  CostEstimate estimate_sort(const CostEstimate& input, size_t num_sort_columns);
  CostEstimate estimate_aggregation(const CostEstimate& input, size_t num_groups);

private:
  double estimate_selectivity(const Expression* predicate, const TableMetadata& table);
  double estimate_join_selectivity(const Expression* join_condition,
                                   const TableMetadata& left_table,
                                   const TableMetadata& right_table);
};

class AdvancedQueryOptimizer {
private:
  Catalog* catalog_;
  std::unique_ptr<CostModel> cost_model_;
  std::unique_ptr<TableStatistics> statistics_;

  // Optimization parameters
  struct OptimizerConfig {
    bool enable_predicate_pushdown = true;
    bool enable_projection_pushdown = true;
    bool enable_join_reordering = true;
    bool enable_subquery_flattening = true;
    bool enable_constant_folding = true;
    bool enable_common_subexpression_elimination = true;
    bool enable_parallel_execution = true;
    bool enable_vectorized_execution = true;
    uint32_t max_join_reorder_tables = 8;
    uint32_t parallel_threshold_rows = 10000;
  } config_;

public:
  explicit AdvancedQueryOptimizer(Catalog* catalog);
  ~AdvancedQueryOptimizer() = default;

  std::unique_ptr<PlanNode> optimize(std::unique_ptr<PlanNode> plan, const QueryHints& hints = {});

  void set_config(const OptimizerConfig& config) {
    config_ = config;
  }
  OptimizerConfig get_config() const {
    return config_;
  }

private:
  // Rule-based optimizations
  std::unique_ptr<PlanNode> apply_predicate_pushdown(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> apply_projection_pushdown(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> apply_constant_folding(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> apply_common_subexpression_elimination(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> flatten_subqueries(std::unique_ptr<PlanNode> plan);

  // Cost-based optimizations
  std::unique_ptr<PlanNode> optimize_join_order(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> choose_join_algorithm(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> choose_access_method(std::unique_ptr<PlanNode> plan);

  // Advanced optimizations
  std::unique_ptr<PlanNode> add_parallel_execution(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> add_vectorization(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> optimize_aggregation(std::unique_ptr<PlanNode> plan);
  std::unique_ptr<PlanNode> optimize_window_functions(std::unique_ptr<PlanNode> plan);

  // Join ordering using dynamic programming
  std::unique_ptr<PlanNode>
  dp_join_enumeration(const std::vector<std::unique_ptr<PlanNode>>& relations);

  // Utility functions
  bool can_push_predicate_through_join(const Expression& predicate, const PlanNode& join_node);
  std::vector<const Expression*> extract_predicates(const Expression& expr);
  double estimate_plan_cost(const PlanNode& plan);
  bool is_beneficial_index(const IndexInfo& index, const Expression& predicate);
};

// Parallel execution plan nodes
class ParallelScanPlanNode : public PlanNode {
public:
  table_oid_t table_oid;
  std::string table_name;
  std::unique_ptr<Expression> predicate;
  uint32_t num_workers;

  ParallelScanPlanNode(table_oid_t oid, const std::string& name, std::shared_ptr<Schema> schema,
                       uint32_t workers)
      : PlanNode(PlanNodeType::TABLE_SCAN), table_oid(oid), table_name(name), num_workers(workers) {
    output_schema = schema;
  }

  std::string to_string(int indent = 0) const override;
};

class ParallelHashJoinPlanNode : public PlanNode {
public:
  std::unique_ptr<Expression> join_predicate;
  uint32_t num_workers;
  size_t hash_table_size_hint;

  ParallelHashJoinPlanNode(std::unique_ptr<Expression> pred, uint32_t workers,
                           std::unique_ptr<PlanNode> left, std::unique_ptr<PlanNode> right)
      : PlanNode(PlanNodeType::HASH_JOIN), join_predicate(std::move(pred)), num_workers(workers),
        hash_table_size_hint(0) {
    children.push_back(std::move(left));
    children.push_back(std::move(right));
  }

  std::string to_string(int indent = 0) const override;
};

class VectorizedAggregationPlanNode : public PlanNode {
public:
  std::vector<std::unique_ptr<Expression>> group_by_expressions;
  std::vector<std::unique_ptr<Expression>> aggregate_expressions;
  std::vector<AggregateOperator::AggregateType> aggregate_types;
  uint32_t vector_size;

  VectorizedAggregationPlanNode(std::vector<std::unique_ptr<Expression>> group_bys,
                                std::vector<std::unique_ptr<Expression>> aggregates,
                                std::vector<AggregateOperator::AggregateType> types,
                                std::unique_ptr<PlanNode> child, uint32_t vec_size = 1024)
      : PlanNode(PlanNodeType::AGGREGATE), group_by_expressions(std::move(group_bys)),
        aggregate_expressions(std::move(aggregates)), aggregate_types(std::move(types)),
        vector_size(vec_size) {
    children.push_back(std::move(child));
  }

  std::string to_string(int indent = 0) const override;
};

// Window function support
class WindowFunctionPlanNode : public PlanNode {
public:
  enum class WindowFunctionType {
    ROW_NUMBER,
    RANK,
    DENSE_RANK,
    LAG,
    LEAD,
    FIRST_VALUE,
    LAST_VALUE,
    NTH_VALUE
  };

  WindowFunctionType function_type;
  std::vector<std::unique_ptr<Expression>> partition_by;
  std::vector<OrderByClause> order_by;
  std::unique_ptr<Expression> frame_start;
  std::unique_ptr<Expression> frame_end;

  WindowFunctionPlanNode(WindowFunctionType type,
                         std::vector<std::unique_ptr<Expression>> partition,
                         std::vector<OrderByClause> order, std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::AGGREGATE), function_type(type), partition_by(std::move(partition)),
        order_by(std::move(order)) {
    children.push_back(std::move(child));
  }

  std::string to_string(int indent = 0) const override;
};

// Common Table Expression (CTE) support
class CTEPlanNode : public PlanNode {
public:
  std::string cte_name;
  std::unique_ptr<PlanNode> cte_definition;
  bool materialized;

  CTEPlanNode(const std::string& name, std::unique_ptr<PlanNode> definition, bool mat = true)
      : PlanNode(PlanNodeType::PROJECTION), cte_name(name), cte_definition(std::move(definition)),
        materialized(mat) {}

  std::string to_string(int indent = 0) const override;
};

} // namespace latticedb