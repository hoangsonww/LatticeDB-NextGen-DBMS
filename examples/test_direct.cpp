// Direct SQL testing program for LatticeDB
// This bypasses the CLI and tests SQL directly through the database engine

#include "../src/common/result.h"
#include "../src/engine/database_engine.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace latticedb;

void print_separator() {
  std::cout << "----------------------------------------" << std::endl;
}

void print_query_result(const QueryResult& result) {
  if (!result.success) {
    std::cout << "FAILED: " << result.message << std::endl;
    return;
  }

  if (result.rows.empty()) {
    std::cout << result.message;
    if (result.rows_affected > 0) {
      std::cout << " (" << result.rows_affected << " rows affected)";
    }
    std::cout << std::endl;
    return;
  }

  // Print column headers
  for (size_t i = 0; i < result.column_names.size(); ++i) {
    if (i > 0)
      std::cout << " | ";
    std::cout << std::setw(15) << result.column_names[i];
  }
  std::cout << std::endl;

  // Print separator
  for (size_t i = 0; i < result.column_names.size(); ++i) {
    if (i > 0)
      std::cout << "-+-";
    std::cout << std::string(15, '-');
  }
  std::cout << std::endl;

  // Print rows
  for (const auto& row : result.rows) {
    for (size_t i = 0; i < row.size(); ++i) {
      if (i > 0)
        std::cout << " | ";
      std::cout << std::setw(15) << row[i].to_string();
    }
    std::cout << std::endl;
  }

  std::cout << "(" << result.rows.size() << " rows)" << std::endl;
}

void test_sql(DatabaseEngine& db, const std::string& description, const std::string& sql) {
  print_separator();
  std::cout << description << std::endl;
  std::cout << "SQL: " << sql << std::endl;
  std::cout << "Result: ";

  try {
    auto result = db.execute_sql(sql);
    print_query_result(result);
  } catch (const std::exception& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
  }
}

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "LatticeDB Direct SQL Test" << std::endl;
  std::cout << "========================================" << std::endl << std::endl;

  try {
    DatabaseEngine db;

    // Table creation
    test_sql(db, "1. CREATE EMPLOYEES TABLE",
             "CREATE TABLE employees ("
             "emp_id INT PRIMARY KEY, "
             "name VARCHAR(100), "
             "salary INT, "
             "dept_id INT)");

    test_sql(db, "2. CREATE DEPARTMENTS TABLE",
             "CREATE TABLE departments ("
             "dept_id INT PRIMARY KEY, "
             "dept_name VARCHAR(50), "
             "budget INT)");

    // Insert data
    test_sql(db, "3. INSERT EMPLOYEE 1",
             "INSERT INTO employees VALUES (1, 'John Smith', 120000, 1)");

    test_sql(db, "4. INSERT EMPLOYEE 2", "INSERT INTO employees VALUES (2, 'Jane Doe', 150000, 1)");

    test_sql(db, "5. INSERT EMPLOYEE 3",
             "INSERT INTO employees VALUES (3, 'Bob Johnson', 95000, 2)");

    test_sql(db, "6. INSERT EMPLOYEE 4",
             "INSERT INTO employees VALUES (4, 'Alice Williams', 105000, 2)");

    test_sql(db, "7. INSERT EMPLOYEE 5",
             "INSERT INTO employees VALUES (5, 'Charlie Brown', 85000, 3)");

    test_sql(db, "8. INSERT DEPARTMENT 1",
             "INSERT INTO departments VALUES (1, 'Engineering', 2000000)");

    test_sql(db, "9. INSERT DEPARTMENT 2", "INSERT INTO departments VALUES (2, 'Sales', 1500000)");

    test_sql(db, "10. INSERT DEPARTMENT 3",
             "INSERT INTO departments VALUES (3, 'Marketing', 1000000)");

    // Basic SELECT
    test_sql(db, "11. SELECT ALL EMPLOYEES", "SELECT * FROM employees");

    test_sql(db, "12. SELECT ALL DEPARTMENTS", "SELECT * FROM departments");

    // SELECT with WHERE
    test_sql(db, "13. SELECT HIGH SALARY EMPLOYEES",
             "SELECT name, salary FROM employees WHERE salary > 100000");

    test_sql(db, "14. SELECT ENGINEERING DEPT", "SELECT * FROM employees WHERE dept_id = 1");

    // JOIN operations
    test_sql(db, "15. INNER JOIN",
             "SELECT e.name, e.salary, d.dept_name "
             "FROM employees e "
             "INNER JOIN departments d ON e.dept_id = d.dept_id");

    test_sql(db, "16. LEFT JOIN",
             "SELECT d.dept_name, e.name "
             "FROM departments d "
             "LEFT JOIN employees e ON d.dept_id = e.dept_id");

    // Aggregations
    test_sql(db, "17. COUNT EMPLOYEES", "SELECT COUNT(*) FROM employees");

    test_sql(db, "18. SUM SALARIES", "SELECT SUM(salary) FROM employees");

    test_sql(db, "19. AVG SALARY", "SELECT AVG(salary) FROM employees");

    test_sql(db, "20. MIN/MAX SALARY", "SELECT MIN(salary), MAX(salary) FROM employees");

    // GROUP BY
    test_sql(db, "21. GROUP BY DEPARTMENT",
             "SELECT dept_id, COUNT(*) as emp_count, AVG(salary) as avg_salary "
             "FROM employees "
             "GROUP BY dept_id");

    test_sql(db, "22. GROUP BY WITH JOIN",
             "SELECT d.dept_name, COUNT(e.emp_id) as emp_count, SUM(e.salary) as total_salary "
             "FROM departments d "
             "LEFT JOIN employees e ON d.dept_id = e.dept_id "
             "GROUP BY d.dept_id, d.dept_name");

    // UPDATE
    test_sql(db, "23. UPDATE SALARY", "UPDATE employees SET salary = 100000 WHERE emp_id = 3");

    test_sql(db, "24. VERIFY UPDATE", "SELECT * FROM employees WHERE emp_id = 3");

    // DELETE
    test_sql(db, "25. DELETE EMPLOYEE", "DELETE FROM employees WHERE emp_id = 5");

    test_sql(db, "26. VERIFY DELETE", "SELECT COUNT(*) FROM employees");

    // Transactions
    test_sql(db, "27. BEGIN TRANSACTION", "BEGIN TRANSACTION");

    test_sql(db, "28. INSERT IN TRANSACTION",
             "INSERT INTO employees VALUES (6, 'New Employee', 90000, 1)");

    test_sql(db, "29. COMMIT TRANSACTION", "COMMIT");

    test_sql(db, "30. VERIFY TRANSACTION", "SELECT * FROM employees WHERE emp_id = 6");

    // Index creation
    test_sql(db, "31. CREATE INDEX", "CREATE INDEX idx_salary ON employees(salary)");

    // ORDER BY
    test_sql(db, "32. ORDER BY SALARY", "SELECT name, salary FROM employees ORDER BY salary DESC");

    // LIMIT
    test_sql(db, "33. TOP 3 SALARIES",
             "SELECT name, salary FROM employees ORDER BY salary DESC LIMIT 3");

    // Complex query
    test_sql(db, "34. COMPLEX QUERY",
             "SELECT d.dept_name, COUNT(e.emp_id) as count, AVG(e.salary) as avg_sal "
             "FROM departments d "
             "LEFT JOIN employees e ON d.dept_id = e.dept_id "
             "GROUP BY d.dept_id, d.dept_name "
             "HAVING AVG(e.salary) > 100000 OR AVG(e.salary) IS NULL");

    // Cleanup
    test_sql(db, "35. DROP TABLES", "DROP TABLE employees");

    test_sql(db, "36. DROP DEPARTMENTS", "DROP TABLE departments");

    print_separator();
    std::cout << "\nTest completed successfully!" << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}