#include "schema.h"
#include "../common/exception.h"
#include <sstream>

namespace latticedb {

static bool is_variable_type(ValueType t) {
  switch (t) {
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::BLOB:
  case ValueType::VECTOR:
    return true;
  default:
    return false;
  }
}

uint32_t Column::get_serialized_size() const {
  switch (type_) {
  case ValueType::BOOLEAN:
    return 1;
  case ValueType::TINYINT:
    return 1;
  case ValueType::SMALLINT:
    return 2;
  case ValueType::INTEGER:
    return 4;
  case ValueType::BIGINT:
    return 8;
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    return 8;
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    return 8;
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::BLOB:
  case ValueType::VECTOR:
    return 0; // variable
  case ValueType::NULL_TYPE:
    return 0;
  }
  return 0;
}

uint32_t Column::get_variable_length(const Value& value) const {
  switch (type_) {
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    return static_cast<uint32_t>(value.try_get<std::string>()->size());
  case ValueType::BLOB:
    return static_cast<uint32_t>(value.try_get<std::vector<uint8_t>>()->size());
  case ValueType::VECTOR:
    return static_cast<uint32_t>(value.try_get<std::vector<double>>()->size() * sizeof(double));
  default:
    return 0;
  }
}

bool Column::is_variable_length() const {
  return is_variable_type(type_);
}

std::string Column::to_string() const {
  std::stringstream ss;
  ss << name_ << " ";
  switch (type_) {
  case ValueType::BOOLEAN:
    ss << "BOOLEAN";
    break;
  case ValueType::TINYINT:
    ss << "TINYINT";
    break;
  case ValueType::SMALLINT:
    ss << "SMALLINT";
    break;
  case ValueType::INTEGER:
    ss << "INTEGER";
    break;
  case ValueType::BIGINT:
    ss << "BIGINT";
    break;
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    ss << "DOUBLE";
    break;
  case ValueType::VARCHAR:
    ss << "VARCHAR(" << length_ << ")";
    break;
  case ValueType::TEXT:
    ss << "TEXT";
    break;
  case ValueType::TIMESTAMP:
    ss << "TIMESTAMP";
    break;
  case ValueType::DATE:
    ss << "DATE";
    break;
  case ValueType::TIME:
    ss << "TIME";
    break;
  case ValueType::BLOB:
    ss << "BLOB";
    break;
  case ValueType::VECTOR:
    ss << "VECTOR";
    break;
  case ValueType::NULL_TYPE:
    ss << "NULL";
    break;
  }
  if (!nullable_)
    ss << " NOT NULL";
  return ss.str();
}

Schema::Schema(const std::vector<Column>& columns)
    : columns_(columns), tuple_size_(0), has_variable_length_columns_(false) {
  initialize();
}

void Schema::initialize() {
  name_to_index_.clear();
  tuple_size_ = 0;
  has_variable_length_columns_ = false;
  for (uint32_t i = 0; i < columns_.size(); ++i) {
    name_to_index_[columns_[i].name()] = i;
    auto sz = columns_[i].get_serialized_size();
    if (sz == 0 && columns_[i].is_variable_length()) {
      has_variable_length_columns_ = true;
    } else {
      tuple_size_ += sz;
    }
  }
}

const Column& Schema::get_column(uint32_t idx) const {
  if (idx >= columns_.size())
    throw CatalogException("Column index out of range");
  return columns_[idx];
}

uint32_t Schema::get_column_index(const std::string& name) const {
  auto it = name_to_index_.find(name);
  if (it == name_to_index_.end())
    throw CatalogException("Column not found: " + name);
  return it->second;
}

std::optional<uint32_t> Schema::try_get_column_index(const std::string& name) const {
  auto it = name_to_index_.find(name);
  if (it == name_to_index_.end())
    return std::nullopt;
  return it->second;
}

uint32_t Schema::get_tuple_size(const std::vector<Value>& values) const {
  if (!has_variable_length_columns_)
    return tuple_size_;
  if (values.size() != columns_.size())
    throw CatalogException("Tuple size mismatch");
  uint32_t sz = 0;
  for (size_t i = 0; i < columns_.size(); ++i) {
    auto fixed = columns_[i].get_serialized_size();
    if (fixed > 0)
      sz += fixed;
    else
      sz += 4 + columns_[i].get_variable_length(values[i]);
  }
  return sz;
}

bool Schema::is_valid_tuple(const std::vector<Value>& values) const {
  if (values.size() != columns_.size())
    return false;
  for (size_t i = 0; i < columns_.size(); ++i) {
    if (values[i].is_null() && !columns_[i].is_nullable())
      return false;
    // Basic compatibility check
    if (!values[i].is_null() && values[i].type() != columns_[i].type()) {
      // Allow numeric coercion into numeric types and string into VARCHAR/TEXT
      if (!((values[i].type() == ValueType::VARCHAR || values[i].type() == ValueType::TEXT) &&
            (columns_[i].type() == ValueType::VARCHAR || columns_[i].type() == ValueType::TEXT)) &&
          !((values[i].type() != ValueType::VARCHAR && values[i].type() != ValueType::TEXT &&
             values[i].type() != ValueType::BLOB && values[i].type() != ValueType::VECTOR) &&
            (columns_[i].type() != ValueType::VARCHAR && columns_[i].type() != ValueType::TEXT &&
             columns_[i].type() != ValueType::BLOB && columns_[i].type() != ValueType::VECTOR))) {
        return false;
      }
    }
  }
  return true;
}

std::string Schema::to_string() const {
  std::stringstream ss;
  ss << "Schema[";
  for (size_t i = 0; i < columns_.size(); ++i) {
    if (i)
      ss << ", ";
    ss << columns_[i].to_string();
  }
  ss << "]";
  return ss.str();
}

std::shared_ptr<Schema> Schema::copy_schema(const Schema* from,
                                            const std::vector<uint32_t>& attrs) {
  std::vector<Column> cols;
  cols.reserve(attrs.size());
  for (auto idx : attrs)
    cols.push_back(from->get_column(idx));
  return std::make_shared<Schema>(cols);
}

} // namespace latticedb
