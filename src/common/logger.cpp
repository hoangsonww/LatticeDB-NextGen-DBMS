#include <mutex>

#include "logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace latticedb {

Logger* Logger::get_instance() {
  static Logger instance;
  return &instance;
}

Logger::Logger() : level_(LogLevel::INFO), console_output_(true), file_output_(false) {}

void Logger::log(LogLevel level, const std::string& message, const char* file, int line) {
  if (level < level_)
    return;
  std::ostringstream oss;
  oss << get_timestamp() << " [" << level_to_string(level) << "] ";
  if (file)
    oss << file << ":" << line << " ";
  oss << message;
  auto line_str = oss.str();
  if (console_output_) {
    std::cout << line_str << std::endl;
  }
  if (file_output_) {
    std::lock_guard<std::mutex> lk(file_mutex_);
    if (log_file_.is_open()) {
      log_file_ << line_str << std::endl;
      log_file_.flush();
    }
  }
}

void Logger::set_log_file(const std::string& filename) {
  std::lock_guard<std::mutex> lk(file_mutex_);
  if (log_file_.is_open())
    log_file_.close();
  log_file_.open(filename, std::ios::app);
}

std::string Logger::level_to_string(LogLevel level) {
  switch (level) {
  case LogLevel::TRACE:
    return "TRACE";
  case LogLevel::DEBUG:
    return "DEBUG";
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARN:
    return "WARN";
  case LogLevel::ERROR:
    return "ERROR";
  case LogLevel::FATAL:
    return "FATAL";
  }
  return "INFO";
}

std::string Logger::get_timestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto t = system_clock::to_time_t(now);
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
      << ms.count();
  return oss.str();
}

} // namespace latticedb
