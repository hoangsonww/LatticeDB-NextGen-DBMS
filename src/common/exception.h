#pragma once

#include <memory>
#include <stdexcept>
#include <string>

namespace latticedb {

enum class ExceptionType {
  INVALID,
  OUT_OF_RANGE,
  CONVERSION,
  UNKNOWN_TYPE,
  DECIMAL,
  MISMATCH_TYPE,
  DIVIDE_BY_ZERO,
  OBJECT_SIZE,
  INVALID_TYPE,
  SERIALIZATION,
  TRANSACTION,
  NOT_IMPLEMENTED,
  EXPRESSION,
  CATALOG,
  PARSER,
  PLANNER,
  EXECUTOR,
  CONSTRAINT,
  INDEX,
  STAT,
  CONNECTION,
  SYNTAX,
  SETTINGS,
  OPTIMIZER,
  NULL_POINTER,
  IO,
  INTERRUPT,
  FATAL,
  INTERNAL,
  INVALID_INPUT,
  OUT_OF_MEMORY,
  PERMISSION,
  PARAMETER,
  HTTP,
  MISSING_EXTENSION,
  AUTOLOAD,
  SEQUENCE,
  DEPENDENCY,
  BINDING,
  CONVERSION_EXCEPTION
};

class Exception : public std::exception {
public:
  explicit Exception(const std::string& message);
  Exception(ExceptionType type, const std::string& message);

  const char* what() const noexcept override {
    return message_.c_str();
  }
  ExceptionType type() const {
    return type_;
  }

protected:
  ExceptionType type_;
  std::string message_;
};

class TransactionException : public Exception {
public:
  explicit TransactionException(const std::string& message)
      : Exception(ExceptionType::TRANSACTION, message) {}
};

class CatalogException : public Exception {
public:
  explicit CatalogException(const std::string& message)
      : Exception(ExceptionType::CATALOG, message) {}
};

class ExecutionException : public Exception {
public:
  explicit ExecutionException(const std::string& message)
      : Exception(ExceptionType::EXECUTOR, message) {}
};

class ConversionException : public Exception {
public:
  explicit ConversionException(const std::string& message)
      : Exception(ExceptionType::CONVERSION, message) {}
};

class ConstraintException : public Exception {
public:
  explicit ConstraintException(const std::string& message)
      : Exception(ExceptionType::CONSTRAINT, message) {}
};

class IndexException : public Exception {
public:
  explicit IndexException(const std::string& message) : Exception(ExceptionType::INDEX, message) {}
};

class IOException : public Exception {
public:
  explicit IOException(const std::string& message) : Exception(ExceptionType::IO, message) {}
};

class OutOfMemoryException : public Exception {
public:
  explicit OutOfMemoryException(const std::string& message)
      : Exception(ExceptionType::OUT_OF_MEMORY, message) {}
};

} // namespace latticedb