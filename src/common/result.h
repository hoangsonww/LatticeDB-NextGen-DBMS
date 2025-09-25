#pragma once

#include "../types/value.h"
#include <string>
#include <vector>

namespace latticedb {

struct QueryResult {
  std::vector<std::string> column_names;
  std::vector<std::vector<Value>> rows;
  std::string message;
  bool success{false};
  size_t rows_affected{0};
};

} // namespace latticedb
