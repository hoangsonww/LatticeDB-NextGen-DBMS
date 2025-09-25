-- Test CREATE TABLE
CREATE TABLE employees (
    id INT,
    name VARCHAR(100),
    salary DOUBLE,
    active BOOLEAN
);

-- Test INSERT
INSERT INTO employees VALUES (1, 'John', 50000.0, 1);
INSERT INTO employees VALUES (2, 'Jane', 60000.0, 1), (3, 'Bob', 45000.0, 0);

-- Test SELECT
SELECT * FROM employees;
SELECT name, salary FROM employees WHERE salary > 50000;

-- Test UPDATE  
UPDATE employees SET salary = 55000 WHERE name = 'John';
SELECT * FROM employees WHERE name = 'John';

-- Test DELETE
DELETE FROM employees WHERE active = 0;
SELECT * FROM employees;

-- Test Transactions
BEGIN;
INSERT INTO employees VALUES (4, 'Alice', 70000.0, 1);
ROLLBACK;
SELECT * FROM employees;

BEGIN;
INSERT INTO employees VALUES (5, 'Charlie', 80000.0, 1);
COMMIT;
SELECT * FROM employees;

-- Test JOIN
CREATE TABLE departments (id INT, name VARCHAR(50), manager_id INT);
INSERT INTO departments VALUES (1, 'Engineering', 1), (2, 'Sales', 2);
SELECT e.name, d.name FROM employees e JOIN departments d ON e.id = d.manager_id;

-- Test Aggregates
SELECT COUNT(*) FROM employees;
SELECT MAX(salary), MIN(salary), AVG(salary) FROM employees;

-- Test GROUP BY
SELECT active, COUNT(*) FROM employees GROUP BY active;

-- Test ORDER BY
SELECT * FROM employees ORDER BY salary DESC;

-- Test constraints
CREATE TABLE test_constraints (
    id INT PRIMARY KEY,
    email VARCHAR(100) UNIQUE,
    age INT CHECK (age >= 18),
    name VARCHAR(100) NOT NULL
);

-- Test index
CREATE INDEX idx_emp_name ON employees(name);
DROP INDEX idx_emp_name;

-- Test DROP TABLE
DROP TABLE departments;
DROP TABLE employees;
DROP TABLE test_constraints;
