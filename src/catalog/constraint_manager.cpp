#include "constraint_manager.h"
#include "catalog_manager.h"
#include <functional>
#include <sstream>

namespace latticedb {

// PrimaryKeyConstraint implementation
bool PrimaryKeyConstraint::validate(const Tuple& tuple, const Schema& schema) const {
  // Check for NULL values in primary key columns
  auto values = tuple.get_values();
  for (uint32_t col_idx : key_columns_) {
    if (col_idx >= values.size() || values[col_idx].is_null()) {
      return false; // Primary key cannot be NULL
    }
  }
  return true;
}

bool PrimaryKeyConstraint::check_duplicate(const Tuple& tuple) {
  size_t hash = compute_key_hash(tuple);
  return existing_keys_.find(hash) != existing_keys_.end();
}

void PrimaryKeyConstraint::add_key(const Tuple& tuple) {
  size_t hash = compute_key_hash(tuple);
  existing_keys_.insert(hash);
}

void PrimaryKeyConstraint::remove_key(const Tuple& tuple) {
  size_t hash = compute_key_hash(tuple);
  existing_keys_.erase(hash);
}

size_t PrimaryKeyConstraint::compute_key_hash(const Tuple& tuple) const {
  auto values = tuple.get_values();
  size_t hash = 0;
  for (uint32_t col_idx : key_columns_) {
    if (col_idx < values.size()) {
      hash ^= values[col_idx].hash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
  }
  return hash;
}

std::string PrimaryKeyConstraint::to_string() const {
  std::stringstream ss;
  ss << "PRIMARY KEY(";
  for (size_t i = 0; i < key_columns_.size(); ++i) {
    if (i > 0)
      ss << ", ";
    ss << "col" << key_columns_[i];
  }
  ss << ")";
  return ss.str();
}

// ForeignKeyConstraint implementation
bool ForeignKeyConstraint::validate(const Tuple& tuple, const Schema& schema) const {
  // Check that foreign key values are not NULL (unless allowed)
  auto values = tuple.get_values();
  for (uint32_t col_idx : foreign_columns_) {
    if (col_idx >= values.size()) {
      return false;
    }
  }
  return true;
}

bool ForeignKeyConstraint::check_reference_exists(const Tuple& tuple, Catalog* catalog) const {
  // Check if the referenced values exist in the parent table
  // This would need to query the referenced table
  // For now, simplified implementation
  return true;
}

bool ForeignKeyConstraint::check_can_delete(const Tuple& tuple, Catalog* catalog) const {
  // Check if deleting this tuple would violate referential integrity
  // Would need to check child tables
  return true;
}

std::string ForeignKeyConstraint::to_string() const {
  std::stringstream ss;
  ss << "FOREIGN KEY(";
  for (size_t i = 0; i < foreign_columns_.size(); ++i) {
    if (i > 0)
      ss << ", ";
    ss << "col" << foreign_columns_[i];
  }
  ss << ") REFERENCES table_" << referenced_table_ << "(";
  for (size_t i = 0; i < referenced_columns_.size(); ++i) {
    if (i > 0)
      ss << ", ";
    ss << "col" << referenced_columns_[i];
  }
  ss << ")";
  if (cascade_delete_)
    ss << " ON DELETE CASCADE";
  if (cascade_update_)
    ss << " ON UPDATE CASCADE";
  return ss.str();
}

// UniqueConstraint implementation
bool UniqueConstraint::validate(const Tuple& tuple, const Schema& schema) const {
  // NULL values are allowed in UNIQUE constraints
  return true;
}

bool UniqueConstraint::check_unique(const Tuple& tuple) {
  // Check if all values are NULL (allowed)
  auto values = tuple.get_values();
  bool all_null = true;
  for (uint32_t col_idx : unique_columns_) {
    if (col_idx < values.size() && !values[col_idx].is_null()) {
      all_null = false;
      break;
    }
  }
  if (all_null)
    return true;

  size_t hash = compute_value_hash(tuple);
  return existing_values_.find(hash) == existing_values_.end();
}

void UniqueConstraint::add_value(const Tuple& tuple) {
  auto values = tuple.get_values();
  bool all_null = true;
  for (uint32_t col_idx : unique_columns_) {
    if (col_idx < values.size() && !values[col_idx].is_null()) {
      all_null = false;
      break;
    }
  }
  if (!all_null) {
    size_t hash = compute_value_hash(tuple);
    existing_values_.insert(hash);
  }
}

void UniqueConstraint::remove_value(const Tuple& tuple) {
  size_t hash = compute_value_hash(tuple);
  existing_values_.erase(hash);
}

size_t UniqueConstraint::compute_value_hash(const Tuple& tuple) const {
  auto values = tuple.get_values();
  size_t hash = 0;
  for (uint32_t col_idx : unique_columns_) {
    if (col_idx < values.size()) {
      hash ^= values[col_idx].hash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
  }
  return hash;
}

std::string UniqueConstraint::to_string() const {
  std::stringstream ss;
  ss << "UNIQUE(";
  for (size_t i = 0; i < unique_columns_.size(); ++i) {
    if (i > 0)
      ss << ", ";
    ss << "col" << unique_columns_[i];
  }
  ss << ")";
  return ss.str();
}

// NotNullConstraint implementation
bool NotNullConstraint::validate(const Tuple& tuple, const Schema& schema) const {
  auto values = tuple.get_values();
  if (column_index_ >= values.size()) {
    return false;
  }
  return !values[column_index_].is_null();
}

std::string NotNullConstraint::to_string() const {
  return "NOT NULL(col" + std::to_string(column_index_) + ")";
}

// CheckConstraint implementation
std::string CheckConstraint::to_string() const {
  return "CHECK(" + expression_ + ")";
}

// ConstraintManager implementation
void ConstraintManager::add_primary_key(table_oid_t table_id,
                                        const std::vector<uint32_t>& columns) {
  auto pk = std::make_unique<PrimaryKeyConstraint>(columns);
  primary_keys_[table_id] = std::move(pk);

  // Also add as a general constraint
  table_constraints_[table_id].push_back(std::make_unique<PrimaryKeyConstraint>(columns));
}

void ConstraintManager::add_foreign_key(table_oid_t table_id,
                                        const std::vector<uint32_t>& foreign_cols,
                                        table_oid_t ref_table,
                                        const std::vector<uint32_t>& ref_cols, bool cascade_delete,
                                        bool cascade_update) {
  auto fk = std::make_unique<ForeignKeyConstraint>(foreign_cols, ref_table, ref_cols,
                                                   cascade_delete, cascade_update);

  foreign_keys_[table_id].push_back(std::move(fk));

  // Also add as a general constraint
  table_constraints_[table_id].push_back(std::make_unique<ForeignKeyConstraint>(
      foreign_cols, ref_table, ref_cols, cascade_delete, cascade_update));
}

void ConstraintManager::add_unique(table_oid_t table_id, const std::vector<uint32_t>& columns) {
  auto unique = std::make_unique<UniqueConstraint>(columns);
  unique_constraints_[table_id].push_back(std::move(unique));

  // Also add as a general constraint
  table_constraints_[table_id].push_back(std::make_unique<UniqueConstraint>(columns));
}

void ConstraintManager::add_not_null(table_oid_t table_id, uint32_t column_index) {
  table_constraints_[table_id].push_back(std::make_unique<NotNullConstraint>(column_index));
}

void ConstraintManager::add_check(table_oid_t table_id, const std::string& expression,
                                  std::function<bool(const Tuple&, const Schema&)> check_func) {
  table_constraints_[table_id].push_back(std::make_unique<CheckConstraint>(expression, check_func));
}

bool ConstraintManager::validate_insert(table_oid_t table_id, const Tuple& tuple,
                                        const Schema& schema) {
  // Check all constraints
  auto it = table_constraints_.find(table_id);
  if (it != table_constraints_.end()) {
    for (const auto& constraint : it->second) {
      if (!constraint->validate(tuple, schema)) {
        return false;
      }
    }
  }

  // Check primary key uniqueness
  if (auto pk = get_primary_key(table_id)) {
    if (pk->check_duplicate(tuple)) {
      return false; // Duplicate primary key
    }
  }

  // Check unique constraints
  auto unique_it = unique_constraints_.find(table_id);
  if (unique_it != unique_constraints_.end()) {
    for (const auto& unique : unique_it->second) {
      if (!unique->check_unique(tuple)) {
        return false; // Duplicate unique value
      }
    }
  }

  return true;
}

bool ConstraintManager::validate_update(table_oid_t table_id, const Tuple& old_tuple,
                                        const Tuple& new_tuple, const Schema& schema) {
  // Remove old values from unique/PK tracking
  if (auto pk = get_primary_key(table_id)) {
    pk->remove_key(old_tuple);
  }

  auto unique_it = unique_constraints_.find(table_id);
  if (unique_it != unique_constraints_.end()) {
    for (const auto& unique : unique_it->second) {
      unique->remove_value(old_tuple);
    }
  }

  // Validate new tuple
  bool valid = validate_insert(table_id, new_tuple, schema);

  // If validation fails, restore old values
  if (!valid) {
    if (auto pk = get_primary_key(table_id)) {
      pk->add_key(old_tuple);
    }
    if (unique_it != unique_constraints_.end()) {
      for (const auto& unique : unique_it->second) {
        unique->add_value(old_tuple);
      }
    }
  }

  return valid;
}

bool ConstraintManager::validate_delete(table_oid_t table_id, const Tuple& tuple,
                                        Catalog* catalog) {
  // Check if there are foreign keys referencing this table
  for (const auto& [other_table, constraints] : foreign_keys_) {
    for (const auto& fk : constraints) {
      if (fk->get_referenced_table() == table_id) {
        if (!fk->check_can_delete(tuple, catalog)) {
          return false; // Would violate referential integrity
        }
      }
    }
  }

  return true;
}

std::vector<Constraint*> ConstraintManager::get_constraints(table_oid_t table_id) {
  std::vector<Constraint*> result;
  auto it = table_constraints_.find(table_id);
  if (it != table_constraints_.end()) {
    for (const auto& constraint : it->second) {
      result.push_back(constraint.get());
    }
  }
  return result;
}

PrimaryKeyConstraint* ConstraintManager::get_primary_key(table_oid_t table_id) {
  auto it = primary_keys_.find(table_id);
  if (it != primary_keys_.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::vector<ForeignKeyConstraint*> ConstraintManager::get_foreign_keys(table_oid_t table_id) {
  std::vector<ForeignKeyConstraint*> result;
  auto it = foreign_keys_.find(table_id);
  if (it != foreign_keys_.end()) {
    for (const auto& fk : it->second) {
      result.push_back(fk.get());
    }
  }
  return result;
}

std::vector<std::pair<table_oid_t, RID>>
ConstraintManager::get_cascade_deletes(table_oid_t table_id, const Tuple& tuple, Catalog* catalog) {
  std::vector<std::pair<table_oid_t, RID>> cascades;

  // Find all foreign keys that reference this table with CASCADE DELETE
  for (const auto& [other_table, constraints] : foreign_keys_) {
    for (const auto& fk : constraints) {
      if (fk->get_referenced_table() == table_id && fk->has_cascade_delete()) {
        // Would need to find matching rows in other_table
        // This is simplified
      }
    }
  }

  return cascades;
}

void ConstraintManager::drop_table_constraints(table_oid_t table_id) {
  table_constraints_.erase(table_id);
  primary_keys_.erase(table_id);
  foreign_keys_.erase(table_id);
  unique_constraints_.erase(table_id);
}

} // namespace latticedb