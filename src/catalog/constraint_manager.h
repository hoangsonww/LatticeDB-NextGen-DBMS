#pragma once

#include "../common/config.h"
#include "../types/schema.h"
#include "../types/value.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace latticedb {

enum class ConstraintType { PRIMARY_KEY, FOREIGN_KEY, UNIQUE, NOT_NULL, CHECK };

class Constraint {
public:
  virtual ~Constraint() = default;
  virtual ConstraintType get_type() const = 0;
  virtual bool validate(const Tuple& tuple, const Schema& schema) const = 0;
  virtual std::string to_string() const = 0;
};

class PrimaryKeyConstraint : public Constraint {
private:
  std::vector<uint32_t> key_columns_;
  std::unordered_set<size_t> existing_keys_; // Hash of key combinations

public:
  explicit PrimaryKeyConstraint(const std::vector<uint32_t>& columns) : key_columns_(columns) {}

  ConstraintType get_type() const override {
    return ConstraintType::PRIMARY_KEY;
  }

  bool validate(const Tuple& tuple, const Schema& schema) const override;

  bool check_duplicate(const Tuple& tuple);

  void add_key(const Tuple& tuple);

  void remove_key(const Tuple& tuple);

  std::string to_string() const override;

private:
  size_t compute_key_hash(const Tuple& tuple) const;
};

class ForeignKeyConstraint : public Constraint {
private:
  std::vector<uint32_t> foreign_columns_;
  table_oid_t referenced_table_;
  std::vector<uint32_t> referenced_columns_;
  bool cascade_delete_;
  bool cascade_update_;

public:
  ForeignKeyConstraint(const std::vector<uint32_t>& foreign_cols, table_oid_t ref_table,
                       const std::vector<uint32_t>& ref_cols, bool cascade_del = false,
                       bool cascade_upd = false)
      : foreign_columns_(foreign_cols), referenced_table_(ref_table), referenced_columns_(ref_cols),
        cascade_delete_(cascade_del), cascade_update_(cascade_upd) {}

  ConstraintType get_type() const override {
    return ConstraintType::FOREIGN_KEY;
  }

  bool validate(const Tuple& tuple, const Schema& schema) const override;

  bool check_reference_exists(const Tuple& tuple, class Catalog* catalog) const;

  bool check_can_delete(const Tuple& tuple, class Catalog* catalog) const;

  table_oid_t get_referenced_table() const {
    return referenced_table_;
  }

  bool has_cascade_delete() const {
    return cascade_delete_;
  }

  bool has_cascade_update() const {
    return cascade_update_;
  }

  std::string to_string() const override;
};

class UniqueConstraint : public Constraint {
private:
  std::vector<uint32_t> unique_columns_;
  std::unordered_set<size_t> existing_values_;

public:
  explicit UniqueConstraint(const std::vector<uint32_t>& columns) : unique_columns_(columns) {}

  ConstraintType get_type() const override {
    return ConstraintType::UNIQUE;
  }

  bool validate(const Tuple& tuple, const Schema& schema) const override;

  bool check_unique(const Tuple& tuple);

  void add_value(const Tuple& tuple);

  void remove_value(const Tuple& tuple);

  std::string to_string() const override;

private:
  size_t compute_value_hash(const Tuple& tuple) const;
};

class NotNullConstraint : public Constraint {
private:
  uint32_t column_index_;

public:
  explicit NotNullConstraint(uint32_t col_idx) : column_index_(col_idx) {}

  ConstraintType get_type() const override {
    return ConstraintType::NOT_NULL;
  }

  bool validate(const Tuple& tuple, const Schema& schema) const override;

  std::string to_string() const override;
};

class CheckConstraint : public Constraint {
private:
  std::string expression_;
  std::function<bool(const Tuple&, const Schema&)> check_func_;

public:
  CheckConstraint(const std::string& expr, std::function<bool(const Tuple&, const Schema&)> func)
      : expression_(expr), check_func_(func) {}

  ConstraintType get_type() const override {
    return ConstraintType::CHECK;
  }

  bool validate(const Tuple& tuple, const Schema& schema) const override {
    return check_func_(tuple, schema);
  }

  std::string to_string() const override;
};

class ConstraintManager {
private:
  std::unordered_map<table_oid_t, std::vector<std::unique_ptr<Constraint>>> table_constraints_;
  std::unordered_map<table_oid_t, std::unique_ptr<PrimaryKeyConstraint>> primary_keys_;
  std::unordered_map<table_oid_t, std::vector<std::unique_ptr<ForeignKeyConstraint>>> foreign_keys_;
  std::unordered_map<table_oid_t, std::vector<std::unique_ptr<UniqueConstraint>>>
      unique_constraints_;

public:
  ConstraintManager() = default;
  ~ConstraintManager() = default;

  // Add constraints
  void add_primary_key(table_oid_t table_id, const std::vector<uint32_t>& columns);

  void add_foreign_key(table_oid_t table_id, const std::vector<uint32_t>& foreign_cols,
                       table_oid_t ref_table, const std::vector<uint32_t>& ref_cols,
                       bool cascade_delete = false, bool cascade_update = false);

  void add_unique(table_oid_t table_id, const std::vector<uint32_t>& columns);

  void add_not_null(table_oid_t table_id, uint32_t column_index);

  void add_check(table_oid_t table_id, const std::string& expression,
                 std::function<bool(const Tuple&, const Schema&)> check_func);

  // Validate operations
  bool validate_insert(table_oid_t table_id, const Tuple& tuple, const Schema& schema);

  bool validate_update(table_oid_t table_id, const Tuple& old_tuple, const Tuple& new_tuple,
                       const Schema& schema);

  bool validate_delete(table_oid_t table_id, const Tuple& tuple, class Catalog* catalog);

  // Get constraints
  std::vector<Constraint*> get_constraints(table_oid_t table_id);

  PrimaryKeyConstraint* get_primary_key(table_oid_t table_id);

  std::vector<ForeignKeyConstraint*> get_foreign_keys(table_oid_t table_id);

  // Handle cascading operations
  std::vector<std::pair<table_oid_t, RID>>
  get_cascade_deletes(table_oid_t table_id, const Tuple& tuple, class Catalog* catalog);

  // Drop table constraints
  void drop_table_constraints(table_oid_t table_id);
};

} // namespace latticedb