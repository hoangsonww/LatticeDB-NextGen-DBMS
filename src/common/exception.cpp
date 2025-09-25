// Implementation for exception.h
#include "exception.h"
namespace latticedb {
Exception::Exception(const std::string& message)
    : type_(ExceptionType::INVALID), message_(message) {}
Exception::Exception(ExceptionType type, const std::string& message)
    : type_(type), message_(message) {}
} // namespace latticedb