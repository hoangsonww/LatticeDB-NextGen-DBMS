// LatticeDB Performance Benchmark Suite
// This benchmark tests all major database operations including
// inserts, queries, joins, aggregations, indexes, and vector search

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <vector>

#include "../src/buffer/buffer_pool_manager.h"
#include "../src/engine/database_engine.h"
#include "../src/index/b_plus_tree.h"
#include "../src/ml/vector_search.h"
#include "../src/query/query_executor.h"
#include "../src/query/query_planner.h"
#include "../src/query/sql_parser.h"
#include "../src/storage/disk_manager.h"
#include "../src/transaction/transaction.h"

using namespace latticedb;
using namespace std::chrono;

// Benchmark result structure
struct BenchmarkResult {
  std::string name;
  double min_time_ms;
  double max_time_ms;
  double avg_time_ms;
  double stddev_ms;
  double throughput_ops_per_sec;
  size_t operations;
};

// Benchmark runner class
class BenchmarkRunner {
private:
  std::vector<BenchmarkResult> results_;
  bool verbose_;

public:
  explicit BenchmarkRunner(bool verbose = false) : verbose_(verbose) {}

  void run_benchmark(const std::string& name, std::function<void()> setup,
                     std::function<void()> benchmark, std::function<void()> teardown,
                     int iterations = 10, size_t ops_per_iteration = 1) {

    if (verbose_) {
      std::cout << "\nRunning benchmark: " << name << std::endl;
      std::cout << "Iterations: " << iterations << ", Ops/iteration: " << ops_per_iteration
                << std::endl;
    }

    std::vector<double> times;

    for (int i = 0; i < iterations; ++i) {
      if (verbose_ && i % (iterations / 10) == 0) {
        std::cout << "  Progress: " << (i * 100 / iterations) << "%" << std::endl;
      }

      // Setup
      if (setup)
        setup();

      // Benchmark
      auto start = high_resolution_clock::now();
      benchmark();
      auto end = high_resolution_clock::now();

      // Teardown
      if (teardown)
        teardown();

      duration<double, std::milli> elapsed = end - start;
      times.push_back(elapsed.count());
    }

    // Calculate statistics
    BenchmarkResult result;
    result.name = name;
    result.operations = ops_per_iteration * iterations;
    result.min_time_ms = *std::min_element(times.begin(), times.end());
    result.max_time_ms = *std::max_element(times.begin(), times.end());
    result.avg_time_ms = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    // Standard deviation
    double sq_sum = 0;
    for (double t : times) {
      sq_sum += (t - result.avg_time_ms) * (t - result.avg_time_ms);
    }
    result.stddev_ms = std::sqrt(sq_sum / times.size());

    // Throughput
    result.throughput_ops_per_sec = (ops_per_iteration * 1000.0) / result.avg_time_ms;

    results_.push_back(result);

    if (verbose_) {
      print_result(result);
    }
  }

  void print_result(const BenchmarkResult& r) {
    std::cout << "\n=== " << r.name << " ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Min: " << r.min_time_ms << " ms" << std::endl;
    std::cout << "  Max: " << r.max_time_ms << " ms" << std::endl;
    std::cout << "  Avg: " << r.avg_time_ms << " ms" << std::endl;
    std::cout << "  StdDev: " << r.stddev_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << r.throughput_ops_per_sec << " ops/sec" << std::endl;
  }

  void print_summary() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "BENCHMARK SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;

    // Table header
    std::cout << std::left << std::setw(35) << "Benchmark" << std::right << std::setw(12)
              << "Avg (ms)" << std::setw(12) << "Min (ms)" << std::setw(12) << "Max (ms)"
              << std::setw(12) << "StdDev" << std::setw(15) << "Throughput" << std::endl;
    std::cout << std::string(96, '-') << std::endl;

    // Results
    for (const auto& r : results_) {
      std::cout << std::left << std::setw(35) << r.name << std::right << std::fixed
                << std::setprecision(2) << std::setw(12) << r.avg_time_ms << std::setw(12)
                << r.min_time_ms << std::setw(12) << r.max_time_ms << std::setw(12) << r.stddev_ms
                << std::setw(15) << r.throughput_ops_per_sec << std::endl;
    }
    std::cout << std::string(96, '-') << std::endl;
  }

  void export_csv(const std::string& filename) {
    std::ofstream file(filename);

    // Header
    file << "Benchmark,Avg_ms,Min_ms,Max_ms,StdDev_ms,Throughput_ops_sec,Operations\n";

    // Data
    for (const auto& r : results_) {
      file << r.name << "," << r.avg_time_ms << "," << r.min_time_ms << "," << r.max_time_ms << ","
           << r.stddev_ms << "," << r.throughput_ops_per_sec << "," << r.operations << "\n";
    }

    file.close();
    std::cout << "\nResults exported to " << filename << std::endl;
  }
};

// Database benchmarks
class LatticeDBBenchmarks {
private:
  std::unique_ptr<DatabaseEngine> db_;
  std::mt19937 gen_;
  std::uniform_int_distribution<> int_dist_;
  std::uniform_real_distribution<> real_dist_;

  void setup_database() {
    db_ = std::make_unique<DatabaseEngine>();
    db_->execute_sql("CREATE TABLE IF NOT EXISTS bench_table ("
                     "id INT PRIMARY KEY, "
                     "value INT, "
                     "name VARCHAR(100), "
                     "score DOUBLE)");
  }

  void cleanup_database() {
    if (db_) {
      db_->execute_sql("DROP TABLE IF EXISTS bench_table");
      db_->execute_sql("DROP TABLE IF EXISTS bench_table2");
      db_->execute_sql("DROP TABLE IF EXISTS bench_index");
    }
  }

  std::string generate_random_string(size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
      result += charset[gen_() % (sizeof(charset) - 1)];
    }
    return result;
  }

public:
  LatticeDBBenchmarks()
      : gen_(std::random_device{}()), int_dist_(1, 1000000), real_dist_(0.0, 1000.0) {}

  void benchmark_sequential_insert(BenchmarkRunner& runner, int num_rows) {
    auto setup = [this]() { setup_database(); };

    auto benchmark = [this, num_rows]() {
      for (int i = 1; i <= num_rows; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Sequential Insert (" + std::to_string(num_rows) + " rows)", setup,
                         benchmark, teardown, 5, num_rows);
  }

  void benchmark_random_insert(BenchmarkRunner& runner, int num_rows) {
    auto setup = [this]() { setup_database(); };

    auto benchmark = [this, num_rows]() {
      std::vector<int> ids(num_rows);
      std::iota(ids.begin(), ids.end(), 1);
      std::shuffle(ids.begin(), ids.end(), gen_);

      for (int id : ids) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(id) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Random Insert (" + std::to_string(num_rows) + " rows)", setup, benchmark,
                         teardown, 5, num_rows);
  }

  void benchmark_point_query(BenchmarkRunner& runner, int table_size, int num_queries) {
    auto setup = [this, table_size]() {
      setup_database();
      // Insert test data
      for (int i = 1; i <= table_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this, table_size, num_queries]() {
      for (int i = 0; i < num_queries; ++i) {
        int target_id = (gen_() % table_size) + 1;
        std::string query = "SELECT * FROM bench_table WHERE id = " + std::to_string(target_id);
        db_->execute_sql(query);
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Point Query (table=" + std::to_string(table_size) +
                             ", queries=" + std::to_string(num_queries) + ")",
                         setup, benchmark, teardown, 5, num_queries);
  }

  void benchmark_range_query(BenchmarkRunner& runner, int table_size, int num_queries,
                             int range_size) {
    auto setup = [this, table_size]() {
      setup_database();
      // Insert test data
      for (int i = 1; i <= table_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this, table_size, num_queries, range_size]() {
      for (int i = 0; i < num_queries; ++i) {
        int start_id = (gen_() % (table_size - range_size)) + 1;
        int end_id = start_id + range_size;
        std::string query = "SELECT * FROM bench_table WHERE id BETWEEN " +
                            std::to_string(start_id) + " AND " + std::to_string(end_id);
        db_->execute_sql(query);
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Range Query (range=" + std::to_string(range_size) + ")", setup, benchmark,
                         teardown, 5, num_queries);
  }

  void benchmark_aggregation(BenchmarkRunner& runner, int table_size, int num_queries) {
    auto setup = [this, table_size]() {
      setup_database();
      // Insert test data
      for (int i = 1; i <= table_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_) % 100) + ", '" +
                            generate_random_string(20) + "', " + std::to_string(real_dist_(gen_)) +
                            ")";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this, num_queries]() {
      for (int i = 0; i < num_queries; ++i) {
        // Rotate through different aggregation queries
        switch (i % 4) {
        case 0:
          db_->execute_sql("SELECT COUNT(*) FROM bench_table");
          break;
        case 1:
          db_->execute_sql("SELECT SUM(value) FROM bench_table");
          break;
        case 2:
          db_->execute_sql("SELECT AVG(score) FROM bench_table");
          break;
        case 3:
          db_->execute_sql("SELECT value, COUNT(*) FROM bench_table GROUP BY value");
          break;
        }
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Aggregation (table=" + std::to_string(table_size) + ")", setup, benchmark,
                         teardown, 5, num_queries);
  }

  void benchmark_join(BenchmarkRunner& runner, int table1_size, int table2_size, int num_queries) {
    auto setup = [this, table1_size, table2_size]() {
      setup_database();

      // Create second table
      db_->execute_sql("CREATE TABLE bench_table2 ("
                       "id INT PRIMARY KEY, "
                       "foreign_id INT, "
                       "data VARCHAR(100))");

      // Insert into first table
      for (int i = 1; i <= table1_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }

      // Insert into second table
      for (int i = 1; i <= table2_size; ++i) {
        int foreign_id = (gen_() % table1_size) + 1;
        std::string query = "INSERT INTO bench_table2 VALUES (" + std::to_string(i) + ", " +
                            std::to_string(foreign_id) + ", '" + generate_random_string(30) + "')";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this, num_queries]() {
      for (int i = 0; i < num_queries; ++i) {
        db_->execute_sql("SELECT t1.id, t1.name, t2.data "
                         "FROM bench_table t1 "
                         "INNER JOIN bench_table2 t2 ON t1.id = t2.foreign_id");
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Join (t1=" + std::to_string(table1_size) +
                             ", t2=" + std::to_string(table2_size) + ")",
                         setup, benchmark, teardown, 5, num_queries);
  }

  void benchmark_index_creation(BenchmarkRunner& runner, int table_size) {
    auto setup = [this, table_size]() {
      setup_database();
      // Insert test data
      for (int i = 1; i <= table_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this]() { db_->execute_sql("CREATE INDEX idx_value ON bench_table(value)"); };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Index Creation (table=" + std::to_string(table_size) + ")", setup,
                         benchmark, teardown, 5, 1);
  }

  void benchmark_update(BenchmarkRunner& runner, int table_size, int num_updates) {
    auto setup = [this, table_size]() {
      setup_database();
      // Insert test data
      for (int i = 1; i <= table_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this, table_size, num_updates]() {
      for (int i = 0; i < num_updates; ++i) {
        int target_id = (gen_() % table_size) + 1;
        int new_value = int_dist_(gen_);
        std::string query = "UPDATE bench_table SET value = " + std::to_string(new_value) +
                            " WHERE id = " + std::to_string(target_id);
        db_->execute_sql(query);
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Update (table=" + std::to_string(table_size) +
                             ", updates=" + std::to_string(num_updates) + ")",
                         setup, benchmark, teardown, 5, num_updates);
  }

  void benchmark_delete(BenchmarkRunner& runner, int table_size, int num_deletes) {
    auto setup = [this, table_size]() {
      setup_database();
      // Insert test data
      for (int i = 1; i <= table_size; ++i) {
        std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(i) + ", " +
                            std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                            "', " + std::to_string(real_dist_(gen_)) + ")";
        db_->execute_sql(query);
      }
    };

    auto benchmark = [this, table_size, num_deletes]() {
      std::vector<int> ids(table_size);
      std::iota(ids.begin(), ids.end(), 1);
      std::shuffle(ids.begin(), ids.end(), gen_);

      for (int i = 0; i < num_deletes && i < table_size; ++i) {
        std::string query = "DELETE FROM bench_table WHERE id = " + std::to_string(ids[i]);
        db_->execute_sql(query);
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Delete (table=" + std::to_string(table_size) +
                             ", deletes=" + std::to_string(num_deletes) + ")",
                         setup, benchmark, teardown, 5, num_deletes);
  }

  void benchmark_transaction(BenchmarkRunner& runner, int ops_per_transaction,
                             int num_transactions) {
    auto setup = [this]() { setup_database(); };

    auto benchmark = [this, ops_per_transaction, num_transactions]() {
      for (int t = 0; t < num_transactions; ++t) {
        db_->execute_sql("BEGIN TRANSACTION");

        for (int i = 0; i < ops_per_transaction; ++i) {
          int id = t * ops_per_transaction + i + 1;
          std::string query = "INSERT INTO bench_table VALUES (" + std::to_string(id) + ", " +
                              std::to_string(int_dist_(gen_)) + ", '" + generate_random_string(20) +
                              "', " + std::to_string(real_dist_(gen_)) + ")";
          db_->execute_sql(query);
        }

        db_->execute_sql("COMMIT");
      }
    };

    auto teardown = [this]() { cleanup_database(); };

    runner.run_benchmark("Transaction (" + std::to_string(ops_per_transaction) + " ops/txn, " +
                             std::to_string(num_transactions) + " txns)",
                         setup, benchmark, teardown, 5, ops_per_transaction * num_transactions);
  }

  void run_all_benchmarks(BenchmarkRunner& runner) {
    std::cout << "\n==== DATABASE OPERATION BENCHMARKS ====\n" << std::endl;

    // Small scale benchmarks
    benchmark_sequential_insert(runner, 1000);
    benchmark_random_insert(runner, 1000);
    benchmark_point_query(runner, 1000, 100);
    benchmark_range_query(runner, 1000, 100, 50);
    benchmark_aggregation(runner, 1000, 20);
    benchmark_join(runner, 500, 1000, 10);
    benchmark_index_creation(runner, 1000);
    benchmark_update(runner, 1000, 100);
    benchmark_delete(runner, 1000, 100);
    benchmark_transaction(runner, 10, 50);
  }
};

// Vector search benchmarks
class VectorSearchBenchmarks {
private:
  std::unique_ptr<VectorSearchEngine> engine_;
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_real_distribution<> dist_;
  int dimensions_;
  int num_vectors_;

  std::vector<double> random_vector(size_t dim) {
    std::vector<double> vec;
    for (size_t i = 0; i < dim; i++) {
      vec.push_back(dist_(gen_));
    }
    return vec;
  }

public:
  VectorSearchBenchmarks() : gen_(rd_()), dist_(-1.0, 1.0) {
    engine_ = std::make_unique<VectorSearchEngine>();
  }

  void benchmark_vector_insert(BenchmarkRunner& runner, int num_vectors, size_t dimensions) {
    auto setup = [this, dimensions]() {
      engine_->drop_index("test_index");
      VectorSearchConfig config;
      config.metric = VectorDistanceMetric::L2;
      engine_->create_index("test_index", dimensions, VectorIndexType::HNSW, config);
    };

    auto benchmark = [this, num_vectors, dimensions]() {
      for (int i = 0; i < num_vectors; i++) {
        engine_->add_vector("test_index", i, random_vector(dimensions));
      }
    };

    auto teardown = [this]() { engine_->drop_index("test_index"); };

    runner.run_benchmark("Vector Insert (" + std::to_string(num_vectors) + " x " +
                             std::to_string(dimensions) + "D)",
                         setup, benchmark, teardown, 5, num_vectors);
  }

  void benchmark_vector_search(BenchmarkRunner& runner, int corpus_size, int num_queries,
                               size_t dimensions, int k) {
    auto setup = [this, corpus_size, dimensions]() {
      engine_->drop_index("test_index");
      VectorSearchConfig config;
      config.metric = VectorDistanceMetric::L2;
      engine_->create_index("test_index", dimensions, VectorIndexType::HNSW, config);
      for (int i = 0; i < corpus_size; i++) {
        engine_->add_vector("test_index", i, random_vector(dimensions));
      }
    };

    auto benchmark = [this, num_queries, dimensions, k]() {
      for (int i = 0; i < num_queries; i++) {
        auto query = random_vector(dimensions);
        engine_->search("test_index", query, k);
      }
    };

    auto teardown = [this]() { engine_->drop_index("test_index"); };

    runner.run_benchmark("Vector Search (corpus=" + std::to_string(corpus_size) +
                             ", k=" + std::to_string(k) + ")",
                         setup, benchmark, teardown, 5, num_queries);
  }

  void run_all_benchmarks(BenchmarkRunner& runner) {
    benchmark_vector_insert(runner, 1000, 128);
    benchmark_vector_insert(runner, 1000, 768);
    benchmark_vector_search(runner, 10000, 100, 128, 10);
    benchmark_vector_search(runner, 10000, 100, 768, 10);
  }
};

// Main benchmark entry point
int main(int argc, char* argv[]) {
  bool verbose = false;
  bool include_vector = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (arg == "--vector") {
      include_vector = true;
    } else if (arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  -v, --verbose    Show detailed progress" << std::endl;
      std::cout << "  --vector         Include vector search benchmarks" << std::endl;
      std::cout << "  --help           Show this help message" << std::endl;
      return 0;
    }
  }

  try {
    BenchmarkRunner runner(verbose);

    // Run database benchmarks
    {
      LatticeDBBenchmarks db_benchmarks;
      db_benchmarks.run_all_benchmarks(runner);
    }

    // Run vector search benchmarks if requested
    if (include_vector) {
      std::cout << "\n==== VECTOR SEARCH BENCHMARKS ====\n" << std::endl;
      VectorSearchBenchmarks vector_benchmarks;
      vector_benchmarks.run_all_benchmarks(runner);
    }

    // Print summary
    runner.print_summary();

    // Export to CSV
    runner.export_csv("benchmark_results.csv");

  } catch (const std::exception& e) {
    std::cerr << "Benchmark failed: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}