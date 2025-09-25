#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace latticedb {

enum class ValueType {
  NULL_TYPE,
  BOOLEAN,
  TINYINT,
  SMALLINT,
  INTEGER,
  BIGINT,
  DECIMAL,
  REAL,
  DOUBLE,
  VARCHAR,
  TEXT,
  TIMESTAMP,
  DATE,
  TIME,
  BLOB,
  VECTOR
};

class Value {
public:
  using ValueData = std::variant<std::monostate,       // NULL
                                 bool,                 // BOOLEAN
                                 int8_t,               // TINYINT
                                 int16_t,              // SMALLINT
                                 int32_t,              // INTEGER
                                 int64_t,              // BIGINT
                                 double,               // DECIMAL, REAL, DOUBLE
                                 std::string,          // VARCHAR, TEXT, TIMESTAMP, DATE, TIME
                                 std::vector<uint8_t>, // BLOB
                                 std::vector<double>   // VECTOR
                                 >;

private:
  ValueData data_;
  ValueType type_;

public:
  Value() : data_(std::monostate{}), type_(ValueType::NULL_TYPE) {}

  explicit Value(bool val) : data_(val), type_(ValueType::BOOLEAN) {}
  explicit Value(int8_t val) : data_(val), type_(ValueType::TINYINT) {}
  explicit Value(int16_t val) : data_(val), type_(ValueType::SMALLINT) {}
  explicit Value(int32_t val) : data_(val), type_(ValueType::INTEGER) {}
  explicit Value(int64_t val) : data_(val), type_(ValueType::BIGINT) {}
  explicit Value(double val) : data_(val), type_(ValueType::DOUBLE) {}
  explicit Value(const std::string& val, ValueType type = ValueType::VARCHAR)
      : data_(val), type_(type) {}
  explicit Value(const char* val) : data_(std::string(val)), type_(ValueType::VARCHAR) {}
  explicit Value(const std::vector<uint8_t>& val) : data_(val), type_(ValueType::BLOB) {}
  explicit Value(const std::vector<double>& val) : data_(val), type_(ValueType::VECTOR) {}

  ValueType type() const {
    return type_;
  }
  bool is_null() const {
    return type_ == ValueType::NULL_TYPE;
  }

  template <typename T> T get() const {
    return std::get<T>(data_);
  }

  template <typename T> std::optional<T> try_get() const {
    if (auto ptr = std::get_if<T>(&data_)) {
      return *ptr;
    }
    return std::nullopt;
  }

  std::string to_string() const;
  size_t hash() const;
  int compare(const Value& other) const;
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const {
    return !(*this == other);
  }
  bool operator<(const Value& other) const {
    return compare(other) < 0;
  }
  bool operator<=(const Value& other) const {
    return compare(other) <= 0;
  }
  bool operator>(const Value& other) const {
    return compare(other) > 0;
  }
  bool operator>=(const Value& other) const {
    return compare(other) >= 0;
  }

  Value cast_to(ValueType target_type) const;
  bool is_compatible_type(ValueType target_type) const;

  size_t serialize_size() const;
  void serialize(uint8_t* buffer) const;
  static Value deserialize(const uint8_t* buffer, size_t& offset);
};

} // namespace latticedb