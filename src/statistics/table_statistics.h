#pragma once
#include <mutex>

#include "../types/schema.h"
#include "../types/value.h"
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

namespace latticedb {

struct ColumnStatistics {
  std::string column_name;
  ValueType data_type;

  // Basic statistics
  uint64_t null_count = 0;
  uint64_t distinct_count = 0;
  double selectivity = 1.0;

  // Value range statistics
  Value min_value;
  Value max_value;
  double average_length = 0.0; // For string columns

  // Distribution statistics
  std::vector<Value> most_common_values;
  std::vector<double> most_common_frequencies;
  std::vector<std::pair<Value, Value>> histogram_bounds;
  std::vector<double> histogram_frequencies;

  // Advanced statistics
  double correlation_coefficient = 0.0; // With primary key if applicable
  std::chrono::system_clock::time_point last_updated;

  // Bloom filter for membership testing
  std::vector<uint64_t> bloom_filter_bits;
  uint32_t bloom_filter_hash_functions = 3;

  ColumnStatistics(const std::string& name, ValueType type)
      : column_name(name), data_type(type), last_updated(std::chrono::system_clock::now()) {}
};

struct TableStatistics {
  std::string table_name;
  uint64_t row_count = 0;
  uint64_t page_count = 0;
  double average_row_size = 0.0;

  std::unordered_map<std::string, std::unique_ptr<ColumnStatistics>> column_stats;

  // Join statistics
  std::unordered_map<std::string, double> foreign_key_selectivity; // table_name -> selectivity

  // Index statistics
  std::unordered_map<std::string, uint32_t> index_height;     // index_name -> height
  std::unordered_map<std::string, uint64_t> index_page_count; // index_name -> pages

  // Temporal statistics
  std::chrono::system_clock::time_point last_analyzed;
  std::chrono::system_clock::time_point last_modified;
  uint64_t modifications_since_analyze = 0;

  // Workload statistics
  uint64_t scan_count = 0;
  uint64_t seek_count = 0;
  std::unordered_map<std::string, uint64_t> predicate_usage; // predicate -> count

  TableStatistics(const std::string& name) : table_name(name) {}
};

class StatisticsCollector {
public:
  enum class SamplingMethod {
    RANDOM_SAMPLING,
    SYSTEMATIC_SAMPLING,
    RESERVOIR_SAMPLING,
    STRATIFIED_SAMPLING
  };

private:
  BufferPoolManager* buffer_pool_manager_;
  Catalog* catalog_;

  // Configuration
  uint32_t default_sample_size_ = 30000;
  uint32_t histogram_buckets_ = 100;
  uint32_t mcv_count_ = 10;        // Most Common Values count
  double analyze_threshold_ = 0.1; // Trigger auto-analyze after 10% changes

public:
  explicit StatisticsCollector(BufferPoolManager* bpm, Catalog* catalog);

  // Manual statistics collection
  std::unique_ptr<TableStatistics>
  analyze_table(const std::string& table_name,
                SamplingMethod method = SamplingMethod::RANDOM_SAMPLING);

  std::unique_ptr<ColumnStatistics> analyze_column(const std::string& table_name,
                                                   const std::string& column_name);

  // Automatic statistics maintenance
  void enable_auto_statistics(bool enable = true);
  void set_analyze_threshold(double threshold);

  // Statistics queries
  double estimate_selectivity(const std::string& table_name, const std::string& column_name,
                              const Expression& predicate);

  uint64_t estimate_distinct_count(const std::string& table_name, const std::string& column_name);

  double estimate_join_selectivity(const std::string& left_table, const std::string& left_column,
                                   const std::string& right_table, const std::string& right_column);

  // Bloom filter operations
  void build_bloom_filter(ColumnStatistics& col_stats, const std::vector<Value>& values);
  bool bloom_filter_contains(const ColumnStatistics& col_stats, const Value& value);

  // Histogram operations
  void build_equi_width_histogram(ColumnStatistics& col_stats, const std::vector<Value>& values);
  void build_equi_depth_histogram(ColumnStatistics& col_stats, const std::vector<Value>& values);
  double estimate_range_selectivity(const ColumnStatistics& col_stats, const Value& min_val,
                                    const Value& max_val);

  // Multi-dimensional statistics
  void collect_correlation_statistics(const std::string& table_name);
  double get_column_correlation(const std::string& table_name, const std::string& col1,
                                const std::string& col2);

  // Workload-based statistics
  void record_predicate_usage(const std::string& table_name, const Expression& predicate);
  void record_join_usage(const std::string& left_table, const std::string& right_table);
  std::vector<std::string> suggest_indexes(const std::string& table_name);

private:
  std::vector<Tuple> collect_sample(const std::string& table_name, uint32_t sample_size,
                                    SamplingMethod method);

  std::vector<Value> extract_column_values(const std::vector<Tuple>& sample, const Schema& schema,
                                           uint32_t column_index);

  void update_most_common_values(ColumnStatistics& col_stats, const std::vector<Value>& values);

  uint64_t hash_value_for_bloom_filter(const Value& value, uint32_t hash_index);

  // HyperLogLog for cardinality estimation
  std::vector<uint8_t> hyperloglog_sketch_;
  uint64_t estimate_cardinality_hll(const std::vector<Value>& values);
  void update_hll_sketch(const Value& value);
};

class StatisticsStorage {
private:
  std::unordered_map<std::string, std::unique_ptr<TableStatistics>> table_stats_;
  std::mutex stats_mutex_;
  std::string stats_file_path_;

public:
  explicit StatisticsStorage(const std::string& file_path = "latticedb_stats.json");

  void store_table_statistics(std::unique_ptr<TableStatistics> stats);
  std::unique_ptr<TableStatistics> load_table_statistics(const std::string& table_name);

  void persist_to_disk();
  void load_from_disk();

  void clear_statistics(const std::string& table_name = "");
  std::vector<std::string> list_tables_with_statistics();

  // Statistics aging
  void cleanup_old_statistics(std::chrono::hours max_age = std::chrono::hours(24));
  bool are_statistics_stale(const std::string& table_name,
                            std::chrono::hours max_age = std::chrono::hours(6));

private:
  void serialize_statistics();
  void deserialize_statistics();
};

// Cost-based optimization helpers
class CostEstimator {
public:
  static double estimate_scan_cost(const TableStatistics& table_stats,
                                   const Expression* predicate = nullptr);

  static double estimate_index_scan_cost(const TableStatistics& table_stats,
                                         const std::string& index_name,
                                         const Expression& predicate);

  static double estimate_join_cost(const TableStatistics& left_stats,
                                   const TableStatistics& right_stats,
                                   const Expression& join_condition,
                                   const std::string& join_algorithm);

  static double estimate_sort_cost(uint64_t row_count, uint32_t avg_row_size);

  static double estimate_hash_cost(uint64_t row_count, uint32_t hash_table_size);

  static uint64_t estimate_result_size(const TableStatistics& table_stats,
                                       const Expression& predicate);

private:
  static constexpr double IO_COST_PER_PAGE = 1.0;
  static constexpr double CPU_COST_PER_ROW = 0.01;
  static constexpr double MEMORY_COST_PER_MB = 0.1;
  static constexpr double RANDOM_IO_PENALTY = 4.0;
};

} // namespace latticedb