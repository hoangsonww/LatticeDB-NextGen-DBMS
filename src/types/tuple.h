#pragma once

#include "value.h"
#include <memory>
#include <vector>

namespace latticedb {

class Schema;

class Tuple {
private:
  std::vector<Value> values_;
  bool allocated_;
  std::unique_ptr<uint8_t[]> data_;
  size_t size_;

public:
  static constexpr uint32_t INVALID_RID = UINT32_MAX;

  Tuple() : allocated_(false), size_(0) {}

  explicit Tuple(std::vector<Value> values);

  Tuple(const uint8_t* data, size_t size);

  Tuple(const Tuple& other);
  Tuple& operator=(const Tuple& other);
  Tuple(Tuple&& other) noexcept;
  Tuple& operator=(Tuple&& other) noexcept;

  ~Tuple() = default;

  Value get_value(const Schema* schema, uint32_t column_idx) const;

  void set_value(const Schema* schema, uint32_t column_idx, const Value& value);

  const std::vector<Value>& get_values() const {
    return values_;
  }

  size_t size() const {
    return size_;
  }

  const uint8_t* data() const {
    return data_.get();
  }

  bool is_allocated() const {
    return allocated_;
  }

  void serialize_to(uint8_t* buffer) const;

  static Tuple deserialize_from(const uint8_t* data, size_t size);

  std::string to_string(const Schema* schema) const;

  bool operator==(const Tuple& other) const;
  bool operator!=(const Tuple& other) const {
    return !(*this == other);
  }

private:
  void serialize();
  void deserialize();
};

struct RID {
  uint32_t page_id;
  uint32_t slot_num;

  RID() : page_id(UINT32_MAX), slot_num(UINT32_MAX) {}
  RID(uint32_t page_id, uint32_t slot_num) : page_id(page_id), slot_num(slot_num) {}

  bool operator==(const RID& other) const {
    return page_id == other.page_id && slot_num == other.slot_num;
  }

  bool operator!=(const RID& other) const {
    return !(*this == other);
  }

  bool is_valid() const {
    return page_id != UINT32_MAX && slot_num != UINT32_MAX;
  }

  std::string to_string() const {
    return "(" + std::to_string(page_id) + ", " + std::to_string(slot_num) + ")";
  }
};

} // namespace latticedb

namespace std {
template <> struct hash<latticedb::RID> {
  size_t operator()(const latticedb::RID& rid) const {
    return hash<uint32_t>()(rid.page_id) ^ (hash<uint32_t>()(rid.slot_num) << 1);
  }
};
} // namespace std