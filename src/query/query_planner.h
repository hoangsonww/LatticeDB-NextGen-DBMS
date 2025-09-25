#pragma once

#include "../catalog/catalog_manager.h"
#include "../types/schema.h"
#include "sql_parser.h"
#include <memory>
#include <vector>

namespace latticedb {

enum class PlanNodeType {
  INVALID,
  TABLE_SCAN,
  INDEX_SCAN,
  PROJECTION,
  FILTER,
  NESTED_LOOP_JOIN,
  HASH_JOIN,
  SORT,
  LIMIT,
  INSERT,
  UPDATE,
  DELETE,
  AGGREGATE
};

enum class AggregationType { COUNT, SUM, MIN, MAX, AVG };

class PlanNode {
public:
  PlanNodeType type;
  std::shared_ptr<Schema> output_schema;
  std::vector<std::unique_ptr<PlanNode>> children;

  explicit PlanNode(PlanNodeType t) : type(t) {}
  virtual ~PlanNode() = default;

  virtual std::string to_string(int indent = 0) const = 0;
};

class TableScanPlanNode : public PlanNode {
public:
  table_oid_t table_oid;
  std::string table_name;
  std::unique_ptr<Expression> predicate;

  TableScanPlanNode(table_oid_t oid, const std::string& name, std::shared_ptr<Schema> schema)
      : PlanNode(PlanNodeType::TABLE_SCAN), table_oid(oid), table_name(name) {
    output_schema = schema;
  }

  std::string to_string(int indent = 0) const override;
};

class IndexScanPlanNode : public PlanNode {
public:
  index_oid_t index_oid;
  std::string index_name;
  table_oid_t table_oid;
  std::unique_ptr<Expression> predicate;

  IndexScanPlanNode(index_oid_t idx_oid, const std::string& idx_name, table_oid_t tbl_oid)
      : PlanNode(PlanNodeType::INDEX_SCAN), index_oid(idx_oid), index_name(idx_name),
        table_oid(tbl_oid) {}

  std::string to_string(int indent = 0) const override;
};

class ProjectionPlanNode : public PlanNode {
public:
  std::vector<std::unique_ptr<Expression>> expressions;

  ProjectionPlanNode(std::vector<std::unique_ptr<Expression>> exprs, std::shared_ptr<Schema> schema)
      : PlanNode(PlanNodeType::PROJECTION), expressions(std::move(exprs)) {
    output_schema = schema;
  }

  std::string to_string(int indent = 0) const override;
};

class FilterPlanNode : public PlanNode {
public:
  std::unique_ptr<Expression> predicate;

  FilterPlanNode(std::unique_ptr<Expression> pred, std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::FILTER), predicate(std::move(pred)) {
    children.push_back(std::move(child));
    output_schema = children[0]->output_schema;
  }

  std::string to_string(int indent = 0) const override;
};

class SortPlanNode : public PlanNode {
public:
  std::vector<OrderByClause> order_bys;

  SortPlanNode(std::vector<OrderByClause> orders, std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::SORT), order_bys(std::move(orders)) {
    children.push_back(std::move(child));
    output_schema = children[0]->output_schema;
  }

  std::string to_string(int indent = 0) const override;
};

class LimitPlanNode : public PlanNode {
public:
  int limit;

  LimitPlanNode(int lim, std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::LIMIT), limit(lim) {
    children.push_back(std::move(child));
    output_schema = children[0]->output_schema;
  }

  std::string to_string(int indent = 0) const override;
};

class InsertPlanNode : public PlanNode {
public:
  table_oid_t table_oid;
  std::string table_name;
  std::vector<std::vector<Value>> values;

  InsertPlanNode(table_oid_t oid, const std::string& name, std::vector<std::vector<Value>> vals)
      : PlanNode(PlanNodeType::INSERT), table_oid(oid), table_name(name), values(std::move(vals)) {}

  std::string to_string(int indent = 0) const override;
};

class UpdatePlanNode : public PlanNode {
public:
  table_oid_t table_oid;
  std::string table_name;
  std::vector<std::pair<std::string, Value>> assignments;

  UpdatePlanNode(table_oid_t oid, const std::string& name,
                 std::vector<std::pair<std::string, Value>> assigns,
                 std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::UPDATE), table_oid(oid), table_name(name),
        assignments(std::move(assigns)) {
    children.push_back(std::move(child));
  }

  std::string to_string(int indent = 0) const override;
};

class DeletePlanNode : public PlanNode {
public:
  table_oid_t table_oid;
  std::string table_name;

  DeletePlanNode(table_oid_t oid, const std::string& name, std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::DELETE), table_oid(oid), table_name(name) {
    children.push_back(std::move(child));
  }

  std::string to_string(int indent = 0) const override;
};

class HashJoinPlanNode : public PlanNode {
public:
  std::unique_ptr<Expression> join_predicate;
  std::string left_key_column;
  std::string right_key_column;
  size_t left_join_key_idx;  // Index of join key in left schema
  size_t right_join_key_idx; // Index of join key in right schema
  JoinType join_type;

  HashJoinPlanNode(std::unique_ptr<Expression> predicate, const std::string& left_key,
                   const std::string& right_key, JoinType type, std::unique_ptr<PlanNode> left,
                   std::unique_ptr<PlanNode> right)
      : PlanNode(PlanNodeType::HASH_JOIN), join_predicate(std::move(predicate)),
        left_key_column(left_key), right_key_column(right_key), join_type(type) {
    children.push_back(std::move(left));
    children.push_back(std::move(right));
    // Combine schemas from both children
    if (children.size() == 2 && children[0]->output_schema && children[1]->output_schema) {
      // Set join key indices
      left_join_key_idx = children[0]->output_schema->get_column_index(left_key_column);
      right_join_key_idx = children[1]->output_schema->get_column_index(right_key_column);
      std::vector<Column> columns;
      for (uint32_t i = 0; i < children[0]->output_schema->column_count(); ++i) {
        columns.push_back(children[0]->output_schema->get_column(i));
      }
      for (uint32_t i = 0; i < children[1]->output_schema->column_count(); ++i) {
        columns.push_back(children[1]->output_schema->get_column(i));
      }
      output_schema = std::make_shared<Schema>(columns);
    }
  }

  std::string to_string(int indent = 0) const override;
};

class NestedLoopJoinPlanNode : public PlanNode {
public:
  std::unique_ptr<Expression> join_predicate;
  size_t left_join_key_idx;  // Index of join key in left schema (for simple equality)
  size_t right_join_key_idx; // Index of join key in right schema (for simple equality)
  JoinType join_type;

  NestedLoopJoinPlanNode(std::unique_ptr<Expression> predicate, JoinType type,
                         std::unique_ptr<PlanNode> left, std::unique_ptr<PlanNode> right)
      : PlanNode(PlanNodeType::NESTED_LOOP_JOIN), join_predicate(std::move(predicate)),
        join_type(type) {
    children.push_back(std::move(left));
    children.push_back(std::move(right));
    // Combine schemas
    if (children.size() == 2 && children[0]->output_schema && children[1]->output_schema) {
      std::vector<Column> columns;
      for (uint32_t i = 0; i < children[0]->output_schema->column_count(); ++i) {
        columns.push_back(children[0]->output_schema->get_column(i));
      }
      for (uint32_t i = 0; i < children[1]->output_schema->column_count(); ++i) {
        columns.push_back(children[1]->output_schema->get_column(i));
      }
      output_schema = std::make_shared<Schema>(columns);
    }
  }

  std::string to_string(int indent = 0) const override;
};

class AggregatePlanNode : public PlanNode {
public:
  std::vector<std::string> group_by_columns;
  std::vector<size_t> group_by_cols; // Column indices for GROUP BY
  std::vector<std::pair<AggregationType, std::string>> aggregates;
  std::vector<AggregationType> aggregate_types;             // Types of aggregations
  std::vector<size_t> aggregate_cols;                       // Column indices to aggregate
  std::vector<std::unique_ptr<Expression>> aggregate_exprs; // Aggregate expressions
  std::unique_ptr<Expression> having_predicate;

  AggregatePlanNode(std::vector<std::string> group_by,
                    std::vector<std::pair<AggregationType, std::string>> aggs,
                    std::unique_ptr<Expression> having, std::unique_ptr<PlanNode> child)
      : PlanNode(PlanNodeType::AGGREGATE), group_by_columns(std::move(group_by)),
        aggregates(std::move(aggs)), having_predicate(std::move(having)) {
    children.push_back(std::move(child));
    // Create output schema based on group by and aggregates
    if (!children.empty() && children[0]->output_schema) {
      std::vector<Column> columns;

      // Set up group by column indices
      for (const auto& col_name : group_by_columns) {
        auto idx = children[0]->output_schema->get_column_index(col_name);
        group_by_cols.push_back(idx);
        columns.push_back(children[0]->output_schema->get_column(idx));
      }

      // Set up aggregate column indices and types
      for (const auto& [agg_type, col_name] : aggregates) {
        aggregate_types.push_back(agg_type);
        if (!col_name.empty() && col_name != "*") {
          auto idx = children[0]->output_schema->get_column_index(col_name);
          aggregate_cols.push_back(idx);
        } else {
          aggregate_cols.push_back(0); // For COUNT(*)
        }

        std::string agg_col_name = get_agg_name(agg_type) + "_" + col_name;
        columns.emplace_back(agg_col_name, ValueType::BIGINT);
      }
      output_schema = std::make_shared<Schema>(columns);
    }
  }

  static std::string get_agg_name(AggregationType type) {
    switch (type) {
    case AggregationType::COUNT:
      return "COUNT";
    case AggregationType::SUM:
      return "SUM";
    case AggregationType::MIN:
      return "MIN";
    case AggregationType::MAX:
      return "MAX";
    case AggregationType::AVG:
      return "AVG";
    default:
      return "AGG";
    }
  }

  std::string to_string(int indent = 0) const override;
};

class QueryPlanner {
private:
  Catalog* catalog_;

public:
  explicit QueryPlanner(Catalog* catalog) : catalog_(catalog) {}

  std::unique_ptr<PlanNode> plan_query(const ParsedQuery& query);

private:
  std::unique_ptr<PlanNode> plan_select(const SelectQuery& query);
  std::unique_ptr<PlanNode> plan_insert(const InsertQuery& query);
  std::unique_ptr<PlanNode> plan_update(const UpdateQuery& query);
  std::unique_ptr<PlanNode> plan_delete(const DeleteQuery& query);

  std::unique_ptr<PlanNode> optimize_plan(std::unique_ptr<PlanNode> plan);

  std::unique_ptr<PlanNode> push_down_filter(std::unique_ptr<PlanNode> plan,
                                             std::unique_ptr<Expression> filter);

  bool can_use_index(const Expression& predicate, const std::string& table_name,
                     index_oid_t* index_oid);

  std::shared_ptr<Schema> infer_schema(const std::vector<std::unique_ptr<Expression>>& expressions,
                                       const Schema& input_schema);

  double estimate_cardinality(const PlanNode& node);
  double estimate_cost(const PlanNode& node);
};

} // namespace latticedb