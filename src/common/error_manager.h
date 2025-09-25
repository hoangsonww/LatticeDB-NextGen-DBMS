#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace latticedb {

// Error severity levels
enum class ErrorSeverity { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, FATAL = 4 };

// Error categories based on SQL standards
enum class ErrorCategory {
  SYNTAX_ERROR,         // SQL syntax errors
  SEMANTIC_ERROR,       // Semantic/logical errors
  CONSTRAINT_VIOLATION, // Constraint violations
  TRANSACTION_ERROR,    // Transaction-related errors
  CONCURRENCY_ERROR,    // Deadlock, lock timeout
  RESOURCE_ERROR,       // Out of memory, disk full
  PERMISSION_ERROR,     // Access denied
  CONNECTION_ERROR,     // Network/connection issues
  INTERNAL_ERROR,       // Internal DB errors
  DATA_ERROR,           // Data type/conversion errors
  INTEGRITY_ERROR,      // Data integrity violations
  IO_ERROR,             // I/O operation errors
  RECOVERY_ERROR,       // Recovery/backup errors
  CONFIGURATION_ERROR   // Config/setup errors
};

// SQL State codes (subset of standard SQL states)
class SQLState {
public:
  static constexpr const char* SUCCESS = "00000";
  static constexpr const char* SYNTAX_ERROR = "42000";
  static constexpr const char* TABLE_NOT_FOUND = "42P01";
  static constexpr const char* COLUMN_NOT_FOUND = "42703";
  static constexpr const char* DUPLICATE_KEY = "23505";
  static constexpr const char* FOREIGN_KEY_VIOLATION = "23503";
  static constexpr const char* NOT_NULL_VIOLATION = "23502";
  static constexpr const char* CHECK_VIOLATION = "23514";
  static constexpr const char* UNIQUE_VIOLATION = "23505";
  static constexpr const char* DEADLOCK = "40P01";
  static constexpr const char* LOCK_TIMEOUT = "55P03";
  static constexpr const char* TRANSACTION_ROLLBACK = "40000";
  static constexpr const char* INSUFFICIENT_RESOURCES = "53000";
  static constexpr const char* DISK_FULL = "53100";
  static constexpr const char* OUT_OF_MEMORY = "53200";
  static constexpr const char* ACCESS_DENIED = "42501";
  static constexpr const char* CONNECTION_FAILURE = "08006";
  static constexpr const char* DATA_TYPE_MISMATCH = "22000";
  static constexpr const char* DIVISION_BY_ZERO = "22012";
  static constexpr const char* INTERNAL_ERROR = "XX000";
};

// Extended error information
struct ErrorContext {
  std::string query;
  int line_number;
  int column_number;
  std::string table_name;
  std::string column_name;
  std::string constraint_name;
  std::unordered_map<std::string, std::string> additional_info;

  ErrorContext() : line_number(-1), column_number(-1) {}
};

// Base exception class for all database errors
class DatabaseException : public std::exception {
protected:
  ErrorSeverity severity_;
  ErrorCategory category_;
  std::string sql_state_;
  std::string message_;
  std::string detailed_message_;
  ErrorContext context_;
  std::chrono::system_clock::time_point timestamp_;

public:
  DatabaseException(ErrorSeverity severity, ErrorCategory category, const std::string& sql_state,
                    const std::string& message)
      : severity_(severity), category_(category), sql_state_(sql_state), message_(message),
        timestamp_(std::chrono::system_clock::now()) {}

  const char* what() const noexcept override {
    return message_.c_str();
  }

  ErrorSeverity severity() const {
    return severity_;
  }
  ErrorCategory category() const {
    return category_;
  }
  const std::string& sql_state() const {
    return sql_state_;
  }
  const std::string& detailed_message() const {
    return detailed_message_;
  }
  const ErrorContext& context() const {
    return context_;
  }

  void set_context(const ErrorContext& ctx) {
    context_ = ctx;
  }
  void set_detailed_message(const std::string& msg) {
    detailed_message_ = msg;
  }

  std::string to_string() const;
};

// Specific exception types
class SyntaxException : public DatabaseException {
public:
  SyntaxException(const std::string& message)
      : DatabaseException(ErrorSeverity::ERROR, ErrorCategory::SYNTAX_ERROR, SQLState::SYNTAX_ERROR,
                          message) {}
};

class ConstraintViolationException : public DatabaseException {
public:
  ConstraintViolationException(const std::string& constraint_type, const std::string& message)
      : DatabaseException(ErrorSeverity::ERROR, ErrorCategory::CONSTRAINT_VIOLATION,
                          get_sql_state(constraint_type), message) {}

private:
  static std::string get_sql_state(const std::string& type) {
    if (type == "PRIMARY_KEY")
      return SQLState::DUPLICATE_KEY;
    if (type == "FOREIGN_KEY")
      return SQLState::FOREIGN_KEY_VIOLATION;
    if (type == "UNIQUE")
      return SQLState::UNIQUE_VIOLATION;
    if (type == "NOT_NULL")
      return SQLState::NOT_NULL_VIOLATION;
    if (type == "CHECK")
      return SQLState::CHECK_VIOLATION;
    return SQLState::INTERNAL_ERROR;
  }
};

class DeadlockException : public DatabaseException {
public:
  DeadlockException(const std::string& message)
      : DatabaseException(ErrorSeverity::ERROR, ErrorCategory::CONCURRENCY_ERROR,
                          SQLState::DEADLOCK, message) {}
};

class ResourceException : public DatabaseException {
public:
  ResourceException(const std::string& resource_type, const std::string& message)
      : DatabaseException(ErrorSeverity::FATAL, ErrorCategory::RESOURCE_ERROR,
                          get_sql_state(resource_type), message) {}

private:
  static std::string get_sql_state(const std::string& type) {
    if (type == "MEMORY")
      return SQLState::OUT_OF_MEMORY;
    if (type == "DISK")
      return SQLState::DISK_FULL;
    return SQLState::INSUFFICIENT_RESOURCES;
  }
};

class PermissionException : public DatabaseException {
public:
  PermissionException(const std::string& message)
      : DatabaseException(ErrorSeverity::ERROR, ErrorCategory::PERMISSION_ERROR,
                          SQLState::ACCESS_DENIED, message) {}
};

class DataException : public DatabaseException {
public:
  DataException(const std::string& message,
                const std::string& sql_state = SQLState::DATA_TYPE_MISMATCH)
      : DatabaseException(ErrorSeverity::ERROR, ErrorCategory::DATA_ERROR, sql_state, message) {}
};

class InternalException : public DatabaseException {
public:
  InternalException(const std::string& message)
      : DatabaseException(ErrorSeverity::FATAL, ErrorCategory::INTERNAL_ERROR,
                          SQLState::INTERNAL_ERROR, message) {}
};

// Error handler for logging and recovery
class ErrorHandler {
public:
  using ErrorCallback = std::function<void(const DatabaseException&)>;

private:
  std::vector<ErrorCallback> error_callbacks_;
  std::unordered_map<ErrorCategory, int> error_counts_;
  std::vector<std::unique_ptr<DatabaseException>> error_history_;
  size_t max_history_size_;
  std::mutex mutex_;
  bool enable_stack_trace_;

public:
  ErrorHandler(size_t max_history = 1000)
      : max_history_size_(max_history), enable_stack_trace_(false) {}

  // Register error callback
  void register_callback(ErrorCallback callback);

  // Handle an error
  void handle_error(const DatabaseException& error);

  // Log error to file
  void log_error(const DatabaseException& error);

  // Get error statistics
  std::unordered_map<ErrorCategory, int> get_error_stats() const;

  // Get recent errors
  std::vector<const DatabaseException*> get_recent_errors(size_t count) const;

  // Clear error history
  void clear_history();

  // Enable/disable stack trace
  void set_stack_trace_enabled(bool enabled) {
    enable_stack_trace_ = enabled;
  }

  // Check if should retry operation
  bool should_retry(const DatabaseException& error) const;

  // Get recovery suggestion
  std::string get_recovery_suggestion(const DatabaseException& error) const;

private:
  // Capture stack trace
  std::string capture_stack_trace() const;

  // Format error for logging
  std::string format_error(const DatabaseException& error) const;
};

// Error recovery strategies
class ErrorRecovery {
public:
  // Attempt to recover from error
  static bool attempt_recovery(const DatabaseException& error);

  // Rollback transaction on error
  static void rollback_on_error(Transaction* txn);

  // Retry operation with exponential backoff
  template <typename Func> static bool retry_with_backoff(Func operation, int max_retries = 3);

  // Handle out of memory
  static bool handle_out_of_memory();

  // Handle disk full
  static bool handle_disk_full();

  // Handle deadlock
  static bool handle_deadlock(Transaction* txn);

  // Clean up after fatal error
  static void cleanup_after_fatal_error();
};

// Global error manager
class ErrorManager {
private:
  static std::unique_ptr<ErrorManager> instance_;
  ErrorHandler handler_;
  bool panic_mode_;
  std::mutex panic_mutex_;

public:
  static ErrorManager& get_instance();

  // Report error
  void report_error(const DatabaseException& error);

  // Check if in panic mode
  bool is_panic_mode() const {
    return panic_mode_;
  }

  // Enter panic mode
  void enter_panic_mode();

  // Exit panic mode
  void exit_panic_mode();

  // Get error handler
  ErrorHandler& get_handler() {
    return handler_;
  }

  // Format error for user
  static std::string format_user_error(const DatabaseException& error);

  // Format error for log
  static std::string format_log_error(const DatabaseException& error);

private:
  ErrorManager();
};

// Assertion and debugging helpers
#ifdef DEBUG
#define LATTICE_ASSERT(condition, message)                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      throw InternalException(std::string("Assertion failed: ") + message);                        \
    }                                                                                              \
  } while (0)
#else
#define LATTICE_ASSERT(condition, message) ((void)0)
#endif

// Error checking macros
#define CHECK_NOT_NULL(ptr, message)                                                               \
  do {                                                                                             \
    if ((ptr) == nullptr) {                                                                        \
      throw InternalException(std::string("Null pointer: ") + message);                            \
    }                                                                                              \
  } while (0)

#define CHECK_BOUNDS(index, size, message)                                                         \
  do {                                                                                             \
    if ((index) >= (size)) {                                                                       \
      throw DataException(std::string("Index out of bounds: ") + message);                         \
    }                                                                                              \
  } while (0)

} // namespace latticedb