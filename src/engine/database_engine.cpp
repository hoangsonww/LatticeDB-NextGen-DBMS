#include "database_engine.h"
#include "../common/exception.h"
#include "../common/result.h"
#include <map>

namespace latticedb {

DatabaseEngine::DatabaseEngine(const std::string& database_file, bool enable_logging,
                               StorageEngineType storage_type)
    : database_file_(database_file), enable_logging_(enable_logging) {
  (void)storage_type;
}

DatabaseEngine::~DatabaseEngine() {
  try {
    shutdown();
  } catch (...) {
    // Ignore exceptions during destruction
  }
}

bool DatabaseEngine::initialize() {
  // Initialize components
  disk_manager_ = std::make_unique<DiskManager>(database_file_);
  buffer_pool_manager_ = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager_.get());
  lock_manager_ = std::make_unique<LockManager>();
  log_manager_ = std::make_unique<LogManager>(DEFAULT_LOG_FILE);
  log_manager_->set_enable_logging(enable_logging_);
  catalog_ = std::make_unique<Catalog>(buffer_pool_manager_.get(), lock_manager_.get(),
                                       log_manager_.get());
  create_storage_engine(StorageEngineType::ROW_STORE);
  query_planner_ = std::make_unique<QueryPlanner>(catalog_.get());
  sql_parser_ = std::make_unique<SQLParser>();
  transaction_context_ = std::make_unique<TransactionContext>();
  return true;
}

void DatabaseEngine::shutdown() {
  // Flush all pages before shutting down
  if (buffer_pool_manager_) {
    buffer_pool_manager_->flush_all_pages();
  }

  // Clear all components in reverse order of creation
  transaction_context_.reset();
  sql_parser_.reset();
  query_planner_.reset();
  storage_engine_.reset();
  catalog_.reset();
  log_manager_.reset();
  lock_manager_.reset();
  buffer_pool_manager_.reset();
  disk_manager_.reset();
}

void DatabaseEngine::create_storage_engine(StorageEngineType storage_type) {
  switch (storage_type) {
  case StorageEngineType::ROW_STORE:
  default:
    storage_engine_ = std::make_unique<RowStoreEngine>(buffer_pool_manager_.get(), catalog_.get(),
                                                       lock_manager_.get(), log_manager_.get());
    break;
  }
}

QueryResult DatabaseEngine::execute_sql(const std::string& sql, Transaction* txn) {
  auto parsed = sql_parser_->parse(sql);
  if (parsed.type == QueryType::INVALID) {
    return QueryResult{{}, {}, parsed.error_message, false, 0};
  }
  return execute_parsed_query(parsed, txn);
}

Transaction* DatabaseEngine::begin_transaction(IsolationLevel isolation_level) {
  return transaction_context_->begin(nullptr, isolation_level);
}

bool DatabaseEngine::commit_transaction(Transaction* txn) {
  if (!txn)
    return false;
  transaction_context_->commit(txn);
  return true;
}

bool DatabaseEngine::abort_transaction(Transaction* txn) {
  if (!txn)
    return false;
  transaction_context_->abort(txn);
  return true;
}

bool DatabaseEngine::create_table(const std::string& table_name, const Schema& schema,
                                  Transaction* txn) {
  storage_engine_->create_table(table_name, schema, txn);
  return true;
}

bool DatabaseEngine::drop_table(const std::string& table_name, Transaction* txn) {
  return storage_engine_->drop_table(table_name, txn);
}

bool DatabaseEngine::create_index(const std::string& index_name, const std::string& table_name,
                                  const std::vector<std::string>& column_names, Transaction* txn) {
  (void)index_name;
  (void)table_name;
  (void)column_names;
  (void)txn;
  return false;
}

bool DatabaseEngine::drop_index(const std::string& index_name, Transaction* txn) {
  (void)index_name;
  (void)txn;
  return false;
}

bool DatabaseEngine::insert_tuple(const std::string& table_name, const Tuple& tuple,
                                  Transaction* txn) {
  auto* meta = catalog_->get_table(table_name);
  if (!meta)
    return false;
  RID rid;
  return storage_engine_->insert(meta->get_oid(), tuple, &rid, txn);
}

bool DatabaseEngine::update_tuple(const std::string& table_name, const RID& rid, const Tuple& tuple,
                                  Transaction* txn) {
  auto* meta = catalog_->get_table(table_name);
  if (!meta)
    return false;
  return storage_engine_->update(meta->get_oid(), rid, tuple, txn);
}

bool DatabaseEngine::delete_tuple(const std::string& table_name, const RID& rid, Transaction* txn) {
  auto* meta = catalog_->get_table(table_name);
  if (!meta)
    return false;
  return storage_engine_->delete_tuple(meta->get_oid(), rid, txn);
}

std::vector<Tuple> DatabaseEngine::select_tuples(const std::string& table_name, Transaction* txn) {
  std::vector<Tuple> rows;
  auto* meta = catalog_->get_table(table_name);
  if (!meta)
    return rows;
  auto it = storage_engine_->scan(meta->get_oid(), txn);
  if (!it)
    return rows;
  while (it->has_next())
    rows.push_back(it->next());
  return rows;
}

std::vector<std::string> DatabaseEngine::get_table_names() const {
  return catalog_->get_table_names();
}
TableMetadata* DatabaseEngine::get_table_info(const std::string& table_name) {
  return catalog_->get_table(table_name);
}

void DatabaseEngine::checkpoint() {
  if (log_manager_)
    log_manager_->flush();
}

QueryResult DatabaseEngine::execute_parsed_query(const ParsedQuery& query, Transaction* txn) {
  QueryResult result;
  switch (query.type) {
  case QueryType::CREATE_TABLE: {
    std::vector<Column> cols;
    for (const auto& cd : query.create_table->columns)
      cols.emplace_back(cd.name, cd.type, cd.length, cd.nullable);
    Schema schema(cols);
    if (create_table(query.create_table->table_name, schema, txn)) {
      result.success = true;
      result.message = "CREATE TABLE";
      return result;
    }
    result.success = false;
    result.message = "Failed to create table";
    return result;
  }
  case QueryType::INSERT: {
    size_t count = 0;
    for (auto row : query.insert->values) {
      // Align values to schema order if column list provided
      auto* meta = catalog_->get_table(query.insert->table_name);
      if (!meta) {
        result.success = false;
        result.message = "Table not found";
        return result;
      }
      const auto& schema = meta->get_schema();
      std::vector<Value> vals(schema.column_count());
      if (!query.insert->columns.empty()) {
        for (size_t i = 0; i < query.insert->columns.size(); ++i) {
          auto idx = schema.get_column_index(query.insert->columns[i]);
          if (i < row.size())
            vals[idx] = row[i];
        }
      } else {
        vals = std::move(row);
      }
      Tuple t(vals);
      if (!insert_tuple(query.insert->table_name, t, txn)) {
        result.success = false;
        result.message = "Insert failed";
        return result;
      }
      ++count;
    }
    current_tx_id_++;
    snapshot_table(query.insert->table_name);
    result.success = true;
    result.rows_affected = count;
    result.message = "INSERT";
    return result;
  }
  case QueryType::SELECT: {
    auto* meta = catalog_->get_table(query.select->table_name);
    if (!meta) {
      result.success = false;
      result.message = "Table not found";
      return result;
    }
    std::vector<Tuple> rows;
    if (query.select->system_time_tx &&
        table_snapshots_[query.select->table_name].count(*query.select->system_time_tx)) {
      // From snapshot
      for (const auto& t :
           table_snapshots_[query.select->table_name][*query.select->system_time_tx])
        rows.push_back(t);
    } else {
      rows = select_tuples(query.select->table_name, txn);
    }

    // Handle JOINs
    for (const auto& join : query.select->joins) {
      auto* join_meta = catalog_->get_table(join.table_name);
      if (!join_meta) {
        result.success = false;
        result.message = "Join table not found: " + join.table_name;
        return result;
      }
      auto join_rows = select_tuples(join.table_name, txn);
      std::vector<Tuple> joined;

      // Get schemas for column resolution
      const auto& left_schema = meta->get_schema();
      const auto& right_schema = join_meta->get_schema();

      // Simple nested loop join with condition evaluation
      for (const auto& left_tuple : rows) {
        for (const auto& right_tuple : join_rows) {
          bool should_join = true;

          // Evaluate join condition if present
          if (join.condition && join.condition->type == ExpressionType::OPERATOR &&
              join.condition->op_type == OperatorType::EQUAL &&
              join.condition->children.size() == 2) {

            auto& left_expr = join.condition->children[0];
            auto& right_expr = join.condition->children[1];

            // Get values from the appropriate tuple
            Value left_val, right_val;

            if (left_expr->type == ExpressionType::COLUMN_REF) {
              // Try to find column in left table
              auto left_idx = left_schema.try_get_column_index(left_expr->column_name);
              if (left_idx.has_value()) {
                left_val = left_tuple.get_values()[left_idx.value()];
              } else {
                // Try right table
                auto right_idx = right_schema.try_get_column_index(left_expr->column_name);
                if (right_idx.has_value()) {
                  left_val = right_tuple.get_values()[right_idx.value()];
                }
              }
            }

            if (right_expr->type == ExpressionType::COLUMN_REF) {
              // Try to find column in left table
              auto left_idx = left_schema.try_get_column_index(right_expr->column_name);
              if (left_idx.has_value()) {
                right_val = left_tuple.get_values()[left_idx.value()];
              } else {
                // Try right table
                auto right_idx = right_schema.try_get_column_index(right_expr->column_name);
                if (right_idx.has_value()) {
                  right_val = right_tuple.get_values()[right_idx.value()];
                }
              }
            }

            // Compare values
            should_join = (left_val == right_val);
          }

          if (should_join) {
            // Combine tuples
            std::vector<Value> combined_values;
            combined_values.insert(combined_values.end(), left_tuple.get_values().begin(),
                                   left_tuple.get_values().end());
            combined_values.insert(combined_values.end(), right_tuple.get_values().begin(),
                                   right_tuple.get_values().end());
            joined.emplace_back(combined_values);
          }
        }
      }
      rows = joined;
    }
    // WHERE filter (only col = literal supported)
    auto where = query.select->where_clause.get();
    std::vector<Tuple> filtered;
    if (where && where->type == ExpressionType::OPERATOR && where->op_type == OperatorType::EQUAL &&
        where->children.size() == 2) {
      auto colname = where->children[0]->column_name;
      auto lit = where->children[1]->value;
      try {
        auto col_idx = meta->get_schema().get_column_index(colname);
        for (const auto& t : rows) {
          if (t.get_values()[col_idx] == lit)
            filtered.push_back(t);
        }
      } catch (const CatalogException& e) {
        result.success = false;
        result.message = "Column not found in WHERE clause: " + colname;
        return result;
      }
    } else {
      filtered = rows;
    }
    // Handle GROUP BY and aggregates
    bool has_aggregates = false;
    bool has_group_by = !query.select->group_by.empty();

    for (auto& e : query.select->select_list) {
      if (e->type == ExpressionType::AGGREGATE) {
        has_aggregates = true;
        break;
      }
    }

    if (has_aggregates || has_group_by) {
      // Aggregate processing
      std::map<std::vector<Value>, std::vector<Tuple>> groups;

      if (has_group_by) {
        // Group by specified columns
        for (const auto& tuple : filtered) {
          std::vector<Value> group_key;
          for (const auto& col_name : query.select->group_by) {
            auto idx = meta->get_schema().get_column_index(col_name);
            group_key.push_back(tuple.get_values()[idx]);
          }
          groups[group_key].push_back(tuple);
        }
      } else {
        // Single group for all rows
        groups[std::vector<Value>{}] = filtered;
      }

      // Compute aggregates
      result.success = true;
      for (auto& e : query.select->select_list) {
        if (e->type == ExpressionType::AGGREGATE) {
          result.column_names.push_back("agg_result");
        } else if (e->type == ExpressionType::COLUMN_REF) {
          result.column_names.push_back(e->column_name);
        }
      }

      for (const auto& [group_key, group_tuples] : groups) {
        std::vector<Value> result_row;

        for (auto& e : query.select->select_list) {
          if (e->type == ExpressionType::AGGREGATE) {
            Value agg_result;

            switch (e->agg_type) {
            case AggregateType::COUNT:
              agg_result = Value(static_cast<int64_t>(group_tuples.size()));
              break;
            case AggregateType::SUM: {
              double sum = 0;
              if (!e->children.empty()) {
                auto col_idx = meta->get_schema().get_column_index(e->children[0]->column_name);
                for (const auto& t : group_tuples) {
                  const auto& val = t.get_values()[col_idx];
                  // Convert value to double based on its type
                  switch (val.type()) {
                  case ValueType::INTEGER:
                    sum += static_cast<double>(val.get<int32_t>());
                    break;
                  case ValueType::BIGINT:
                    sum += static_cast<double>(val.get<int64_t>());
                    break;
                  case ValueType::DOUBLE:
                    sum += val.get<double>();
                    break;
                  default:
                    break;
                  }
                }
              }
              agg_result = Value(sum);
              break;
            }
            case AggregateType::AVG: {
              double sum = 0;
              if (!e->children.empty() && !group_tuples.empty()) {
                auto col_idx = meta->get_schema().get_column_index(e->children[0]->column_name);
                for (const auto& t : group_tuples) {
                  const auto& val = t.get_values()[col_idx];
                  // Convert value to double based on its type
                  switch (val.type()) {
                  case ValueType::INTEGER:
                    sum += static_cast<double>(val.get<int32_t>());
                    break;
                  case ValueType::BIGINT:
                    sum += static_cast<double>(val.get<int64_t>());
                    break;
                  case ValueType::DOUBLE:
                    sum += val.get<double>();
                    break;
                  default:
                    break;
                  }
                }
                sum /= group_tuples.size();
              }
              agg_result = Value(sum);
              break;
            }
            case AggregateType::MIN: {
              if (!e->children.empty() && !group_tuples.empty()) {
                auto col_idx = meta->get_schema().get_column_index(e->children[0]->column_name);
                auto min_val = group_tuples[0].get_values()[col_idx];
                for (const auto& t : group_tuples) {
                  if (t.get_values()[col_idx] < min_val) {
                    min_val = t.get_values()[col_idx];
                  }
                }
                agg_result = min_val;
              }
              break;
            }
            case AggregateType::MAX: {
              if (!e->children.empty() && !group_tuples.empty()) {
                auto col_idx = meta->get_schema().get_column_index(e->children[0]->column_name);
                auto max_val = group_tuples[0].get_values()[col_idx];
                for (const auto& t : group_tuples) {
                  if (t.get_values()[col_idx] > max_val) {
                    max_val = t.get_values()[col_idx];
                  }
                }
                agg_result = max_val;
              }
              break;
            }
            }
            result_row.push_back(agg_result);
          } else if (e->type == ExpressionType::COLUMN_REF) {
            // For GROUP BY columns, take first tuple's value
            if (!group_tuples.empty()) {
              auto col_idx = meta->get_schema().get_column_index(e->column_name);
              result_row.push_back(group_tuples[0].get_values()[col_idx]);
            }
          }
        }
        result.rows.push_back(result_row);
      }
      return result;
    }

    // Normal projection (non-aggregate)
    std::vector<uint32_t> proj_idx;
    if (query.select->select_list.empty() ||
        (query.select->select_list.size() == 1 &&
         query.select->select_list[0]->type == ExpressionType::STAR)) {
      for (uint32_t i = 0; i < meta->get_schema().column_count(); ++i)
        proj_idx.push_back(i);
    } else {
      for (auto& e : query.select->select_list) {
        if (e->type == ExpressionType::COLUMN_REF) {
          try {
            proj_idx.push_back(meta->get_schema().get_column_index(e->column_name));
          } catch (const CatalogException& ex) {
            result.success = false;
            result.message = "Column not found: " + e->column_name;
            return result;
          }
        } else if (e->type == ExpressionType::STAR) {
          // Handle * in select list
          for (uint32_t i = 0; i < meta->get_schema().column_count(); ++i)
            proj_idx.push_back(i);
        }
      }
    }
    result.success = true;
    for (auto idx : proj_idx)
      result.column_names.push_back(meta->get_schema().get_column(idx).name());
    for (const auto& t : filtered) {
      std::vector<Value> out;
      const auto& vals = t.get_values();
      for (auto idx : proj_idx)
        out.push_back(vals[idx]);
      result.rows.push_back(std::move(out));
    }
    return result;
  }
  case QueryType::UPDATE: {
    auto* meta = catalog_->get_table(query.update->table_name);
    if (!meta) {
      result.success = false;
      result.message = "Table not found";
      return result;
    }
    const auto& schema = meta->get_schema();
    // scan and update
    size_t count = 0;
    auto it = storage_engine_->scan(meta->get_oid(), txn);
    while (it && it->has_next()) {
      Tuple t = it->next();
      RID rid = it->get_rid();
      bool match = true;
      auto where = query.update->where_clause.get();
      if (where && where->type == ExpressionType::OPERATOR &&
          where->op_type == OperatorType::EQUAL && where->children.size() == 2) {
        auto colname = where->children[0]->column_name;
        auto lit = where->children[1]->value;
        try {
          auto col_idx = schema.get_column_index(colname);
          match = (t.get_values()[col_idx] == lit);
        } catch (const CatalogException& e) {
          result.success = false;
          result.message = "Column not found in WHERE clause: " + colname;
          return result;
        }
      }
      if (!match)
        continue;
      auto vals = t.get_values();
      for (auto& asg : query.update->assignments) {
        try {
          auto idx = schema.get_column_index(asg.first);
          vals[idx] = asg.second;
        } catch (const CatalogException& e) {
          result.success = false;
          result.message = "Column not found: " + asg.first;
          return result;
        }
      }
      Tuple nt(vals);
      storage_engine_->update(meta->get_oid(), rid, nt, txn);
      count++;
    }
    current_tx_id_++;
    snapshot_table(query.update->table_name);
    result.success = true;
    result.rows_affected = count;
    result.message = "UPDATE";
    return result;
  }
  case QueryType::DELETE: {
    auto* meta = catalog_->get_table(query.delete_query->table_name);
    if (!meta) {
      result.success = false;
      result.message = "Table not found";
      return result;
    }
    const auto& schema = meta->get_schema();
    size_t count = 0;
    std::vector<RID> to_delete;

    // First scan to collect RIDs to delete
    auto it = storage_engine_->scan(meta->get_oid(), txn);
    while (it && it->has_next()) {
      Tuple t = it->next();
      RID rid = it->get_rid();
      bool match = true;

      auto where = query.delete_query->where_clause.get();
      if (where && where->type == ExpressionType::OPERATOR &&
          where->op_type == OperatorType::EQUAL && where->children.size() == 2) {
        auto colname = where->children[0]->column_name;
        auto lit = where->children[1]->value;
        try {
          auto col_idx = schema.get_column_index(colname);
          match = (t.get_values()[col_idx] == lit);
        } catch (const CatalogException& e) {
          result.success = false;
          result.message = "Column not found in WHERE clause: " + colname;
          return result;
        }
      }

      if (match) {
        to_delete.push_back(rid);
      }
    }

    // Now delete the collected tuples
    for (const auto& rid : to_delete) {
      if (storage_engine_->delete_tuple(meta->get_oid(), rid, txn)) {
        count++;
      }
    }

    current_tx_id_++;
    snapshot_table(query.delete_query->table_name);
    result.success = true;
    result.rows_affected = count;
    result.message = "DELETE";
    return result;
  }
  case QueryType::BEGIN: {
    // Start a new transaction
    if (!txn) {
      // In real implementation, would create a new transaction
      result.success = true;
      result.message = "BEGIN";
    } else {
      result.success = false;
      result.message = "Transaction already in progress";
    }
    return result;
  }
  case QueryType::COMMIT: {
    // Commit the current transaction
    if (txn) {
      // In real implementation, would commit the transaction
      result.success = true;
      result.message = "COMMIT";
    } else {
      result.success = false;
      result.message = "No transaction in progress";
    }
    return result;
  }
  case QueryType::ROLLBACK: {
    // Rollback the current transaction
    if (txn) {
      // In real implementation, would rollback the transaction
      result.success = true;
      result.message = "ROLLBACK";
    } else {
      result.success = false;
      result.message = "No transaction in progress";
    }
    return result;
  }
  case QueryType::DROP_TABLE: {
    if (!query.drop_table) {
      result.success = false;
      result.message = "Invalid DROP TABLE query";
      return result;
    }

    auto* meta = catalog_->get_table(query.drop_table->table_name);
    if (!meta) {
      if (query.drop_table->if_exists) {
        result.success = true;
        result.message = "DROP TABLE";
        return result;
      }
      result.success = false;
      result.message = "Table not found: " + query.drop_table->table_name;
      return result;
    }

    // Remove from catalog
    if (catalog_->drop_table(query.drop_table->table_name, txn)) {
      // Clear snapshots
      table_snapshots_.erase(query.drop_table->table_name);
      result.success = true;
      result.message = "DROP TABLE";
    } else {
      result.success = false;
      result.message = "Failed to drop table";
    }
    return result;
  }
  default:
    result.success = false;
    result.message = "Unsupported query";
    return result;
  }
}

void DatabaseEngine::cleanup_transaction(Transaction* /*txn*/) {}

bool DatabaseEngine::validate_transaction_state(Transaction* /*txn*/) {
  return true;
}

void DatabaseEngine::snapshot_table(const std::string& table_name) {
  auto* meta = catalog_->get_table(table_name);
  if (!meta)
    return;
  auto it = storage_engine_->scan(meta->get_oid(), nullptr);
  std::vector<Tuple> snap;
  while (it && it->has_next())
    snap.push_back(it->next());
  table_snapshots_[table_name][current_tx_id_] = std::move(snap);
}

} // namespace latticedb
