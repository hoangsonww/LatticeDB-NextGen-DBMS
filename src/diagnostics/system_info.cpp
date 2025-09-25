#include "system_info.h"
#include <iomanip>
#include <mutex>
#include <sstream>

namespace latticedb {

std::unique_ptr<SystemDiagnostics> g_diagnostics = nullptr;

SystemDiagnostics::SystemDiagnostics() {
  metrics_.start_time = std::chrono::system_clock::now();

  // Set default database info
  db_info_.enabled_features = {"ACID Transactions",  "Write-Ahead Logging", "B+ Tree Indexes",
                               "Vector Search",      "Time Travel Queries", "Compression",
                               "Row-Level Security", "Query Optimization",  "Stream Processing"};
}

void SystemDiagnostics::record_query_execution(double time_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.queries_executed++;
  update_averages();
  metrics_.avg_query_time_ms =
      (metrics_.avg_query_time_ms * (metrics_.queries_executed - 1) + time_ms) /
      metrics_.queries_executed;
}

void SystemDiagnostics::record_transaction_commit() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.transactions_committed++;
}

void SystemDiagnostics::record_transaction_abort() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.transactions_aborted++;
}

void SystemDiagnostics::record_page_read() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.pages_read++;
}

void SystemDiagnostics::record_page_write() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.pages_written++;
}

void SystemDiagnostics::record_buffer_hit() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.buffer_hits++;
}

void SystemDiagnostics::record_buffer_miss() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.buffer_misses++;
}

void SystemDiagnostics::record_connection_open() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_.active_connections++;
  metrics_.total_connections++;
}

void SystemDiagnostics::record_connection_close() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (metrics_.active_connections > 0) {
    metrics_.active_connections--;
  }
}

PerformanceMetrics SystemDiagnostics::get_performance_metrics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return metrics_;
}

DatabaseInfo SystemDiagnostics::get_database_info() const {
  return db_info_;
}

std::vector<TableStats> SystemDiagnostics::get_table_statistics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TableStats> stats;
  for (const auto& [_, table_stat] : table_stats_) {
    stats.push_back(table_stat);
  }
  return stats;
}

bool SystemDiagnostics::check_database_health() const {
  std::lock_guard<std::mutex> lock(mutex_);

  // Basic health checks
  bool healthy = true;

  // Check buffer hit rate
  if (metrics_.get_buffer_hit_rate() < 80.0 &&
      metrics_.buffer_hits + metrics_.buffer_misses > 1000) {
    healthy = false;
  }

  // Check transaction abort rate
  uint64_t total_txn = metrics_.transactions_committed + metrics_.transactions_aborted;
  if (total_txn > 0) {
    double abort_rate = (double)metrics_.transactions_aborted / total_txn * 100.0;
    if (abort_rate > 10.0) {
      healthy = false;
    }
  }

  // Check average query time
  if (metrics_.avg_query_time_ms > 1000.0) {
    healthy = false;
  }

  return healthy;
}

std::vector<std::string> SystemDiagnostics::get_warnings() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> warnings;

  // Low buffer hit rate warning
  double hit_rate = metrics_.get_buffer_hit_rate();
  if (hit_rate < 80.0 && metrics_.buffer_hits + metrics_.buffer_misses > 1000) {
    warnings.push_back("Low buffer hit rate: " + std::to_string(hit_rate) + "%");
  }

  // High transaction abort rate
  uint64_t total_txn = metrics_.transactions_committed + metrics_.transactions_aborted;
  if (total_txn > 0) {
    double abort_rate = (double)metrics_.transactions_aborted / total_txn * 100.0;
    if (abort_rate > 10.0) {
      warnings.push_back("High transaction abort rate: " + std::to_string(abort_rate) + "%");
    }
  }

  // Slow queries
  if (metrics_.avg_query_time_ms > 500.0) {
    warnings.push_back("Slow average query time: " + std::to_string(metrics_.avg_query_time_ms) +
                       "ms");
  }

  return warnings;
}

std::vector<std::string> SystemDiagnostics::get_recommendations() const {
  std::vector<std::string> recommendations;

  double hit_rate = metrics_.get_buffer_hit_rate();
  if (hit_rate < 80.0) {
    recommendations.push_back("Consider increasing buffer pool size to improve cache hit rate");
  }

  if (metrics_.avg_query_time_ms > 500.0) {
    recommendations.push_back("Consider adding indexes on frequently queried columns");
    recommendations.push_back("Analyze slow queries and optimize them");
  }

  uint64_t total_txn = metrics_.transactions_committed + metrics_.transactions_aborted;
  if (total_txn > 0) {
    double abort_rate = (double)metrics_.transactions_aborted / total_txn * 100.0;
    if (abort_rate > 10.0) {
      recommendations.push_back(
          "High transaction conflict rate - consider adjusting isolation levels");
    }
  }

  return recommendations;
}

std::string SystemDiagnostics::export_json_report() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::stringstream ss;

  ss << "{\n";
  ss << "  \"database_info\": {\n";
  ss << "    \"version\": \"" << db_info_.version << "\",\n";
  ss << "    \"build_date\": \"" << db_info_.build_date << "\",\n";
  ss << "    \"page_size\": " << db_info_.page_size << ",\n";
  ss << "    \"buffer_pool_size\": " << db_info_.buffer_pool_size << ",\n";
  ss << "    \"wal_enabled\": " << (db_info_.wal_enabled ? "true" : "false") << "\n";
  ss << "  },\n";

  ss << "  \"performance_metrics\": {\n";
  ss << "    \"uptime\": \"" << metrics_.get_uptime() << "\",\n";
  ss << "    \"queries_executed\": " << metrics_.queries_executed << ",\n";
  ss << "    \"avg_query_time_ms\": " << metrics_.avg_query_time_ms << ",\n";
  ss << "    \"transactions_committed\": " << metrics_.transactions_committed << ",\n";
  ss << "    \"transactions_aborted\": " << metrics_.transactions_aborted << ",\n";
  ss << "    \"buffer_hit_rate\": " << metrics_.get_buffer_hit_rate() << ",\n";
  ss << "    \"pages_read\": " << metrics_.pages_read << ",\n";
  ss << "    \"pages_written\": " << metrics_.pages_written << ",\n";
  ss << "    \"active_connections\": " << metrics_.active_connections << "\n";
  ss << "  },\n";

  ss << "  \"health_status\": " << (check_database_health() ? "\"healthy\"" : "\"warning\"")
     << "\n";
  ss << "}\n";

  return ss.str();
}

std::string SystemDiagnostics::export_text_report() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::stringstream ss;

  ss << "=== LatticeDB System Diagnostics Report ===\n\n";

  ss << "Database Information:\n";
  ss << "  Version: " << db_info_.version << "\n";
  ss << "  Build Date: " << db_info_.build_date << "\n";
  ss << "  Page Size: " << db_info_.page_size << " bytes\n";
  ss << "  Buffer Pool Size: " << db_info_.buffer_pool_size << " pages\n";
  ss << "  WAL Enabled: " << (db_info_.wal_enabled ? "Yes" : "No") << "\n";
  ss << "  Compression: " << (db_info_.compression_enabled ? "Enabled" : "Disabled") << "\n\n";

  ss << "Performance Metrics:\n";
  ss << "  Uptime: " << metrics_.get_uptime() << "\n";
  ss << "  Queries Executed: " << metrics_.queries_executed << "\n";
  ss << "  Average Query Time: " << std::fixed << std::setprecision(2) << metrics_.avg_query_time_ms
     << " ms\n";
  ss << "  Transactions:\n";
  ss << "    Committed: " << metrics_.transactions_committed << "\n";
  ss << "    Aborted: " << metrics_.transactions_aborted << "\n";
  ss << "  Buffer Pool:\n";
  ss << "    Hit Rate: " << std::fixed << std::setprecision(1) << metrics_.get_buffer_hit_rate()
     << "%\n";
  ss << "    Hits: " << metrics_.buffer_hits << "\n";
  ss << "    Misses: " << metrics_.buffer_misses << "\n";
  ss << "  I/O Operations:\n";
  ss << "    Pages Read: " << metrics_.pages_read << "\n";
  ss << "    Pages Written: " << metrics_.pages_written << "\n";
  ss << "  Connections:\n";
  ss << "    Active: " << metrics_.active_connections << "\n";
  ss << "    Total: " << metrics_.total_connections << "\n\n";

  ss << "Health Status: " << (check_database_health() ? "HEALTHY" : "WARNING") << "\n\n";

  auto warnings = get_warnings();
  if (!warnings.empty()) {
    ss << "Warnings:\n";
    for (const auto& warning : warnings) {
      ss << "  - " << warning << "\n";
    }
    ss << "\n";
  }

  auto recommendations = get_recommendations();
  if (!recommendations.empty()) {
    ss << "Recommendations:\n";
    for (const auto& rec : recommendations) {
      ss << "  - " << rec << "\n";
    }
  }

  return ss.str();
}

void SystemDiagnostics::reset_metrics() {
  std::lock_guard<std::mutex> lock(mutex_);
  metrics_ = PerformanceMetrics();
  metrics_.start_time = std::chrono::system_clock::now();
}

void SystemDiagnostics::update_averages() {
  // Implementation for updating running averages
}

} // namespace latticedb