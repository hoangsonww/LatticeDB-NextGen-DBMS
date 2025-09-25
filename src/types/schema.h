#pragma once

#include "value.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace latticedb {

class Column {
private:
  std::string name_;
  ValueType type_;
  uint32_t length_;
  bool nullable_;
  Value default_value_;
  bool has_default_;

public:
  Column(std::string name, ValueType type, uint32_t length = 0, bool nullable = true)
      : name_(std::move(name)), type_(type), length_(length), nullable_(nullable),
        has_default_(false) {}

  Column(std::string name, ValueType type, uint32_t length, bool nullable, const Value& default_val)
      : name_(std::move(name)), type_(type), length_(length), nullable_(nullable),
        default_value_(default_val), has_default_(true) {}

  const std::string& name() const {
    return name_;
  }
  ValueType type() const {
    return type_;
  }
  uint32_t length() const {
    return length_;
  }
  bool is_nullable() const {
    return nullable_;
  }
  bool has_default() const {
    return has_default_;
  }
  const Value& default_value() const {
    return default_value_;
  }

  uint32_t get_serialized_size() const;
  uint32_t get_variable_length(const Value& value) const;
  bool is_variable_length() const;

  std::string to_string() const;
};

class Schema {
private:
  std::vector<Column> columns_;
  std::unordered_map<std::string, uint32_t> name_to_index_;
  uint32_t tuple_size_;
  bool has_variable_length_columns_;

public:
  explicit Schema(const std::vector<Column>& columns);

  const std::vector<Column>& columns() const {
    return columns_;
  }

  uint32_t column_count() const {
    return static_cast<uint32_t>(columns_.size());
  }

  const Column& get_column(uint32_t idx) const;

  uint32_t get_column_index(const std::string& name) const;

  std::optional<uint32_t> try_get_column_index(const std::string& name) const;

  uint32_t get_tuple_size() const {
    return tuple_size_;
  }

  bool has_variable_length_columns() const {
    return has_variable_length_columns_;
  }

  uint32_t get_tuple_size(const std::vector<Value>& values) const;

  bool is_valid_tuple(const std::vector<Value>& values) const;

  std::string to_string() const;

  static std::shared_ptr<Schema> copy_schema(const Schema* from,
                                             const std::vector<uint32_t>& attrs);

private:
  void initialize();
};

} // namespace latticedb