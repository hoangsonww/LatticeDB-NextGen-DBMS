#pragma once

#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#ifdef DEBUG
#undef DEBUG
#endif

namespace latticedb {

enum class LogLevel { TRACE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4, FATAL = 5 };

class Logger {
private:
  std::ofstream log_file_;
  LogLevel level_;
  std::mutex file_mutex_;
  bool console_output_;
  bool file_output_;

  Logger();

public:
  static Logger* get_instance();

  void set_level(LogLevel level) {
    level_ = level;
  }
  void set_console_output(bool enabled) {
    console_output_ = enabled;
  }
  void set_file_output(bool enabled) {
    file_output_ = enabled;
  }
  void set_log_file(const std::string& filename);

  void log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);

  template <typename... Args> void trace(const std::string& format, Args&&... args) {
    if (level_ <= LogLevel::TRACE) {
      log(LogLevel::TRACE, format_string(format, std::forward<Args>(args)...));
    }
  }

  template <typename... Args> void debug(const std::string& format, Args&&... args) {
    if (level_ <= LogLevel::DEBUG) {
      log(LogLevel::DEBUG, format_string(format, std::forward<Args>(args)...));
    }
  }

  template <typename... Args> void info(const std::string& format, Args&&... args) {
    if (level_ <= LogLevel::INFO) {
      log(LogLevel::INFO, format_string(format, std::forward<Args>(args)...));
    }
  }

  template <typename... Args> void warn(const std::string& format, Args&&... args) {
    if (level_ <= LogLevel::WARN) {
      log(LogLevel::WARN, format_string(format, std::forward<Args>(args)...));
    }
  }

  template <typename... Args> void error(const std::string& format, Args&&... args) {
    if (level_ <= LogLevel::ERROR) {
      log(LogLevel::ERROR, format_string(format, std::forward<Args>(args)...));
    }
  }

  template <typename... Args> void fatal(const std::string& format, Args&&... args) {
    log(LogLevel::FATAL, format_string(format, std::forward<Args>(args)...));
  }

private:
  template <typename... Args> std::string format_string(const std::string& format, Args&&... args) {
    std::stringstream ss;
    format_helper(ss, format, std::forward<Args>(args)...);
    return ss.str();
  }

  template <typename T, typename... Args>
  void format_helper(std::stringstream& ss, const std::string& format, T&& arg, Args&&... args) {
    size_t pos = format.find("{}");
    if (pos != std::string::npos) {
      ss << format.substr(0, pos) << arg;
      format_helper(ss, format.substr(pos + 2), std::forward<Args>(args)...);
    } else {
      ss << format;
    }
  }

  void format_helper(std::stringstream& ss, const std::string& format) {
    ss << format;
  }

  std::string level_to_string(LogLevel level);
  std::string get_timestamp();
};

#define LOG_TRACE(...) latticedb::Logger::get_instance()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) latticedb::Logger::get_instance()->debug(__VA_ARGS__)
#define LOG_INFO(...) latticedb::Logger::get_instance()->info(__VA_ARGS__)
#define LOG_WARN(...) latticedb::Logger::get_instance()->warn(__VA_ARGS__)
#define LOG_ERROR(...) latticedb::Logger::get_instance()->error(__VA_ARGS__)
#define LOG_FATAL(...) latticedb::Logger::get_instance()->fatal(__VA_ARGS__)

} // namespace latticedb
