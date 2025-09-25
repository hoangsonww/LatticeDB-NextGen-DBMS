#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace latticedb {

// System performance metrics
struct PerformanceMetrics {
  uint64_t queries_executed = 0;
  uint64_t transactions_committed = 0;
  uint64_t transactions_aborted = 0;
  uint64_t pages_read = 0;
  uint64_t pages_written = 0;
  uint64_t buffer_hits = 0;
  uint64_t buffer_misses = 0;
  double avg_query_time_ms = 0.0;
  uint64_t active_connections = 0;
  uint64_t total_connections = 0;
  std::chrono::system_clock::time_point start_time;

  double get_buffer_hit_rate() const {
    uint64_t total = buffer_hits + buffer_misses;
    return total > 0 ? (double)buffer_hits / total * 100.0 : 0.0;
  }

  std::string get_uptime() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now - start_time;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count() % 60;
    return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
  }
};

// Database configuration info
struct DatabaseInfo {
  std::string version = "2.0.0";
  std::string build_date = __DATE__;
  uint32_t page_size = 4096;
  uint32_t buffer_pool_size = 0;
  uint32_t max_connections = 100;
  bool wal_enabled = false;
  bool compression_enabled = false;
  std::string storage_path;
  std::vector<std::string> enabled_features;
};

// Table statistics
struct TableStats {
  std::string table_name;
  uint64_t row_count = 0;
  uint64_t page_count = 0;
  uint64_t index_count = 0;
  uint64_t total_size_bytes = 0;
  std::chrono::system_clock::time_point last_modified;
  std::chrono::system_clock::time_point last_analyzed;
};

class SystemDiagnostics {
public:
  SystemDiagnostics();
  ~SystemDiagnostics() = default;

  // Performance monitoring
  void record_query_execution(double time_ms);
  void record_transaction_commit();
  void record_transaction_abort();
  void record_page_read();
  void record_page_write();
  void record_buffer_hit();
  void record_buffer_miss();
  void record_connection_open();
  void record_connection_close();

  // Information retrieval
  PerformanceMetrics get_performance_metrics() const;
  DatabaseInfo get_database_info() const;
  std::vector<TableStats> get_table_statistics() const;

  // Health checks
  bool check_database_health() const;
  std::vector<std::string> get_warnings() const;
  std::vector<std::string> get_recommendations() const;

  // Export diagnostics
  std::string export_json_report() const;
  std::string export_text_report() const;

  // Reset statistics
  void reset_metrics();

private:
  PerformanceMetrics metrics_;
  DatabaseInfo db_info_;
  std::unordered_map<std::string, TableStats> table_stats_;
  mutable std::mutex mutex_;

  void update_averages();
};

// Global diagnostics instance
extern std::unique_ptr<SystemDiagnostics> g_diagnostics;

} // namespace latticedb
