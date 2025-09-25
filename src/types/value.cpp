#include "value.h"
#include "../common/exception.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>

namespace latticedb {

static bool is_numeric(ValueType t) {
  switch (t) {
  case ValueType::TINYINT:
  case ValueType::SMALLINT:
  case ValueType::INTEGER:
  case ValueType::BIGINT:
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    return true;
  default:
    return false;
  }
}

std::string Value::to_string() const {
  std::ostringstream ss;
  switch (type_) {
  case ValueType::NULL_TYPE:
    ss << "NULL";
    break;
  case ValueType::BOOLEAN:
    ss << (std::get<bool>(data_) ? "TRUE" : "FALSE");
    break;
  case ValueType::TINYINT:
    ss << static_cast<int>(std::get<int8_t>(data_));
    break;
  case ValueType::SMALLINT:
    ss << std::get<int16_t>(data_);
    break;
  case ValueType::INTEGER:
    ss << std::get<int32_t>(data_);
    break;
  case ValueType::BIGINT:
    ss << std::get<int64_t>(data_);
    break;
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    ss << std::get<double>(data_);
    break;
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    ss << std::get<std::string>(data_);
    break;
  case ValueType::BLOB:
    ss << "<BLOB:" << std::get<std::vector<uint8_t>>(data_).size() << ">";
    break;
  case ValueType::VECTOR:
    ss << "<VECTOR:" << std::get<std::vector<double>>(data_).size() << ">";
    break;
  }
  return ss.str();
}

size_t Value::hash() const {
  std::hash<std::string> sh;
  switch (type_) {
  case ValueType::NULL_TYPE:
    return 0;
  case ValueType::BOOLEAN:
    return std::hash<bool>()(std::get<bool>(data_));
  case ValueType::TINYINT:
    return std::hash<int8_t>()(std::get<int8_t>(data_));
  case ValueType::SMALLINT:
    return std::hash<int16_t>()(std::get<int16_t>(data_));
  case ValueType::INTEGER:
    return std::hash<int32_t>()(std::get<int32_t>(data_));
  case ValueType::BIGINT:
    return std::hash<int64_t>()(std::get<int64_t>(data_));
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    return std::hash<long long>()(
        static_cast<long long>(std::llround(std::get<double>(data_) * 1e6)));
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    return sh(std::get<std::string>(data_));
  case ValueType::BLOB: {
    const auto& v = std::get<std::vector<uint8_t>>(data_);
    size_t h = 0;
    for (auto b : v)
      h = h * 131 + b;
    return h;
  }
  case ValueType::VECTOR: {
    const auto& v = std::get<std::vector<double>>(data_);
    size_t h = 0;
    for (auto d : v)
      h ^= std::hash<long long>()(static_cast<long long>(std::llround(d * 1e6)) +
                                  0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    return h;
  }
  }
  return 0;
}

static double to_double(ValueType t, const Value::ValueData& data) {
  switch (t) {
  case ValueType::TINYINT:
    return static_cast<double>(std::get<int8_t>(data));
  case ValueType::SMALLINT:
    return static_cast<double>(std::get<int16_t>(data));
  case ValueType::INTEGER:
    return static_cast<double>(std::get<int32_t>(data));
  case ValueType::BIGINT:
    return static_cast<double>(std::get<int64_t>(data));
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    return std::get<double>(data);
  case ValueType::BOOLEAN:
    return std::get<bool>(data) ? 1.0 : 0.0;
  default:
    throw ConversionException("Cannot convert to number");
  }
}

int Value::compare(const Value& other) const {
  if (type_ == other.type_) {
    switch (type_) {
    case ValueType::NULL_TYPE:
      return 0;
    case ValueType::BOOLEAN:
      return (int)std::get<bool>(data_) - (int)other.get<bool>();
    case ValueType::TINYINT:
      return std::get<int8_t>(data_) - other.get<int8_t>();
    case ValueType::SMALLINT:
      return std::get<int16_t>(data_) - other.get<int16_t>();
    case ValueType::INTEGER:
      return (std::get<int32_t>(data_) > other.get<int32_t>()) -
             (std::get<int32_t>(data_) < other.get<int32_t>());
    case ValueType::BIGINT:
      return (std::get<int64_t>(data_) > other.get<int64_t>()) -
             (std::get<int64_t>(data_) < other.get<int64_t>());
    case ValueType::DECIMAL:
    case ValueType::REAL:
    case ValueType::DOUBLE:
      return (std::get<double>(data_) > other.get<double>()) -
             (std::get<double>(data_) < other.get<double>());
    case ValueType::VARCHAR:
    case ValueType::TEXT:
    case ValueType::TIMESTAMP:
    case ValueType::DATE:
    case ValueType::TIME:
      return std::get<std::string>(data_).compare(other.get<std::string>());
    case ValueType::BLOB: {
      const auto& a = std::get<std::vector<uint8_t>>(data_);
      const auto& b = other.get<std::vector<uint8_t>>();
      if (a == b) {
        return 0;
      }
      return a < b ? -1 : 1;
    }
    case ValueType::VECTOR: {
      const auto& a = std::get<std::vector<double>>(data_);
      const auto& b = other.get<std::vector<double>>();
      if (a == b) {
        return 0;
      }
      return a < b ? -1 : 1;
    }
    }
  }
  // Cross-type compare: support numeric and string fallback
  if (is_numeric(type_) && is_numeric(other.type_)) {
    double lhs = to_double(type_, data_);
    double rhs = to_double(other.type_, other.data_);
    return (lhs > rhs) - (lhs < rhs);
  }
  if ((type_ == ValueType::VARCHAR || type_ == ValueType::TEXT) &&
      (other.type_ == ValueType::VARCHAR || other.type_ == ValueType::TEXT)) {
    return std::get<std::string>(data_).compare(std::get<std::string>(other.data_));
  }
  throw ConversionException("Incomparable value types");
}

bool Value::operator==(const Value& other) const {
  if (type_ != other.type_) {
    if (is_numeric(type_) && is_numeric(other.type_)) {
      return std::fabs(to_double(type_, data_) - to_double(other.type_, other.data_)) < 1e-9;
    }
    if ((type_ == ValueType::VARCHAR || type_ == ValueType::TEXT) &&
        (other.type_ == ValueType::VARCHAR || other.type_ == ValueType::TEXT)) {
      return std::get<std::string>(data_) == std::get<std::string>(other.data_);
    }
    return false;
  }
  switch (type_) {
  case ValueType::NULL_TYPE:
    return true;
  case ValueType::BOOLEAN:
    return std::get<bool>(data_) == std::get<bool>(other.data_);
  case ValueType::TINYINT:
    return std::get<int8_t>(data_) == std::get<int8_t>(other.data_);
  case ValueType::SMALLINT:
    return std::get<int16_t>(data_) == std::get<int16_t>(other.data_);
  case ValueType::INTEGER:
    return std::get<int32_t>(data_) == std::get<int32_t>(other.data_);
  case ValueType::BIGINT:
    return std::get<int64_t>(data_) == std::get<int64_t>(other.data_);
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    return std::fabs(std::get<double>(data_) - std::get<double>(other.data_)) < 1e-9;
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    return std::get<std::string>(data_) == std::get<std::string>(other.data_);
  case ValueType::BLOB:
    return std::get<std::vector<uint8_t>>(data_) == std::get<std::vector<uint8_t>>(other.data_);
  case ValueType::VECTOR:
    return std::get<std::vector<double>>(data_) == std::get<std::vector<double>>(other.data_);
  }
  return false;
}

Value Value::cast_to(ValueType target_type) const {
  if (type_ == target_type)
    return *this;
  if (!is_compatible_type(target_type)) {
    throw ConversionException("Incompatible cast");
  }
  switch (target_type) {
  case ValueType::BOOLEAN:
    return Value(compare(Value(0)) != 0);
  case ValueType::TINYINT:
    return Value(static_cast<int8_t>(to_double(type_, data_)));
  case ValueType::SMALLINT:
    return Value(static_cast<int16_t>(to_double(type_, data_)));
  case ValueType::INTEGER:
    return Value(static_cast<int32_t>(to_double(type_, data_)));
  case ValueType::BIGINT:
    return Value(static_cast<int64_t>(to_double(type_, data_)));
  case ValueType::DOUBLE:
    return Value(static_cast<double>(to_double(type_, data_)));
  case ValueType::VARCHAR:
  case ValueType::TEXT:
    return Value(to_string(), ValueType::VARCHAR);
  default:
    break;
  }
  throw ConversionException("Unsupported cast target type");
}

bool Value::is_compatible_type(ValueType target_type) const {
  if (type_ == target_type)
    return true;
  if (is_numeric(type_) && is_numeric(target_type))
    return true;
  if ((type_ == ValueType::VARCHAR || type_ == ValueType::TEXT) && is_numeric(target_type))
    return true;
  if (is_numeric(type_) && (target_type == ValueType::VARCHAR || target_type == ValueType::TEXT))
    return true;
  return false;
}

size_t Value::serialize_size() const {
  switch (type_) {
  case ValueType::NULL_TYPE:
    return 1; // type only
  case ValueType::BOOLEAN:
    return 1 + 1;
  case ValueType::TINYINT:
    return 1 + 1;
  case ValueType::SMALLINT:
    return 1 + 2;
  case ValueType::INTEGER:
    return 1 + 4;
  case ValueType::BIGINT:
    return 1 + 8;
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    return 1 + 8;
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME:
    return 1 + 4 + std::get<std::string>(data_).size();
  case ValueType::BLOB:
    return 1 + 4 + std::get<std::vector<uint8_t>>(data_).size();
  case ValueType::VECTOR:
    return 1 + 4 + std::get<std::vector<double>>(data_).size() * sizeof(double);
  }
  return 0;
}

void Value::serialize(uint8_t* buffer) const {
  buffer[0] = static_cast<uint8_t>(type_);
  uint8_t* p = buffer + 1;
  switch (type_) {
  case ValueType::NULL_TYPE:
    break;
  case ValueType::BOOLEAN:
    *p = std::get<bool>(data_) ? 1 : 0;
    break;
  case ValueType::TINYINT:
    *p = static_cast<uint8_t>(std::get<int8_t>(data_));
    break;
  case ValueType::SMALLINT:
    std::memcpy(p, &std::get<int16_t>(data_), 2);
    break;
  case ValueType::INTEGER:
    std::memcpy(p, &std::get<int32_t>(data_), 4);
    break;
  case ValueType::BIGINT:
    std::memcpy(p, &std::get<int64_t>(data_), 8);
    break;
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE:
    std::memcpy(p, &std::get<double>(data_), 8);
    break;
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME: {
    const auto& s = std::get<std::string>(data_);
    uint32_t len = static_cast<uint32_t>(s.size());
    std::memcpy(p, &len, 4);
    p += 4;
    std::memcpy(p, s.data(), len);
    break;
  }
  case ValueType::BLOB: {
    const auto& v = std::get<std::vector<uint8_t>>(data_);
    uint32_t len = static_cast<uint32_t>(v.size());
    std::memcpy(p, &len, 4);
    p += 4;
    if (len)
      std::memcpy(p, v.data(), len);
    break;
  }
  case ValueType::VECTOR: {
    const auto& v = std::get<std::vector<double>>(data_);
    uint32_t len = static_cast<uint32_t>(v.size());
    std::memcpy(p, &len, 4);
    p += 4;
    if (len)
      std::memcpy(p, v.data(), len * sizeof(double));
    break;
  }
  }
}

Value Value::deserialize(const uint8_t* buffer, size_t& offset) {
  ValueType type = static_cast<ValueType>(buffer[offset]);
  ++offset;
  switch (type) {
  case ValueType::NULL_TYPE:
    return Value();
  case ValueType::BOOLEAN:
    return Value(static_cast<bool>(buffer[offset++] != 0));
  case ValueType::TINYINT:
    return Value(static_cast<int8_t>(buffer[offset++]));
  case ValueType::SMALLINT: {
    int16_t v;
    std::memcpy(&v, buffer + offset, 2);
    offset += 2;
    return Value(v);
  }
  case ValueType::INTEGER: {
    int32_t v;
    std::memcpy(&v, buffer + offset, 4);
    offset += 4;
    return Value(v);
  }
  case ValueType::BIGINT: {
    int64_t v;
    std::memcpy(&v, buffer + offset, 8);
    offset += 8;
    return Value(v);
  }
  case ValueType::DECIMAL:
  case ValueType::REAL:
  case ValueType::DOUBLE: {
    double v;
    std::memcpy(&v, buffer + offset, 8);
    offset += 8;
    return Value(v);
  }
  case ValueType::VARCHAR:
  case ValueType::TEXT:
  case ValueType::TIMESTAMP:
  case ValueType::DATE:
  case ValueType::TIME: {
    uint32_t len;
    std::memcpy(&len, buffer + offset, 4);
    offset += 4;
    std::string s(reinterpret_cast<const char*>(buffer + offset),
                  reinterpret_cast<const char*>(buffer + offset) + len);
    offset += len;
    return Value(s, ValueType::VARCHAR);
  }
  case ValueType::BLOB: {
    uint32_t len;
    std::memcpy(&len, buffer + offset, 4);
    offset += 4;
    std::vector<uint8_t> v(len);
    if (len)
      std::memcpy(v.data(), buffer + offset, len);
    offset += len;
    return Value(v);
  }
  case ValueType::VECTOR: {
    uint32_t len;
    std::memcpy(&len, buffer + offset, 4);
    offset += 4;
    std::vector<double> v(len);
    if (len)
      std::memcpy(v.data(), buffer + offset, len * sizeof(double));
    offset += len * sizeof(double);
    return Value(v);
  }
  }
  return Value();
}

} // namespace latticedb
