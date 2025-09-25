#include "tuple.h"
#include "schema.h"
#include <cstring>

namespace latticedb {

Tuple::Tuple(std::vector<Value> values) : values_(std::move(values)), allocated_(false), size_(0) {
  serialize();
}

Tuple::Tuple(const uint8_t* data, size_t size)
    : allocated_(true), data_(new uint8_t[size]), size_(size) {
  std::memcpy(data_.get(), data, size);
  deserialize();
}

Tuple::Tuple(const Tuple& other)
    : values_(other.values_), allocated_(other.allocated_), size_(other.size_) {
  if (other.data_) {
    data_.reset(new uint8_t[other.size_]);
    std::memcpy(data_.get(), other.data_.get(), other.size_);
  }
}

Tuple& Tuple::operator=(const Tuple& other) {
  if (this == &other)
    return *this;
  values_ = other.values_;
  allocated_ = other.allocated_;
  size_ = other.size_;
  if (other.data_) {
    data_.reset(new uint8_t[other.size_]);
    std::memcpy(data_.get(), other.data_.get(), other.size_);
  } else {
    data_.reset();
  }
  return *this;
}

Tuple::Tuple(Tuple&& other) noexcept
    : values_(std::move(other.values_)), allocated_(other.allocated_),
      data_(std::move(other.data_)), size_(other.size_) {
  other.allocated_ = false;
  other.size_ = 0;
}

Tuple& Tuple::operator=(Tuple&& other) noexcept {
  if (this != &other) {
    values_ = std::move(other.values_);
    allocated_ = other.allocated_;
    data_ = std::move(other.data_);
    size_ = other.size_;
    other.allocated_ = false;
    other.size_ = 0;
  }
  return *this;
}

Value Tuple::get_value(const Schema* schema, uint32_t column_idx) const {
  (void)schema;
  return values_.at(column_idx);
}

void Tuple::set_value(const Schema* schema, uint32_t column_idx, const Value& value) {
  (void)schema;
  if (column_idx >= values_.size())
    values_.resize(column_idx + 1);
  values_[column_idx] = value;
  serialize();
}

void Tuple::serialize() {
  // Format: [u32 count][each value serialized]
  size_t sz = sizeof(uint32_t);
  for (const auto& v : values_)
    sz += v.serialize_size();
  data_.reset(new uint8_t[sz]);
  allocated_ = true;
  size_ = sz;
  uint32_t count = static_cast<uint32_t>(values_.size());
  std::memcpy(data_.get(), &count, sizeof(uint32_t));
  size_t off = sizeof(uint32_t);
  for (const auto& v : values_) {
    size_t vs = v.serialize_size();
    v.serialize(data_.get() + off);
    off += vs;
  }
}

void Tuple::deserialize() {
  // Read [u32 count][each value]
  if (!data_)
    return;
  uint32_t count = 0;
  std::memcpy(&count, data_.get(), sizeof(uint32_t));
  values_.clear();
  values_.reserve(count);
  size_t off = sizeof(uint32_t);
  for (uint32_t i = 0; i < count; ++i) {
    Value v = Value::deserialize(data_.get(), off);
    values_.push_back(std::move(v));
  }
}

void Tuple::serialize_to(uint8_t* buffer) const {
  std::memcpy(buffer, data_.get(), size_);
}

Tuple Tuple::deserialize_from(const uint8_t* data, size_t size) {
  return Tuple(data, size);
}

std::string Tuple::to_string(const Schema* schema) const {
  (void)schema;
  std::string s;
  for (size_t i = 0; i < values_.size(); ++i) {
    if (i)
      s += ", ";
    s += values_[i].to_string();
  }
  return s;
}

bool Tuple::operator==(const Tuple& other) const {
  if (values_.size() != other.values_.size())
    return false;
  for (size_t i = 0; i < values_.size(); ++i)
    if (!(values_[i] == other.values_[i]))
      return false;
  return true;
}

} // namespace latticedb
