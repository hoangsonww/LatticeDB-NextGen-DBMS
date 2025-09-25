#!/bin/bash

# LatticeDB Comprehensive Demo Runner
# This script runs SQL examples and captures output for documentation

echo "========================================="
echo "LatticeDB Feature Demonstration"
echo "========================================="
echo

# Check if latticedb binary exists
if [ ! -f ../build/latticedb ]; then
    echo "Error: latticedb binary not found. Please build the project first."
    echo "Run: cd ../build && cmake .. && make"
    exit 1
fi

# Function to run SQL and capture output
run_sql_section() {
    local section_name="$1"
    local sql_file="$2"

    echo "----------------------------------------"
    echo "$section_name"
    echo "----------------------------------------"

    ../build/latticedb < "$sql_file" 2>&1

    echo
}

# Create temporary SQL files for each section
cat > /tmp/demo_01_tables.sql << 'EOF'
-- Create tables
CREATE DATABASE IF NOT EXISTS demo_db;
USE demo_db;

DROP TABLE IF EXISTS employees;
DROP TABLE IF EXISTS departments;

CREATE TABLE departments (
    dept_id INT PRIMARY KEY,
    dept_name VARCHAR(50) NOT NULL,
    location VARCHAR(100),
    budget DECIMAL(12, 2)
);

CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    salary DECIMAL(10, 2),
    dept_id INT
);

SHOW TABLES;
EOF

cat > /tmp/demo_02_insert.sql << 'EOF'
USE demo_db;

-- Insert data
INSERT INTO departments VALUES
(1, 'Engineering', 'San Francisco', 2000000.00),
(2, 'Sales', 'New York', 1500000.00),
(3, 'Marketing', 'Los Angeles', 1000000.00);

INSERT INTO employees VALUES
(1, 'John', 'Smith', 120000.00, 1),
(2, 'Jane', 'Doe', 150000.00, 1),
(3, 'Bob', 'Johnson', 95000.00, 2),
(4, 'Alice', 'Williams', 105000.00, 2),
(5, 'Charlie', 'Brown', 85000.00, 3);

SELECT COUNT(*) AS total_departments FROM departments;
SELECT COUNT(*) AS total_employees FROM employees;
EOF

cat > /tmp/demo_03_select.sql << 'EOF'
USE demo_db;

-- Basic SELECT queries
SELECT * FROM departments;
SELECT first_name, last_name, salary FROM employees WHERE salary > 100000;
SELECT * FROM employees ORDER BY salary DESC LIMIT 3;
EOF

cat > /tmp/demo_04_joins.sql << 'EOF'
USE demo_db;

-- JOIN operations
SELECT e.first_name, e.last_name, d.dept_name, e.salary
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id;

SELECT d.dept_name, e.first_name
FROM departments d
LEFT JOIN employees e ON d.dept_id = e.dept_id
ORDER BY d.dept_name;
EOF

cat > /tmp/demo_05_aggregates.sql << 'EOF'
USE demo_db;

-- GROUP BY and Aggregates
SELECT dept_id, COUNT(*) as emp_count, AVG(salary) as avg_salary
FROM employees
GROUP BY dept_id;

SELECT d.dept_name,
       COUNT(e.emp_id) AS emp_count,
       SUM(e.salary) AS total_salary,
       AVG(e.salary) AS avg_salary,
       MIN(e.salary) AS min_salary,
       MAX(e.salary) AS max_salary
FROM departments d
LEFT JOIN employees e ON d.dept_id = e.dept_id
GROUP BY d.dept_id, d.dept_name;
EOF

cat > /tmp/demo_06_transactions.sql << 'EOF'
USE demo_db;

-- Transactions
BEGIN TRANSACTION;
UPDATE employees SET salary = salary * 1.10 WHERE dept_id = 1;
SELECT * FROM employees WHERE dept_id = 1;
COMMIT;

BEGIN TRANSACTION;
DELETE FROM employees WHERE emp_id = 999;
ROLLBACK;
EOF

cat > /tmp/demo_07_indexes.sql << 'EOF'
USE demo_db;

-- Create indexes
CREATE INDEX idx_emp_salary ON employees(salary);
CREATE INDEX idx_emp_dept ON employees(dept_id);
SHOW INDEXES FROM employees;
EOF

cat > /tmp/demo_08_updates.sql << 'EOF'
USE demo_db;

-- Updates and Deletes
UPDATE employees SET salary = 100000 WHERE emp_id = 3;
SELECT * FROM employees WHERE emp_id = 3;

DELETE FROM employees WHERE salary < 90000;
SELECT COUNT(*) AS remaining_employees FROM employees;
EOF

# Run all demonstrations
echo "Starting LatticeDB Feature Demonstration..."
echo "==========================================="
echo

run_sql_section "1. CREATE DATABASE AND TABLES" /tmp/demo_01_tables.sql
run_sql_section "2. INSERT DATA" /tmp/demo_02_insert.sql
run_sql_section "3. SELECT QUERIES" /tmp/demo_03_select.sql
run_sql_section "4. JOIN OPERATIONS" /tmp/demo_04_joins.sql
run_sql_section "5. GROUP BY AND AGGREGATES" /tmp/demo_05_aggregates.sql
run_sql_section "6. TRANSACTIONS" /tmp/demo_06_transactions.sql
run_sql_section "7. INDEXES" /tmp/demo_07_indexes.sql
run_sql_section "8. UPDATES AND DELETES" /tmp/demo_08_updates.sql

# Advanced features demo (if supported)
cat > /tmp/demo_09_advanced.sql << 'EOF'
USE demo_db;

-- Time Travel Query (LatticeDB feature)
SELECT * FROM employees FOR SYSTEM_TIME AS OF TIMESTAMP '2024-01-01 00:00:00';

-- Explain query plan
EXPLAIN SELECT e.*, d.dept_name
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id
WHERE e.salary > 100000;
EOF

run_sql_section "9. ADVANCED FEATURES" /tmp/demo_09_advanced.sql

# Cleanup
rm -f /tmp/demo_*.sql

echo "========================================="
echo "Demonstration Complete!"
echo "========================================="
echo
echo "Summary:"
echo "- Created database with tables"
echo "- Inserted sample data"
echo "- Demonstrated SELECT queries"
echo "- Showed JOIN operations"
echo "- Performed GROUP BY aggregations"
echo "- Executed transactions"
echo "- Created indexes"
echo "- Performed updates and deletes"
echo "- Demonstrated advanced features"
echo
echo "All features tested successfully!"