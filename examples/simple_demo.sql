-- LatticeDB Working Features Demo
-- This demonstrates the currently working features

-- Create simple tables
CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    name VARCHAR(100),
    salary INT,
    dept_id INT
);

CREATE TABLE departments (
    dept_id INT PRIMARY KEY,
    dept_name VARCHAR(50),
    budget INT
);

-- Insert data
INSERT INTO employees VALUES (1, 'John Smith', 120000, 1);
INSERT INTO employees VALUES (2, 'Jane Doe', 150000, 1);
INSERT INTO employees VALUES (3, 'Bob Johnson', 95000, 2);
INSERT INTO employees VALUES (4, 'Alice Williams', 105000, 2);
INSERT INTO employees VALUES (5, 'Charlie Brown', 85000, 3);

INSERT INTO departments VALUES (1, 'Engineering', 2000000);
INSERT INTO departments VALUES (2, 'Sales', 1500000);
INSERT INTO departments VALUES (3, 'Marketing', 1000000);

-- Basic SELECT
SELECT * FROM employees;
SELECT * FROM departments;

-- SELECT with WHERE
SELECT name, salary FROM employees WHERE salary > 100000;

-- JOIN query
SELECT e.name, e.salary, d.dept_name
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id;

-- GROUP BY with aggregates
SELECT dept_id, COUNT(*) as count, SUM(salary) as total, AVG(salary) as average
FROM employees
GROUP BY dept_id;

-- UPDATE
UPDATE employees SET salary = 100000 WHERE emp_id = 3;
SELECT * FROM employees WHERE emp_id = 3;

-- DELETE
DELETE FROM employees WHERE emp_id = 5;
SELECT COUNT(*) FROM employees;