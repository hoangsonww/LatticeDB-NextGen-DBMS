#include "query_planner.h"

namespace latticedb {

static std::string indent_str(int indent) {
  return std::string(indent, ' ');
}

std::unique_ptr<PlanNode> QueryPlanner::plan_query(const ParsedQuery& query) {
  switch (query.type) {
  case QueryType::SELECT:
    return plan_select(*query.select);
  case QueryType::INSERT:
    return plan_insert(*query.insert);
  default:
    return nullptr;
  }
}

std::unique_ptr<PlanNode> QueryPlanner::plan_select(const SelectQuery& query) {
  auto* table_meta = catalog_->get_table(query.table_name);
  if (!table_meta)
    return nullptr;
  auto scan = std::make_unique<TableScanPlanNode>(table_meta->get_oid(), query.table_name,
                                                  table_meta->get_schema_ptr());
  if (query.limit) {
    auto lim = std::make_unique<LimitPlanNode>(*query.limit, std::move(scan));
    return lim;
  }
  return scan;
}

std::unique_ptr<PlanNode> QueryPlanner::plan_insert(const InsertQuery& query) {
  auto* table_meta = catalog_->get_table(query.table_name);
  if (!table_meta)
    return nullptr;
  return std::make_unique<InsertPlanNode>(table_meta->get_oid(), query.table_name, query.values);
}

std::unique_ptr<PlanNode> QueryPlanner::plan_update(const UpdateQuery& /*query*/) {
  return nullptr;
}
std::unique_ptr<PlanNode> QueryPlanner::plan_delete(const DeleteQuery& /*query*/) {
  return nullptr;
}

std::unique_ptr<PlanNode> QueryPlanner::optimize_plan(std::unique_ptr<PlanNode> plan) {
  return plan;
}

std::unique_ptr<PlanNode> QueryPlanner::push_down_filter(std::unique_ptr<PlanNode> plan,
                                                         std::unique_ptr<Expression> /*filter*/) {
  return plan;
}

bool QueryPlanner::can_use_index(const Expression& /*predicate*/, const std::string& /*table_name*/,
                                 index_oid_t* /*index_oid*/) {
  return false;
}

std::shared_ptr<Schema>
QueryPlanner::infer_schema(const std::vector<std::unique_ptr<Expression>>& /*expressions*/,
                           const Schema& input_schema) {
  return std::make_shared<Schema>(input_schema);
}

double QueryPlanner::estimate_cardinality(const PlanNode& /*node*/) {
  return 1000.0;
}
double QueryPlanner::estimate_cost(const PlanNode& /*node*/) {
  return 100.0;
}

std::string TableScanPlanNode::to_string(int indent) const {
  return indent_str(indent) + "TableScan(" + table_name + ")";
}
std::string IndexScanPlanNode::to_string(int indent) const {
  return indent_str(indent) + "IndexScan(" + index_name + ")";
}
std::string ProjectionPlanNode::to_string(int indent) const {
  return indent_str(indent) + "Projection";
}
std::string FilterPlanNode::to_string(int indent) const {
  return indent_str(indent) + "Filter";
}
std::string SortPlanNode::to_string(int indent) const {
  return indent_str(indent) + "Sort";
}
std::string LimitPlanNode::to_string(int indent) const {
  return indent_str(indent) + "Limit";
}
std::string InsertPlanNode::to_string(int indent) const {
  return indent_str(indent) + "Insert into " + table_name;
}
std::string UpdatePlanNode::to_string(int indent) const {
  return indent_str(indent) + "Update";
}
std::string DeletePlanNode::to_string(int indent) const {
  return indent_str(indent) + "Delete";
}

std::string HashJoinPlanNode::to_string(int indent) const {
  std::string result =
      indent_str(indent) + "HashJoin(" + left_key_column + " = " + right_key_column + ")\n";
  if (children.size() > 0)
    result += children[0]->to_string(indent + 2) + "\n";
  if (children.size() > 1)
    result += children[1]->to_string(indent + 2);
  return result;
}

std::string NestedLoopJoinPlanNode::to_string(int indent) const {
  std::string result = indent_str(indent) + "NestedLoopJoin\n";
  if (children.size() > 0)
    result += children[0]->to_string(indent + 2) + "\n";
  if (children.size() > 1)
    result += children[1]->to_string(indent + 2);
  return result;
}

std::string AggregatePlanNode::to_string(int indent) const {
  std::string result = indent_str(indent) + "Aggregate(";
  if (!group_by_columns.empty()) {
    result += "GROUP BY ";
    for (size_t i = 0; i < group_by_columns.size(); ++i) {
      if (i > 0)
        result += ", ";
      result += group_by_columns[i];
    }
  }
  result += ")\n";
  if (!children.empty())
    result += children[0]->to_string(indent + 2);
  return result;
}

} // namespace latticedb
