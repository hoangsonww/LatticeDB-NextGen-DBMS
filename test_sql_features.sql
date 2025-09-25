-- Test SQL features: JOINs, GROUP BY, and Aggregates

-- Clean up any existing test tables
DROP TABLE IF EXISTS employees;
DROP TABLE IF EXISTS departments;
DROP TABLE IF EXISTS salaries;

-- Create test tables
CREATE TABLE departments (
    dept_id INTEGER PRIMARY KEY,
    dept_name VARCHAR(50),
    location VARCHAR(50)
);

CREATE TABLE employees (
    emp_id INTEGER PRIMARY KEY,
    emp_name VARCHAR(100),
    dept_id INTEGER,
    hire_date DATE,
    age INTEGER
);

CREATE TABLE salaries (
    emp_id INTEGER,
    salary DOUBLE,
    bonus DOUBLE,
    year INTEGER
);

-- Insert test data
INSERT INTO departments VALUES (1, 'Engineering', 'Building A');
INSERT INTO departments VALUES (2, 'Sales', 'Building B');
INSERT INTO departments VALUES (3, 'HR', 'Building C');
INSERT INTO departments VALUES (4, 'Marketing', 'Building D');

INSERT INTO employees VALUES (1, 'Alice Johnson', 1, '2020-01-15', 28);
INSERT INTO employees VALUES (2, 'Bob Smith', 1, '2019-03-22', 32);
INSERT INTO employees VALUES (3, 'Charlie Brown', 2, '2021-07-10', 25);
INSERT INTO employees VALUES (4, 'Diana Prince', 2, '2018-11-05', 35);
INSERT INTO employees VALUES (5, 'Eve Wilson', 3, '2020-09-18', 29);
INSERT INTO employees VALUES (6, 'Frank Miller', 1, '2022-02-28', 27);
INSERT INTO employees VALUES (7, 'Grace Lee', 4, '2019-06-12', 31);
INSERT INTO employees VALUES (8, 'Henry Davis', 4, '2021-01-20', 26);

INSERT INTO salaries VALUES (1, 75000, 5000, 2023);
INSERT INTO salaries VALUES (2, 85000, 7000, 2023);
INSERT INTO salaries VALUES (3, 65000, 3000, 2023);
INSERT INTO salaries VALUES (4, 72000, 4500, 2023);
INSERT INTO salaries VALUES (5, 60000, 2000, 2023);
INSERT INTO salaries VALUES (6, 70000, 4000, 2023);
INSERT INTO salaries VALUES (7, 68000, 3500, 2023);
INSERT INTO salaries VALUES (8, 62000, 2500, 2023);
INSERT INTO salaries VALUES (1, 72000, 4500, 2022);
INSERT INTO salaries VALUES (2, 82000, 6500, 2022);

-- Test basic SELECT
SELECT * FROM departments;
SELECT * FROM employees;
SELECT * FROM salaries WHERE year = 2023;

-- Test aggregate functions
SELECT COUNT(*) FROM employees;
SELECT COUNT(emp_id) FROM employees;
SELECT AVG(salary) FROM salaries WHERE year = 2023;
SELECT MIN(salary), MAX(salary) FROM salaries WHERE year = 2023;
SELECT SUM(salary + bonus) FROM salaries WHERE year = 2023;

-- Test GROUP BY with aggregates
SELECT dept_id, COUNT(*) FROM employees GROUP BY dept_id;
SELECT year, AVG(salary), SUM(bonus) FROM salaries GROUP BY year;
SELECT dept_id, MIN(age), MAX(age), AVG(age) FROM employees GROUP BY dept_id;

-- Test INNER JOIN
SELECT e.emp_name, d.dept_name
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id;

-- Test JOIN with WHERE clause
SELECT e.emp_name, d.dept_name, d.location
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id
WHERE d.location = 'Building A';

-- Test multiple JOINs
SELECT e.emp_name, d.dept_name, s.salary, s.bonus
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id
INNER JOIN salaries s ON e.emp_id = s.emp_id
WHERE s.year = 2023;

-- Test JOIN with GROUP BY
SELECT d.dept_name, COUNT(e.emp_id), AVG(s.salary)
FROM departments d
INNER JOIN employees e ON d.dept_id = e.dept_id
INNER JOIN salaries s ON e.emp_id = s.emp_id
WHERE s.year = 2023
GROUP BY d.dept_name;

-- Test LEFT JOIN
SELECT d.dept_name, COUNT(e.emp_id)
FROM departments d
LEFT JOIN employees e ON d.dept_id = e.dept_id
GROUP BY d.dept_name;

-- Complex query with JOIN, GROUP BY, HAVING, and ORDER BY
SELECT d.dept_name, COUNT(e.emp_id) as emp_count, AVG(s.salary) as avg_salary
FROM departments d
INNER JOIN employees e ON d.dept_id = e.dept_id
INNER JOIN salaries s ON e.emp_id = s.emp_id
WHERE s.year = 2023
GROUP BY d.dept_name
HAVING AVG(s.salary) > 65000
ORDER BY avg_salary DESC;

-- Test aggregates without GROUP BY
SELECT COUNT(*), AVG(age), MIN(age), MAX(age) FROM employees;

-- Clean up
DROP TABLE salaries;
DROP TABLE employees;
DROP TABLE departments;