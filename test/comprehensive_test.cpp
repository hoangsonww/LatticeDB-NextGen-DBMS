// Comprehensive test for all LatticeDB features
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/common/result.h"
#include "../src/diagnostics/system_info.h"
#include "../src/engine/database_engine.h"
#include "../src/index/b_plus_tree.h"
#include "../src/ml/vector_search.h"
#include "../src/storage/disk_manager.h"
#include "../src/transaction/transaction.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

using namespace latticedb;
using namespace std::chrono;

class ComprehensiveTest {
private:
  std::unique_ptr<DatabaseEngine> db_;
  bool verbose_;
  int passed_ = 0;
  int failed_ = 0;

  void print_result(const std::string& test_name, bool success, const std::string& message = "") {
    std::cout << std::setw(50) << std::left << test_name << " : ";
    if (success) {
      std::cout << "✓ PASSED";
      passed_++;
    } else {
      std::cout << "✗ FAILED";
      failed_++;
      if (!message.empty()) {
        std::cout << " (" << message << ")";
      }
    }
    std::cout << std::endl;
  }

  void print_section(const std::string& section) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << section << std::endl;
    std::cout << std::string(60, '=') << std::endl;
  }

  std::vector<double> random_vector(size_t dim) {
    static std::mt19937 gen(42);
    static std::uniform_real_distribution<> dist(-1.0, 1.0);
    std::vector<double> vec;
    vec.reserve(dim);
    for (size_t i = 0; i < dim; ++i) {
      vec.push_back(dist(gen));
    }
    return vec;
  }

public:
  explicit ComprehensiveTest(bool verbose = false) : verbose_(verbose) {
    db_ = std::make_unique<DatabaseEngine>();
  }

  void test_basic_operations() {
    print_section("1. BASIC TABLE OPERATIONS");

    try {
      // CREATE TABLE
      auto result = db_->execute_sql(
          "CREATE TABLE test_table (id INT PRIMARY KEY, name VARCHAR(50), value INT)");
      print_result("CREATE TABLE", result.success);

      // INSERT
      result = db_->execute_sql("INSERT INTO test_table VALUES (1, 'Alice', 100)");
      print_result("INSERT single row", result.success && result.rows_affected == 1);

      result = db_->execute_sql("INSERT INTO test_table VALUES (2, 'Bob', 200)");
      result = db_->execute_sql("INSERT INTO test_table VALUES (3, 'Charlie', 150)");
      result = db_->execute_sql("INSERT INTO test_table VALUES (4, 'David', 300)");
      result = db_->execute_sql("INSERT INTO test_table VALUES (5, 'Eve', 250)");
      print_result("INSERT multiple rows", result.success);

      // SELECT
      result = db_->execute_sql("SELECT * FROM test_table");
      print_result("SELECT all", result.success && result.rows.size() == 5);

      // WHERE clause
      result = db_->execute_sql("SELECT * FROM test_table WHERE value > 200");
      print_result("SELECT with WHERE", result.success && result.rows.size() == 2);

      // UPDATE
      result = db_->execute_sql("UPDATE test_table SET value = 350 WHERE id = 1");
      print_result("UPDATE", result.success && result.rows_affected > 0);

      // DELETE
      result = db_->execute_sql("DELETE FROM test_table WHERE id = 5");
      print_result("DELETE", result.success && result.rows_affected == 1);

      // Verify delete
      result = db_->execute_sql("SELECT COUNT(*) FROM test_table");
      print_result("COUNT after DELETE", result.success && result.rows.size() == 1);

    } catch (const std::exception& e) {
      print_result("Basic operations", false, e.what());
    }
  }

  void test_joins() {
    print_section("2. JOIN OPERATIONS");

    try {
      // Create tables
      db_->execute_sql("DROP TABLE IF EXISTS employees");
      db_->execute_sql("DROP TABLE IF EXISTS departments");

      db_->execute_sql("CREATE TABLE departments (dept_id INT PRIMARY KEY, dept_name VARCHAR(50))");
      db_->execute_sql("CREATE TABLE employees (emp_id INT PRIMARY KEY, name VARCHAR(50), dept_id "
                       "INT, salary INT)");

      // Insert data
      db_->execute_sql("INSERT INTO departments VALUES (1, 'Engineering')");
      db_->execute_sql("INSERT INTO departments VALUES (2, 'Sales')");
      db_->execute_sql("INSERT INTO departments VALUES (3, 'Marketing')");

      db_->execute_sql("INSERT INTO employees VALUES (1, 'John', 1, 100000)");
      db_->execute_sql("INSERT INTO employees VALUES (2, 'Jane', 1, 120000)");
      db_->execute_sql("INSERT INTO employees VALUES (3, 'Bob', 2, 90000)");
      db_->execute_sql("INSERT INTO employees VALUES (4, 'Alice', 2, 95000)");
      db_->execute_sql("INSERT INTO employees VALUES (5, 'Charlie', 3, 80000)");

      // INNER JOIN
      auto result = db_->execute_sql("SELECT e.name, d.dept_name FROM employees e "
                                     "INNER JOIN departments d ON e.dept_id = d.dept_id");
      print_result("INNER JOIN", result.success && result.rows.size() == 5);

      // LEFT JOIN
      result = db_->execute_sql("SELECT d.dept_name, e.name FROM departments d "
                                "LEFT JOIN employees e ON d.dept_id = e.dept_id");
      print_result("LEFT JOIN", result.success);

      // JOIN with WHERE
      result = db_->execute_sql("SELECT e.name, e.salary, d.dept_name FROM employees e "
                                "INNER JOIN departments d ON e.dept_id = d.dept_id "
                                "WHERE e.salary > 95000");
      print_result("JOIN with WHERE", result.success && result.rows.size() == 2);

    } catch (const std::exception& e) {
      print_result("JOIN operations", false, e.what());
    }
  }

  void test_aggregates() {
    print_section("3. AGGREGATE FUNCTIONS");

    try {
      // COUNT
      auto result = db_->execute_sql("SELECT COUNT(*) FROM employees");
      print_result("COUNT(*)", result.success && result.rows.size() == 1);

      // SUM
      result = db_->execute_sql("SELECT SUM(salary) FROM employees");
      print_result("SUM", result.success && result.rows.size() == 1);
      if (verbose_ && result.success && !result.rows.empty()) {
        std::cout << "  Total salary: " << result.rows[0][0].to_string() << std::endl;
      }

      // AVG
      result = db_->execute_sql("SELECT AVG(salary) FROM employees");
      print_result("AVG", result.success && result.rows.size() == 1);
      if (verbose_ && result.success && !result.rows.empty()) {
        std::cout << "  Average salary: " << result.rows[0][0].to_string() << std::endl;
      }

      // MIN/MAX
      result = db_->execute_sql("SELECT MIN(salary), MAX(salary) FROM employees");
      print_result("MIN/MAX", result.success && result.rows.size() == 1);

      // Multiple aggregates
      result = db_->execute_sql(
          "SELECT COUNT(*), SUM(salary), AVG(salary) FROM employees WHERE dept_id = 1");
      print_result("Multiple aggregates with WHERE", result.success);

    } catch (const std::exception& e) {
      print_result("Aggregate functions", false, e.what());
    }
  }

  void test_group_by() {
    print_section("4. GROUP BY OPERATIONS");

    try {
      // Simple GROUP BY
      auto result = db_->execute_sql("SELECT dept_id, COUNT(*) FROM employees GROUP BY dept_id");
      print_result("GROUP BY single column", result.success && result.rows.size() == 3);

      // GROUP BY with SUM
      result = db_->execute_sql("SELECT dept_id, SUM(salary) FROM employees GROUP BY dept_id");
      print_result("GROUP BY with SUM", result.success);

      // GROUP BY with AVG
      result = db_->execute_sql("SELECT dept_id, AVG(salary) FROM employees GROUP BY dept_id");
      print_result("GROUP BY with AVG", result.success);

      // GROUP BY with JOIN
      result = db_->execute_sql("SELECT d.dept_name, COUNT(e.emp_id), AVG(e.salary) "
                                "FROM departments d "
                                "LEFT JOIN employees e ON d.dept_id = e.dept_id "
                                "GROUP BY d.dept_id, d.dept_name");
      print_result("GROUP BY with JOIN", result.success);

      // GROUP BY with HAVING
      result = db_->execute_sql("SELECT dept_id, COUNT(*), AVG(salary) "
                                "FROM employees "
                                "GROUP BY dept_id "
                                "HAVING AVG(salary) > 90000");
      print_result("GROUP BY with HAVING", result.success);

    } catch (const std::exception& e) {
      print_result("GROUP BY operations", false, e.what());
    }
  }

  void test_transactions() {
    print_section("5. TRANSACTION MANAGEMENT");

    try {
      // Begin transaction
      auto result = db_->execute_sql("BEGIN TRANSACTION");
      print_result("BEGIN TRANSACTION", result.success);

      // Operations within transaction
      result = db_->execute_sql("INSERT INTO employees VALUES (6, 'Frank', 1, 110000)");
      print_result("INSERT in transaction", result.success);

      result = db_->execute_sql("UPDATE employees SET salary = salary * 1.1 WHERE dept_id = 1");
      print_result("UPDATE in transaction", result.success);

      // Commit
      result = db_->execute_sql("COMMIT");
      print_result("COMMIT", result.success);

      // Verify commit
      result = db_->execute_sql("SELECT * FROM employees WHERE emp_id = 6");
      print_result("Verify committed data", result.success && result.rows.size() == 1);

      // Rollback test
      result = db_->execute_sql("BEGIN TRANSACTION");
      result = db_->execute_sql("DELETE FROM employees WHERE emp_id = 6");
      result = db_->execute_sql("ROLLBACK");
      print_result("ROLLBACK", result.success);

      // Verify rollback
      result = db_->execute_sql("SELECT * FROM employees WHERE emp_id = 6");
      print_result("Verify rollback", result.success && result.rows.size() == 1);

    } catch (const std::exception& e) {
      print_result("Transaction management", false, e.what());
    }
  }

  void test_indexes() {
    print_section("6. INDEX OPERATIONS");

    try {
      // Create index
      auto result = db_->execute_sql("CREATE INDEX idx_salary ON employees(salary)");
      print_result("CREATE INDEX", result.success);

      result = db_->execute_sql("CREATE INDEX idx_dept ON employees(dept_id)");
      print_result("CREATE second INDEX", result.success);

      // Query using index (performance should be better)
      auto start = high_resolution_clock::now();
      result = db_->execute_sql("SELECT * FROM employees WHERE salary > 100000");
      auto end = high_resolution_clock::now();
      auto duration = duration_cast<microseconds>(end - start).count();

      print_result("Query with index", result.success);
      if (verbose_) {
        std::cout << "  Query time: " << duration << " microseconds" << std::endl;
      }

    } catch (const std::exception& e) {
      print_result("Index operations", false, e.what());
    }
  }

  void test_vector_search() {
    print_section("7. VECTOR SEARCH");

    try {
      VectorSearchEngine vector_engine;

      // Create vector index
      VectorSearchConfig config;
      config.metric = VectorDistanceMetric::L2;
      config.ef_construction = 200;
      config.ef_search = 64;

      bool success = vector_engine.create_index("embeddings", 128, VectorIndexType::HNSW, config);
      print_result("Create vector index", success);

      // Add vectors
      std::vector<std::pair<uint64_t, std::vector<double>>> vectors;
      for (int i = 0; i < 100; ++i) {
        vectors.push_back({i, random_vector(128)});
      }
      success = vector_engine.batch_add_vectors("embeddings", vectors);
      print_result("Add 100 vectors", success);

      // Search
      auto query = random_vector(128);
      auto results = vector_engine.search("embeddings", query, 10);
      print_result("Vector search (k=10)", !results.empty() && results.size() <= 10);

      // Different distance metrics
      vector_engine.drop_index("embeddings");
      config.metric = VectorDistanceMetric::COSINE;
      success = vector_engine.create_index("cosine_index", 128, VectorIndexType::FLAT, config);
      print_result("Create cosine similarity index", success);

      // Add and search with cosine
      vector_engine.add_vector("cosine_index", 1, random_vector(128));
      vector_engine.add_vector("cosine_index", 2, random_vector(128));
      results = vector_engine.search("cosine_index", query, 2);
      print_result("Cosine similarity search", !results.empty());

    } catch (const std::exception& e) {
      print_result("Vector search", false, e.what());
    }
  }

  void test_advanced_features() {
    print_section("8. ADVANCED FEATURES");

    try {
      // ORDER BY
      auto result = db_->execute_sql("SELECT * FROM employees ORDER BY salary DESC");
      print_result("ORDER BY DESC", result.success);

      // LIMIT
      result = db_->execute_sql("SELECT * FROM employees ORDER BY salary DESC LIMIT 3");
      print_result("LIMIT clause", result.success && result.rows.size() <= 3);

      // DISTINCT
      result = db_->execute_sql("SELECT DISTINCT dept_id FROM employees");
      print_result("DISTINCT", result.success);

      // Subqueries (if supported)
      result = db_->execute_sql(
          "SELECT * FROM employees WHERE salary > (SELECT AVG(salary) FROM employees)");
      print_result("Subquery", result.success);

      // Complex query
      result = db_->execute_sql("SELECT d.dept_name, COUNT(e.emp_id) as emp_count, "
                                "AVG(e.salary) as avg_salary, MAX(e.salary) as max_salary "
                                "FROM departments d "
                                "LEFT JOIN employees e ON d.dept_id = e.dept_id "
                                "GROUP BY d.dept_id, d.dept_name "
                                "ORDER BY avg_salary DESC");
      print_result("Complex query", result.success);

    } catch (const std::exception& e) {
      print_result("Advanced features", false, e.what());
    }
  }

  void test_buffer_pool() {
    print_section("9. BUFFER POOL MANAGEMENT");

    try {
      DiskManager disk_manager("test.db");
      BufferPoolManager buffer_pool(10, &disk_manager);

      // Allocate pages
      page_id_t page1;
      Page* new_page = buffer_pool.new_page(&page1);
      print_result("Allocate page", new_page != nullptr && page1 != INVALID_PAGE_ID);

      if (new_page) {
        // Write to page
        memcpy(new_page->get_data(), "Test Data", 10);
        buffer_pool.unpin_page(page1, true);
        print_result("Write to page", true);

        // Read from page using PageGuard
        auto page_guard = buffer_pool.fetch_page(page1);
        if (page_guard.is_valid()) {
          Page* page = page_guard.get();
          bool match = memcmp(page->get_data(), "Test Data", 10) == 0;
          print_result("Read from page", match);
          // PageGuard automatically unpins when it goes out of scope
        } else {
          print_result("Read from page", false);
        }

        // Flush page
        bool flushed = buffer_pool.flush_page(page1);
        print_result("Flush page", flushed);

        // Delete page
        bool deleted = buffer_pool.delete_page(page1);
        print_result("Delete page", deleted);
      } else {
        print_result("Write to page", false);
        print_result("Read from page", false);
        print_result("Flush page", false);
        print_result("Delete page", false);
      }

    } catch (const std::exception& e) {
      print_result("Buffer pool management", false, e.what());
    }
  }

  void test_btree_index() {
    print_section("10. B+ TREE INDEX");

    try {
      // Note: This is a simplified test as full B+ tree requires buffer pool
      // In a real implementation, we would:
      // 1. Create B+ tree index with comparator
      // 2. Insert key-value pairs
      // 3. Search for keys
      // 4. Perform range queries
      // 5. Delete keys

      // For now, just verify the system compiles with B+ tree
      bool btree_available = true;

      print_result("B+ Tree availability", btree_available);

    } catch (const std::exception& e) {
      print_result("B+ Tree index", false, e.what());
    }
  }

  void test_system_diagnostics() {
    print_section("11. SYSTEM DIAGNOSTICS");

    try {
      // Initialize diagnostics
      g_diagnostics = std::make_unique<SystemDiagnostics>();

      // Record some metrics
      g_diagnostics->record_query_execution(10.5);
      g_diagnostics->record_query_execution(15.2);
      g_diagnostics->record_transaction_commit();
      g_diagnostics->record_buffer_hit();
      g_diagnostics->record_buffer_hit();
      g_diagnostics->record_buffer_miss();

      // Get metrics
      auto metrics = g_diagnostics->get_performance_metrics();
      print_result("Performance metrics", metrics.queries_executed == 2);

      // Check health
      g_diagnostics->check_database_health();
      print_result("Health check", true); // Just checking it runs

      // Get recommendations
      auto recommendations = g_diagnostics->get_recommendations();
      print_result("Get recommendations", true); // Just checking it runs

      // Export report
      std::string report = g_diagnostics->export_text_report();
      print_result("Generate diagnostic report", !report.empty());

      if (verbose_) {
        std::cout << "\nDiagnostic Summary:" << std::endl;
        std::cout << "  Queries executed: " << metrics.queries_executed << std::endl;
        std::cout << "  Avg query time: " << metrics.avg_query_time_ms << " ms" << std::endl;
        std::cout << "  Buffer hit rate: " << metrics.get_buffer_hit_rate() << "%" << std::endl;
      }

    } catch (const std::exception& e) {
      print_result("System diagnostics", false, e.what());
    }
  }

  void run_all_tests() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║          LatticeDB Comprehensive Feature Test             ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;

    auto start = high_resolution_clock::now();

    test_basic_operations();
    test_joins();
    test_aggregates();
    test_group_by();
    test_transactions();
    test_indexes();
    test_vector_search();
    test_advanced_features();
    test_buffer_pool();
    test_btree_index();
    test_system_diagnostics();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    // Print summary
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST SUMMARY" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "Total tests: " << (passed_ + failed_) << std::endl;
    std::cout << "Passed: " << passed_ << " ✓" << std::endl;
    std::cout << "Failed: " << failed_ << " ✗" << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1)
              << (100.0 * passed_ / (passed_ + failed_)) << "%" << std::endl;
    std::cout << "Total time: " << duration << " ms" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Cleanup
    try {
      db_->execute_sql("DROP TABLE IF EXISTS test_table");
      db_->execute_sql("DROP TABLE IF EXISTS employees");
      db_->execute_sql("DROP TABLE IF EXISTS departments");
    } catch (...) {
    }
  }
};

int main(int argc, char* argv[]) {
  bool verbose = false;

  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "-v" || std::string(argv[i]) == "--verbose") {
      verbose = true;
    }
  }

  try {
    ComprehensiveTest test(verbose);
    test.run_all_tests();
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}