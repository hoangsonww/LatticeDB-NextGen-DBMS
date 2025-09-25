#include "query_executor.h"
#include "../catalog/table_heap.h"
#include <algorithm>
#include <cstring>

namespace latticedb {

// TableScanExecutor implementation
TableScanExecutor::TableScanExecutor(ExecutionContext* context, const TableScanPlanNode* plan)
    : Executor(context), plan_(plan), table_iterator_(nullptr, RID()) {}

void TableScanExecutor::init() {
  // Reset for new scan
  table_heap_.reset();
  // Iterator will be initialized in next()
}

bool TableScanExecutor::next(Tuple* tuple, RID* rid) {
  auto* table_meta = context_->catalog->get_table(plan_->table_oid);
  if (!table_meta)
    return false;

  // Use member variables instead of static to avoid leaks
  if (!table_heap_) {
    table_heap_ = std::make_unique<TableHeap>(context_->buffer_pool_manager, context_->lock_manager,
                                              context_->log_manager, table_meta->get_oid());
    table_iterator_ = table_heap_->begin(context_->transaction);
  }

  while (table_iterator_ != table_heap_->end()) {
    *tuple = *table_iterator_;
    *rid = table_iterator_.get_rid();
    ++table_iterator_;
    if (evaluate_predicate(*tuple)) {
      return true;
    }
  }
  return false;
}

const Schema& TableScanExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

bool TableScanExecutor::evaluate_predicate(const Tuple& tuple) {
  if (!plan_->predicate)
    return true;

  // Evaluate the predicate expression against the tuple
  return evaluate_expression(*plan_->predicate, tuple);
}

bool TableScanExecutor::evaluate_expression(const Expression& expr, const Tuple& tuple) {
  switch (expr.type) {
  case ExpressionType::OPERATOR: {
    switch (expr.op_type) {
    case OperatorType::EQUAL:
      if (expr.children.size() == 2) {
        Value left_val = evaluate_value(*expr.children[0], tuple);
        Value right_val = evaluate_value(*expr.children[1], tuple);
        return left_val == right_val;
      }
      break;
    case OperatorType::NOT_EQUAL:
      if (expr.children.size() == 2) {
        Value left_val = evaluate_value(*expr.children[0], tuple);
        Value right_val = evaluate_value(*expr.children[1], tuple);
        return !(left_val == right_val);
      }
      break;
    case OperatorType::LESS_THAN:
      if (expr.children.size() == 2) {
        Value left_val = evaluate_value(*expr.children[0], tuple);
        Value right_val = evaluate_value(*expr.children[1], tuple);
        return left_val < right_val;
      }
      break;
    case OperatorType::LESS_THAN_EQUAL:
      if (expr.children.size() == 2) {
        Value left_val = evaluate_value(*expr.children[0], tuple);
        Value right_val = evaluate_value(*expr.children[1], tuple);
        return left_val < right_val || left_val == right_val;
      }
      break;
    case OperatorType::GREATER_THAN:
      if (expr.children.size() == 2) {
        Value left_val = evaluate_value(*expr.children[0], tuple);
        Value right_val = evaluate_value(*expr.children[1], tuple);
        return right_val < left_val;
      }
      break;
    case OperatorType::GREATER_THAN_EQUAL:
      if (expr.children.size() == 2) {
        Value left_val = evaluate_value(*expr.children[0], tuple);
        Value right_val = evaluate_value(*expr.children[1], tuple);
        return right_val < left_val || left_val == right_val;
      }
      break;
    case OperatorType::AND:
      if (expr.children.size() == 2) {
        return evaluate_expression(*expr.children[0], tuple) &&
               evaluate_expression(*expr.children[1], tuple);
      }
      break;
    case OperatorType::OR:
      if (expr.children.size() == 2) {
        return evaluate_expression(*expr.children[0], tuple) ||
               evaluate_expression(*expr.children[1], tuple);
      }
      break;
    case OperatorType::NOT:
      if (expr.children.size() == 1) {
        return !evaluate_expression(*expr.children[0], tuple);
      }
      break;
    default:
      break;
    }
    break;
  }
  case ExpressionType::CONSTANT: {
    if (expr.value.type() == ValueType::BOOLEAN) {
      return expr.value.get<bool>();
    }
    return true;
  }
  default:
    break;
  }
  return true;
}

Value TableScanExecutor::evaluate_value(const Expression& expr, const Tuple& tuple) {
  switch (expr.type) {
  case ExpressionType::COLUMN_REF: {
    // Get the table schema directly from catalog
    auto* table_meta = context_->catalog->get_table(plan_->table_oid);
    if (!table_meta) {
      return Value();
    }
    auto& schema = table_meta->get_schema();
    // Find column index by name
    try {
      auto col_idx = schema.get_column_index(expr.column_name);
      const auto& values = tuple.get_values();
      if (col_idx < values.size()) {
        return values[col_idx];
      }
    } catch (...) {
      // Column not found
      return Value();
    }
    return Value();
  }
  case ExpressionType::CONSTANT:
    return expr.value;
  default:
    return Value();
  }
}

// IndexScanExecutor implementation
IndexScanExecutor::IndexScanExecutor(ExecutionContext* context, const IndexScanPlanNode* plan)
    : Executor(context), plan_(plan) {}

void IndexScanExecutor::init() {
  // Initialize index scan
}

bool IndexScanExecutor::next(Tuple* tuple, RID* rid) {
  // Index scan implementation
  return false;
}

const Schema& IndexScanExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

// ProjectionExecutor implementation
ProjectionExecutor::ProjectionExecutor(ExecutionContext* context, const ProjectionPlanNode* plan,
                                       std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)) {}

void ProjectionExecutor::init() {
  child_executor_->init();
}

bool ProjectionExecutor::next(Tuple* tuple, RID* rid) {
  Tuple child_tuple;
  RID child_rid;

  if (!child_executor_->next(&child_tuple, &child_rid)) {
    return false;
  }

  std::vector<Value> values;
  for (const auto& expr : plan_->expressions) {
    values.push_back(evaluate_expression(*expr, child_tuple));
  }

  *tuple = Tuple(values);
  *rid = child_rid;
  return true;
}

const Schema& ProjectionExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

Value ProjectionExecutor::evaluate_expression(const Expression& expr, const Tuple& input_tuple) {
  switch (expr.type) {
  case ExpressionType::COLUMN_REF: {
    auto& child_schema = child_executor_->get_output_schema();
    auto col_idx = child_schema.get_column_index(expr.column_name);
    return input_tuple.get_values()[col_idx];
  }
  case ExpressionType::CONSTANT:
    return expr.value;
  default:
    return Value();
  }
}

// FilterExecutor implementation
FilterExecutor::FilterExecutor(ExecutionContext* context, const FilterPlanNode* plan,
                               std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)) {}

void FilterExecutor::init() {
  child_executor_->init();
}

bool FilterExecutor::next(Tuple* tuple, RID* rid) {
  while (child_executor_->next(tuple, rid)) {
    if (evaluate_predicate(*plan_->predicate, *tuple)) {
      return true;
    }
  }
  return false;
}

const Schema& FilterExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

bool FilterExecutor::evaluate_predicate(const Expression& expr, const Tuple& tuple) {
  switch (expr.type) {
  case ExpressionType::OPERATOR: {
    switch (expr.op_type) {
    case OperatorType::EQUAL:
      if (expr.children.size() == 2) {
        auto left_val = evaluate_expression(*expr.children[0], tuple);
        auto right_val = evaluate_expression(*expr.children[1], tuple);
        return left_val == right_val;
      }
      break;
    case OperatorType::NOT_EQUAL:
      if (expr.children.size() == 2) {
        auto left_val = evaluate_expression(*expr.children[0], tuple);
        auto right_val = evaluate_expression(*expr.children[1], tuple);
        return !(left_val == right_val);
      }
      break;
    case OperatorType::LESS_THAN:
      if (expr.children.size() == 2) {
        auto left_val = evaluate_expression(*expr.children[0], tuple);
        auto right_val = evaluate_expression(*expr.children[1], tuple);
        return left_val < right_val;
      }
      break;
    case OperatorType::LESS_THAN_EQUAL:
      if (expr.children.size() == 2) {
        auto left_val = evaluate_expression(*expr.children[0], tuple);
        auto right_val = evaluate_expression(*expr.children[1], tuple);
        return left_val < right_val || left_val == right_val;
      }
      break;
    case OperatorType::GREATER_THAN:
      if (expr.children.size() == 2) {
        auto left_val = evaluate_expression(*expr.children[0], tuple);
        auto right_val = evaluate_expression(*expr.children[1], tuple);
        return right_val < left_val;
      }
      break;
    case OperatorType::GREATER_THAN_EQUAL:
      if (expr.children.size() == 2) {
        auto left_val = evaluate_expression(*expr.children[0], tuple);
        auto right_val = evaluate_expression(*expr.children[1], tuple);
        return right_val < left_val || left_val == right_val;
      }
      break;
    case OperatorType::AND:
      if (expr.children.size() == 2) {
        return evaluate_predicate(*expr.children[0], tuple) &&
               evaluate_predicate(*expr.children[1], tuple);
      }
      break;
    case OperatorType::OR:
      if (expr.children.size() == 2) {
        return evaluate_predicate(*expr.children[0], tuple) ||
               evaluate_predicate(*expr.children[1], tuple);
      }
      break;
    case OperatorType::NOT:
      if (expr.children.size() == 1) {
        return !evaluate_predicate(*expr.children[0], tuple);
      }
      break;
    default:
      break;
    }
    break;
  }
  case ExpressionType::CONSTANT: {
    if (expr.value.type() == ValueType::BOOLEAN) {
      return expr.value.get<bool>();
    }
    return true;
  }
  default:
    break;
  }
  return true;
}

Value FilterExecutor::evaluate_expression(const Expression& expr, const Tuple& tuple) {
  switch (expr.type) {
  case ExpressionType::COLUMN_REF: {
    auto& schema = child_executor_->get_output_schema();
    auto col_idx = schema.get_column_index(expr.column_name);
    return tuple.get_values()[col_idx];
  }
  case ExpressionType::CONSTANT:
    return expr.value;
  default:
    return Value();
  }
}

// SortExecutor implementation
SortExecutor::SortExecutor(ExecutionContext* context, const SortPlanNode* plan,
                           std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)), current_index_(0) {}

void SortExecutor::init() {
  child_executor_->init();
  sorted_tuples_.clear();
  current_index_ = 0;

  // Collect all tuples
  Tuple tuple;
  RID rid;
  while (child_executor_->next(&tuple, &rid)) {
    sorted_tuples_.push_back(tuple);
  }

  // Sort tuples
  std::sort(sorted_tuples_.begin(), sorted_tuples_.end(),
            [this](const Tuple& a, const Tuple& b) { return compare_tuples(a, b); });
}

bool SortExecutor::next(Tuple* tuple, RID* rid) {
  if (current_index_ >= sorted_tuples_.size()) {
    return false;
  }

  *tuple = sorted_tuples_[current_index_++];
  *rid = RID(); // Invalid RID for sorted results
  return true;
}

const Schema& SortExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

bool SortExecutor::compare_tuples(const Tuple& left, const Tuple& right) {
  for (const auto& order_by : plan_->order_bys) {
    auto& schema = *plan_->output_schema;
    auto col_idx = schema.get_column_index(order_by.column_name);
    const auto& left_val = left.get_values()[col_idx];
    const auto& right_val = right.get_values()[col_idx];

    if (left_val < right_val) {
      return order_by.is_ascending;
    } else if (right_val < left_val) {
      return !order_by.is_ascending;
    }
  }
  return false;
}

// LimitExecutor implementation
LimitExecutor::LimitExecutor(ExecutionContext* context, const LimitPlanNode* plan,
                             std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)), count_(0) {}

void LimitExecutor::init() {
  child_executor_->init();
  count_ = 0;
}

bool LimitExecutor::next(Tuple* tuple, RID* rid) {
  if (count_ >= plan_->limit) {
    return false;
  }

  if (child_executor_->next(tuple, rid)) {
    count_++;
    return true;
  }
  return false;
}

const Schema& LimitExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

// InsertExecutor implementation
InsertExecutor::InsertExecutor(ExecutionContext* context, const InsertPlanNode* plan)
    : Executor(context), plan_(plan), current_index_(0), executed_(false) {}

void InsertExecutor::init() {
  current_index_ = 0;
  executed_ = false;
}

bool InsertExecutor::next(Tuple* tuple, RID* rid) {
  if (executed_)
    return false;

  auto* table_meta = context_->catalog->get_table(plan_->table_oid);
  if (!table_meta)
    return false;

  auto table_heap =
      std::make_unique<TableHeap>(context_->buffer_pool_manager, context_->lock_manager,
                                  context_->log_manager, table_meta->get_oid());

  int rows_affected = 0;
  for (const auto& values : plan_->values) {
    Tuple insert_tuple(values);
    RID insert_rid;
    if (table_heap->insert_tuple(insert_tuple, &insert_rid, context_->transaction)) {
      rows_affected++;
    }
  }

  // Return number of rows inserted
  std::vector<Value> result_values;
  result_values.push_back(Value(rows_affected));
  *tuple = Tuple(result_values);
  *rid = RID();

  executed_ = true;
  return true;
}

const Schema& InsertExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

// UpdateExecutor implementation
UpdateExecutor::UpdateExecutor(ExecutionContext* context, const UpdatePlanNode* plan,
                               std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)), executed_(false) {}

void UpdateExecutor::init() {
  child_executor_->init();
  executed_ = false;
}

bool UpdateExecutor::next(Tuple* tuple, RID* rid) {
  if (executed_)
    return false;

  auto* table_meta = context_->catalog->get_table(plan_->table_oid);
  if (!table_meta)
    return false;

  auto table_heap =
      std::make_unique<TableHeap>(context_->buffer_pool_manager, context_->lock_manager,
                                  context_->log_manager, table_meta->get_oid());

  int rows_affected = 0;
  Tuple scan_tuple;
  RID scan_rid;

  while (child_executor_->next(&scan_tuple, &scan_rid)) {
    // Apply updates
    auto values = scan_tuple.get_values();
    auto& schema = *table_meta->get_schema_ptr();
    for (const auto& [col_name, new_value] : plan_->assignments) {
      auto col_idx = schema.get_column_index(col_name);
      values[col_idx] = new_value;
    }

    Tuple updated_tuple(values);
    if (table_heap->update_tuple(updated_tuple, scan_rid, context_->transaction)) {
      rows_affected++;
    }
  }

  // Return number of rows updated
  std::vector<Value> result_values;
  result_values.push_back(Value(rows_affected));
  *tuple = Tuple(result_values);
  *rid = RID();

  executed_ = true;
  return true;
}

const Schema& UpdateExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

// DeleteExecutor implementation
DeleteExecutor::DeleteExecutor(ExecutionContext* context, const DeletePlanNode* plan,
                               std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)), executed_(false) {}

void DeleteExecutor::init() {
  child_executor_->init();
  executed_ = false;
}

bool DeleteExecutor::next(Tuple* tuple, RID* rid) {
  if (executed_)
    return false;

  auto* table_meta = context_->catalog->get_table(plan_->table_oid);
  if (!table_meta)
    return false;

  auto table_heap =
      std::make_unique<TableHeap>(context_->buffer_pool_manager, context_->lock_manager,
                                  context_->log_manager, table_meta->get_oid());

  int rows_affected = 0;
  Tuple scan_tuple;
  RID scan_rid;

  while (child_executor_->next(&scan_tuple, &scan_rid)) {
    if (table_heap->mark_delete(scan_rid, context_->transaction)) {
      rows_affected++;
    }
  }

  // Return number of rows deleted
  std::vector<Value> result_values;
  result_values.push_back(Value(rows_affected));
  *tuple = Tuple(result_values);
  *rid = RID();

  executed_ = true;
  return true;
}

const Schema& DeleteExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

// HashJoinExecutor implementation
HashJoinExecutor::HashJoinExecutor(ExecutionContext* context, const HashJoinPlanNode* plan,
                                   std::unique_ptr<Executor> left, std::unique_ptr<Executor> right)
    : Executor(context), plan_(plan), left_executor_(std::move(left)),
      right_executor_(std::move(right)), buffer_index_(0) {}

void HashJoinExecutor::init() {
  left_executor_->init();
  right_executor_->init();
  hash_table_.clear();
  output_buffer_.clear();
  buffer_index_ = 0;

  // Build phase - hash the left relation
  build_hash_table();
}

bool HashJoinExecutor::next(Tuple* tuple, RID* rid) {
  // If we have buffered output, return it
  if (buffer_index_ < output_buffer_.size()) {
    *tuple = output_buffer_[buffer_index_++];
    *rid = RID();
    return true;
  }

  // Probe phase - scan right relation and probe hash table
  Tuple right_tuple;
  RID right_rid;

  while (right_executor_->next(&right_tuple, &right_rid)) {
    // Get join key from right tuple
    Value join_key = right_tuple.get_values()[plan_->right_join_key_idx];

    // Look up in hash table
    auto range = hash_table_.equal_range(join_key);
    for (auto it = range.first; it != range.second; ++it) {
      // Check join condition if specified
      if (plan_->join_predicate) {
        // TODO: Evaluate join predicate
      }

      // Combine tuples and add to buffer
      output_buffer_.push_back(combine_tuples(it->second, right_tuple));
    }

    // If we found matches, return the first one
    if (!output_buffer_.empty()) {
      *tuple = output_buffer_[0];
      buffer_index_ = 1;
      *rid = RID();
      return true;
    }
  }

  return false;
}

const Schema& HashJoinExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

void HashJoinExecutor::build_hash_table() {
  Tuple left_tuple;
  RID left_rid;

  while (left_executor_->next(&left_tuple, &left_rid)) {
    // Extract join key from left tuple
    Value join_key = left_tuple.get_values()[plan_->left_join_key_idx];

    // Insert into hash table
    hash_table_.emplace(join_key, left_tuple);
  }
}

Tuple HashJoinExecutor::combine_tuples(const Tuple& left, const Tuple& right) {
  std::vector<Value> combined_values;

  // Add all values from left tuple
  for (const auto& val : left.get_values()) {
    combined_values.push_back(val);
  }

  // Add all values from right tuple
  for (const auto& val : right.get_values()) {
    combined_values.push_back(val);
  }

  return Tuple(combined_values);
}

// NestedLoopJoinExecutor implementation
NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutionContext* context,
                                               const NestedLoopJoinPlanNode* plan,
                                               std::unique_ptr<Executor> left,
                                               std::unique_ptr<Executor> right)
    : Executor(context), plan_(plan), left_executor_(std::move(left)),
      right_executor_(std::move(right)), has_left_tuple_(false) {}

void NestedLoopJoinExecutor::init() {
  left_executor_->init();
  right_executor_->init();
  has_left_tuple_ = false;
}

bool NestedLoopJoinExecutor::next(Tuple* tuple, RID* rid) {
  RID left_rid, right_rid;

  while (true) {
    // Get a left tuple if we don't have one
    if (!has_left_tuple_) {
      if (!left_executor_->next(&left_tuple_, &left_rid)) {
        // No more left tuples
        return false;
      }
      has_left_tuple_ = true;
      // Reset right executor for new left tuple
      right_executor_->init();
    }

    // Try to get a matching right tuple
    Tuple right_tuple;
    if (right_executor_->next(&right_tuple, &right_rid)) {
      // Evaluate join predicate
      if (evaluate_join_predicate(left_tuple_, right_tuple)) {
        // Found a match - combine and return
        *tuple = combine_tuples(left_tuple_, right_tuple);
        *rid = RID();
        return true;
      }
    } else {
      // No more right tuples for current left tuple
      has_left_tuple_ = false;
    }
  }
}

const Schema& NestedLoopJoinExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

bool NestedLoopJoinExecutor::evaluate_join_predicate(const Tuple& left, const Tuple& right) {
  if (!plan_->join_predicate) {
    // Cross join - all pairs match
    return true;
  }

  // For now, assume simple equality on join keys
  // This would need a full expression evaluator in production
  if (plan_->join_type == JoinType::INNER) {
    // Check if join keys are equal
    Value left_key = left.get_values()[plan_->left_join_key_idx];
    Value right_key = right.get_values()[plan_->right_join_key_idx];
    return left_key == right_key;
  }

  // TODO: Handle other join types (LEFT, RIGHT, FULL)
  return true;
}

Tuple NestedLoopJoinExecutor::combine_tuples(const Tuple& left, const Tuple& right) {
  std::vector<Value> combined_values;

  // Add all values from left tuple
  for (const auto& val : left.get_values()) {
    combined_values.push_back(val);
  }

  // Add all values from right tuple
  for (const auto& val : right.get_values()) {
    combined_values.push_back(val);
  }

  return Tuple(combined_values);
}

// AggregateExecutor implementation
AggregateExecutor::AggregateExecutor(ExecutionContext* context, const AggregatePlanNode* plan,
                                     std::unique_ptr<Executor> child)
    : Executor(context), plan_(plan), child_executor_(std::move(child)), executed_(false) {}

void AggregateExecutor::init() {
  child_executor_->init();
  groups_.clear();
  executed_ = false;

  // Collect all tuples and group them
  Tuple tuple;
  RID rid;

  while (child_executor_->next(&tuple, &rid)) {
    auto group_key = get_group_key(tuple);
    update_aggregates(group_key, tuple);
  }

  current_group_ = groups_.begin();
}

bool AggregateExecutor::next(Tuple* tuple, RID* rid) {
  if (current_group_ == groups_.end()) {
    return false;
  }

  *tuple = make_output_tuple(current_group_->first, current_group_->second);
  *rid = RID();

  ++current_group_;
  return true;
}

const Schema& AggregateExecutor::get_output_schema() const {
  return *plan_->output_schema;
}

std::vector<Value> AggregateExecutor::get_group_key(const Tuple& tuple) {
  std::vector<Value> key;

  for (const auto& col_idx : plan_->group_by_cols) {
    key.push_back(tuple.get_values()[col_idx]);
  }

  // Empty key for non-grouped aggregation
  if (key.empty()) {
    key.push_back(Value()); // Placeholder for single group
  }

  return key;
}

void AggregateExecutor::update_aggregates(const std::vector<Value>& key, const Tuple& tuple) {
  if (groups_.find(key) == groups_.end()) {
    // Initialize aggregates for new group
    std::vector<Value> initial_aggs;

    for (size_t i = 0; i < plan_->aggregate_exprs.size(); ++i) {
      switch (plan_->aggregate_types[i]) {
      case AggregationType::COUNT:
        initial_aggs.push_back(Value(0));
        break;
      case AggregationType::SUM:
      case AggregationType::AVG:
        initial_aggs.push_back(Value(0.0));
        break;
      case AggregationType::MIN:
      case AggregationType::MAX:
        initial_aggs.push_back(tuple.get_values()[plan_->aggregate_cols[i]]);
        break;
      }
    }

    groups_[key] = initial_aggs;
  }

  // Update aggregates
  auto& aggs = groups_[key];

  for (size_t i = 0; i < plan_->aggregate_exprs.size(); ++i) {
    Value col_val = tuple.get_values()[plan_->aggregate_cols[i]];

    switch (plan_->aggregate_types[i]) {
    case AggregationType::COUNT:
      aggs[i] = Value(aggs[i].get<int>() + 1);
      break;

    case AggregationType::SUM:
    case AggregationType::AVG:
      if (col_val.type() == ValueType::INTEGER) {
        aggs[i] = Value(aggs[i].get<double>() + col_val.get<int>());
      } else if (col_val.type() == ValueType::DOUBLE) {
        aggs[i] = Value(aggs[i].get<double>() + col_val.get<double>());
      }
      break;

    case AggregationType::MIN:
      if (col_val < aggs[i]) {
        aggs[i] = col_val;
      }
      break;

    case AggregationType::MAX:
      if (aggs[i] < col_val) {
        aggs[i] = col_val;
      }
      break;
    }
  }
}

Tuple AggregateExecutor::make_output_tuple(const std::vector<Value>& key,
                                           const std::vector<Value>& aggregates) {
  std::vector<Value> output_values;

  // Add group-by columns (excluding placeholder)
  if (!(key.size() == 1 && key[0].is_null())) {
    for (const auto& val : key) {
      output_values.push_back(val);
    }
  }

  // Add aggregate values
  for (size_t i = 0; i < aggregates.size(); ++i) {
    if (plan_->aggregate_types[i] == AggregationType::AVG) {
      // For AVG, divide sum by count
      // This is simplified - would need proper count tracking
      output_values.push_back(aggregates[i]);
    } else {
      output_values.push_back(aggregates[i]);
    }
  }

  return Tuple(output_values);
}

// QueryExecutor implementation
std::unique_ptr<Executor> QueryExecutor::create_executor(const PlanNode* plan) {
  switch (plan->type) {
  case PlanNodeType::TABLE_SCAN:
    return create_table_scan_executor(dynamic_cast<const TableScanPlanNode*>(plan));
  case PlanNodeType::INDEX_SCAN:
    return create_index_scan_executor(dynamic_cast<const IndexScanPlanNode*>(plan));
  case PlanNodeType::PROJECTION:
    return create_projection_executor(dynamic_cast<const ProjectionPlanNode*>(plan));
  case PlanNodeType::FILTER:
    return create_filter_executor(dynamic_cast<const FilterPlanNode*>(plan));
  case PlanNodeType::SORT:
    return create_sort_executor(dynamic_cast<const SortPlanNode*>(plan));
  case PlanNodeType::LIMIT:
    return create_limit_executor(dynamic_cast<const LimitPlanNode*>(plan));
  case PlanNodeType::INSERT:
    return create_insert_executor(dynamic_cast<const InsertPlanNode*>(plan));
  case PlanNodeType::UPDATE:
    return create_update_executor(dynamic_cast<const UpdatePlanNode*>(plan));
  case PlanNodeType::DELETE:
    return create_delete_executor(dynamic_cast<const DeletePlanNode*>(plan));
  case PlanNodeType::HASH_JOIN:
    return create_hash_join_executor(dynamic_cast<const HashJoinPlanNode*>(plan));
  case PlanNodeType::NESTED_LOOP_JOIN:
    return create_nested_loop_join_executor(dynamic_cast<const NestedLoopJoinPlanNode*>(plan));
  case PlanNodeType::AGGREGATE:
    return create_aggregate_executor(dynamic_cast<const AggregatePlanNode*>(plan));
  default:
    return nullptr;
  }
}

std::vector<Tuple> QueryExecutor::execute_plan(const PlanNode* plan) {
  auto executor = create_executor(plan);
  if (!executor)
    return {};

  executor->init();
  std::vector<Tuple> results;
  Tuple tuple;
  RID rid;

  while (executor->next(&tuple, &rid)) {
    results.push_back(tuple);
  }

  return results;
}

QueryResult QueryExecutor::execute_query(const PlanNode* plan) {
  QueryResult result;

  auto executor = create_executor(plan);
  if (!executor) {
    result.success = false;
    result.message = "Failed to create executor";
    return result;
  }

  executor->init();
  Tuple tuple;
  RID rid;

  result.column_names = get_column_names(executor->get_output_schema());

  while (executor->next(&tuple, &rid)) {
    result.rows.push_back(tuple.get_values());
  }

  result.success = true;
  result.rows_affected = result.rows.size();
  result.message = "OK";
  return result;
}

std::unique_ptr<Executor> QueryExecutor::create_table_scan_executor(const TableScanPlanNode* plan) {
  return std::make_unique<TableScanExecutor>(nullptr, plan);
}

std::unique_ptr<Executor> QueryExecutor::create_index_scan_executor(const IndexScanPlanNode* plan) {
  return std::make_unique<IndexScanExecutor>(nullptr, plan);
}

std::unique_ptr<Executor>
QueryExecutor::create_projection_executor(const ProjectionPlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<ProjectionExecutor>(nullptr, plan, std::move(child));
}

std::unique_ptr<Executor> QueryExecutor::create_filter_executor(const FilterPlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<FilterExecutor>(nullptr, plan, std::move(child));
}

std::unique_ptr<Executor> QueryExecutor::create_sort_executor(const SortPlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<SortExecutor>(nullptr, plan, std::move(child));
}

std::unique_ptr<Executor> QueryExecutor::create_limit_executor(const LimitPlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<LimitExecutor>(nullptr, plan, std::move(child));
}

std::unique_ptr<Executor> QueryExecutor::create_insert_executor(const InsertPlanNode* plan) {
  return std::make_unique<InsertExecutor>(nullptr, plan);
}

std::unique_ptr<Executor> QueryExecutor::create_update_executor(const UpdatePlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<UpdateExecutor>(nullptr, plan, std::move(child));
}

std::unique_ptr<Executor> QueryExecutor::create_delete_executor(const DeletePlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<DeleteExecutor>(nullptr, plan, std::move(child));
}

std::unique_ptr<Executor> QueryExecutor::create_hash_join_executor(const HashJoinPlanNode* plan) {
  if (plan->children.size() != 2)
    return nullptr;
  auto left_exec = create_executor(plan->children[0].get());
  auto right_exec = create_executor(plan->children[1].get());
  return std::make_unique<HashJoinExecutor>(nullptr, plan, std::move(left_exec),
                                            std::move(right_exec));
}

std::unique_ptr<Executor>
QueryExecutor::create_nested_loop_join_executor(const NestedLoopJoinPlanNode* plan) {
  if (plan->children.size() != 2)
    return nullptr;
  auto left_exec = create_executor(plan->children[0].get());
  auto right_exec = create_executor(plan->children[1].get());
  return std::make_unique<NestedLoopJoinExecutor>(nullptr, plan, std::move(left_exec),
                                                  std::move(right_exec));
}

std::unique_ptr<Executor> QueryExecutor::create_aggregate_executor(const AggregatePlanNode* plan) {
  auto child = plan->children.empty() ? nullptr : create_executor(plan->children[0].get());
  return std::make_unique<AggregateExecutor>(nullptr, plan, std::move(child));
}

std::vector<std::string> QueryExecutor::get_column_names(const Schema& schema) {
  std::vector<std::string> names;
  for (uint32_t i = 0; i < schema.column_count(); ++i)
    names.push_back(schema.get_column(i).name());
  return names;
}

} // namespace latticedb
