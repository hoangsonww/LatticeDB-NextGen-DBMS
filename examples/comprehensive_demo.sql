-- LatticeDB Comprehensive Feature Demonstration
-- This file demonstrates all major features of LatticeDB

-- ========================================
-- 1. DATABASE AND TABLE OPERATIONS
-- ========================================

-- Create database (if not exists)
CREATE DATABASE IF NOT EXISTS demo_db;
USE demo_db;

-- Drop tables if they exist (cleanup from previous runs)
DROP TABLE IF EXISTS order_items;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS products;
DROP TABLE IF EXISTS customers;
DROP TABLE IF EXISTS employees;
DROP TABLE IF EXISTS departments;
DROP TABLE IF EXISTS audit_log;
DROP TABLE IF EXISTS documents;

-- Create departments table
CREATE TABLE departments (
    dept_id INT PRIMARY KEY,
    dept_name VARCHAR(50) NOT NULL,
    location VARCHAR(100),
    budget DECIMAL(12, 2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create employees table with foreign key
CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE,
    hire_date DATE,
    salary DECIMAL(10, 2),
    dept_id INT,
    manager_id INT,
    is_active BOOLEAN DEFAULT TRUE,
    FOREIGN KEY (dept_id) REFERENCES departments(dept_id),
    FOREIGN KEY (manager_id) REFERENCES employees(emp_id)
);

-- Create customers table
CREATE TABLE customers (
    customer_id INT PRIMARY KEY AUTO_INCREMENT,
    company_name VARCHAR(100) NOT NULL,
    contact_name VARCHAR(50),
    email VARCHAR(100) UNIQUE,
    phone VARCHAR(20),
    address VARCHAR(200),
    city VARCHAR(50),
    country VARCHAR(50),
    credit_limit DECIMAL(10, 2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create products table
CREATE TABLE products (
    product_id INT PRIMARY KEY AUTO_INCREMENT,
    product_name VARCHAR(100) NOT NULL,
    category VARCHAR(50),
    unit_price DECIMAL(10, 2) NOT NULL CHECK (unit_price > 0),
    units_in_stock INT DEFAULT 0,
    reorder_level INT DEFAULT 10,
    discontinued BOOLEAN DEFAULT FALSE
);

-- Create orders table
CREATE TABLE orders (
    order_id INT PRIMARY KEY AUTO_INCREMENT,
    customer_id INT NOT NULL,
    employee_id INT,
    order_date DATE NOT NULL,
    required_date DATE,
    shipped_date DATE,
    ship_via VARCHAR(50),
    freight DECIMAL(10, 2) DEFAULT 0.00,
    ship_address VARCHAR(200),
    order_status VARCHAR(20) DEFAULT 'PENDING',
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id),
    FOREIGN KEY (employee_id) REFERENCES employees(emp_id)
);

-- Create order_items table (many-to-many relationship)
CREATE TABLE order_items (
    order_id INT,
    product_id INT,
    quantity INT NOT NULL CHECK (quantity > 0),
    unit_price DECIMAL(10, 2) NOT NULL,
    discount DECIMAL(3, 2) DEFAULT 0.00,
    PRIMARY KEY (order_id, product_id),
    FOREIGN KEY (order_id) REFERENCES orders(order_id),
    FOREIGN KEY (product_id) REFERENCES products(product_id)
);

-- Create audit log table for demonstrating triggers
CREATE TABLE audit_log (
    log_id INT PRIMARY KEY AUTO_INCREMENT,
    table_name VARCHAR(50),
    operation VARCHAR(10),
    user_name VARCHAR(50),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    details TEXT
);

-- ========================================
-- 2. INSERT DATA
-- ========================================

-- Insert departments
INSERT INTO departments (dept_id, dept_name, location, budget) VALUES
(1, 'Engineering', 'San Francisco', 2000000.00),
(2, 'Sales', 'New York', 1500000.00),
(3, 'Marketing', 'Los Angeles', 1000000.00),
(4, 'HR', 'Chicago', 750000.00),
(5, 'Finance', 'Boston', 900000.00);

-- Insert employees
INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary, dept_id, manager_id) VALUES
(1, 'John', 'Smith', 'john.smith@company.com', '2020-01-15', 120000.00, 1, NULL),
(2, 'Jane', 'Doe', 'jane.doe@company.com', '2019-05-20', 150000.00, 1, 1),
(3, 'Bob', 'Johnson', 'bob.johnson@company.com', '2021-03-10', 95000.00, 2, 1),
(4, 'Alice', 'Williams', 'alice.williams@company.com', '2020-07-01', 105000.00, 2, 3),
(5, 'Charlie', 'Brown', 'charlie.brown@company.com', '2022-01-10', 85000.00, 3, 1),
(6, 'Diana', 'Davis', 'diana.davis@company.com', '2021-09-15', 90000.00, 3, 5),
(7, 'Eve', 'Wilson', 'eve.wilson@company.com', '2020-11-20', 110000.00, 4, 1),
(8, 'Frank', 'Miller', 'frank.miller@company.com', '2019-08-05', 125000.00, 5, 1),
(9, 'Grace', 'Taylor', 'grace.taylor@company.com', '2022-06-01', 75000.00, 1, 2),
(10, 'Henry', 'Anderson', 'henry.anderson@company.com', '2021-12-10', 98000.00, 2, 3);

-- Insert customers
INSERT INTO customers (company_name, contact_name, email, phone, address, city, country, credit_limit) VALUES
('Acme Corp', 'Tom Jones', 'tom@acme.com', '555-0100', '123 Main St', 'New York', 'USA', 50000.00),
('Global Tech', 'Sarah Connor', 'sarah@globaltech.com', '555-0101', '456 Tech Ave', 'San Francisco', 'USA', 75000.00),
('Euro Imports', 'Hans Mueller', 'hans@euroimports.de', '555-0102', '789 Euro St', 'Berlin', 'Germany', 60000.00),
('Asian Markets', 'Li Wang', 'li@asianmarkets.cn', '555-0103', '321 Market Rd', 'Shanghai', 'China', 45000.00),
('South Supplies', 'Maria Garcia', 'maria@southsupplies.mx', '555-0104', '654 Supply Blvd', 'Mexico City', 'Mexico', 40000.00);

-- Insert products
INSERT INTO products (product_name, category, unit_price, units_in_stock, reorder_level) VALUES
('Laptop Pro', 'Electronics', 1299.99, 50, 10),
('Wireless Mouse', 'Electronics', 29.99, 200, 50),
('Office Chair', 'Furniture', 299.99, 75, 20),
('Standing Desk', 'Furniture', 599.99, 30, 10),
('Monitor 27"', 'Electronics', 399.99, 100, 25),
('Keyboard Mechanical', 'Electronics', 89.99, 150, 40),
('Desk Lamp', 'Furniture', 49.99, 120, 30),
('USB Hub', 'Electronics', 39.99, 180, 50),
('Webcam HD', 'Electronics', 79.99, 90, 25),
('Headphones', 'Electronics', 149.99, 60, 15);

-- Insert orders
INSERT INTO orders (customer_id, employee_id, order_date, required_date, shipped_date, ship_via, freight, order_status) VALUES
(1, 3, '2024-01-15', '2024-01-20', '2024-01-16', 'FedEx', 15.99, 'SHIPPED'),
(2, 4, '2024-01-16', '2024-01-22', '2024-01-18', 'UPS', 22.50, 'SHIPPED'),
(1, 3, '2024-01-18', '2024-01-25', '2024-01-20', 'DHL', 18.75, 'SHIPPED'),
(3, 10, '2024-01-20', '2024-01-28', NULL, 'FedEx', 25.00, 'PENDING'),
(4, 4, '2024-01-22', '2024-01-30', NULL, 'UPS', 30.00, 'PROCESSING');

-- Insert order items
INSERT INTO order_items (order_id, product_id, quantity, unit_price, discount) VALUES
(1, 1, 2, 1299.99, 0.10),
(1, 2, 5, 29.99, 0.05),
(2, 3, 3, 299.99, 0.00),
(2, 5, 2, 399.99, 0.10),
(3, 6, 10, 89.99, 0.15),
(3, 7, 5, 49.99, 0.00),
(4, 1, 1, 1299.99, 0.05),
(4, 9, 3, 79.99, 0.00),
(5, 10, 4, 149.99, 0.10);

-- ========================================
-- 3. BASIC SELECT QUERIES
-- ========================================

-- Simple SELECT
SELECT * FROM departments;

-- SELECT with WHERE clause
SELECT first_name, last_name, salary
FROM employees
WHERE salary > 100000;

-- SELECT with ORDER BY
SELECT product_name, unit_price, units_in_stock
FROM products
ORDER BY unit_price DESC
LIMIT 5;

-- ========================================
-- 4. JOINS
-- ========================================

-- INNER JOIN
SELECT e.first_name, e.last_name, d.dept_name, e.salary
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id;

-- LEFT JOIN
SELECT c.company_name, o.order_id, o.order_date, o.order_status
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id
ORDER BY c.company_name;

-- Multiple JOINs
SELECT
    o.order_id,
    c.company_name,
    e.first_name AS employee_name,
    o.order_date,
    o.order_status
FROM orders o
INNER JOIN customers c ON o.customer_id = c.customer_id
INNER JOIN employees e ON o.employee_id = e.emp_id;

-- Self JOIN (employees and their managers)
SELECT
    e1.first_name AS employee,
    e1.last_name AS emp_last,
    e2.first_name AS manager,
    e2.last_name AS mgr_last
FROM employees e1
LEFT JOIN employees e2 ON e1.manager_id = e2.emp_id;

-- ========================================
-- 5. GROUP BY AND AGGREGATES
-- ========================================

-- COUNT by department
SELECT d.dept_name, COUNT(e.emp_id) AS employee_count
FROM departments d
LEFT JOIN employees e ON d.dept_id = e.dept_id
GROUP BY d.dept_id, d.dept_name;

-- SUM and AVG salary by department
SELECT
    d.dept_name,
    COUNT(e.emp_id) AS emp_count,
    SUM(e.salary) AS total_salary,
    AVG(e.salary) AS avg_salary,
    MIN(e.salary) AS min_salary,
    MAX(e.salary) AS max_salary
FROM departments d
INNER JOIN employees e ON d.dept_id = e.dept_id
GROUP BY d.dept_id, d.dept_name
HAVING AVG(e.salary) > 90000;

-- Order totals with items
SELECT
    o.order_id,
    COUNT(oi.product_id) AS item_count,
    SUM(oi.quantity) AS total_quantity,
    SUM(oi.quantity * oi.unit_price * (1 - oi.discount)) AS order_total
FROM orders o
INNER JOIN order_items oi ON o.order_id = oi.order_id
GROUP BY o.order_id;

-- Product sales analysis
SELECT
    p.category,
    COUNT(DISTINCT p.product_id) AS product_count,
    SUM(oi.quantity) AS units_sold,
    SUM(oi.quantity * oi.unit_price * (1 - oi.discount)) AS revenue
FROM products p
LEFT JOIN order_items oi ON p.product_id = oi.product_id
GROUP BY p.category
ORDER BY revenue DESC;

-- ========================================
-- 6. SUBQUERIES
-- ========================================

-- Employees earning more than department average
SELECT e.first_name, e.last_name, e.salary, e.dept_id
FROM employees e
WHERE e.salary > (
    SELECT AVG(e2.salary)
    FROM employees e2
    WHERE e2.dept_id = e.dept_id
);

-- Products never ordered
SELECT product_name
FROM products p
WHERE NOT EXISTS (
    SELECT 1
    FROM order_items oi
    WHERE oi.product_id = p.product_id
);

-- Customers with orders above average
SELECT c.company_name,
       (SELECT COUNT(*) FROM orders o WHERE o.customer_id = c.customer_id) AS order_count
FROM customers c
WHERE c.customer_id IN (
    SELECT customer_id
    FROM orders o
    INNER JOIN order_items oi ON o.order_id = oi.order_id
    GROUP BY customer_id
    HAVING SUM(oi.quantity * oi.unit_price) > (
        SELECT AVG(order_total)
        FROM (
            SELECT SUM(quantity * unit_price) AS order_total
            FROM order_items
            GROUP BY order_id
        ) AS avg_orders
    )
);

-- ========================================
-- 7. UPDATES AND DELETES
-- ========================================

-- UPDATE single row
UPDATE employees
SET salary = salary * 1.10
WHERE emp_id = 9;

-- UPDATE with JOIN
UPDATE products p
SET p.units_in_stock = p.units_in_stock - oi.quantity
FROM order_items oi
WHERE p.product_id = oi.product_id
  AND oi.order_id = 1;

-- DELETE with condition
DELETE FROM audit_log
WHERE timestamp < '2024-01-01';

-- ========================================
-- 8. INDEXES
-- ========================================

-- Create indexes for performance
CREATE INDEX idx_emp_salary ON employees(salary);
CREATE INDEX idx_emp_dept ON employees(dept_id);
CREATE INDEX idx_order_customer ON orders(customer_id);
CREATE INDEX idx_order_date ON orders(order_date);
CREATE UNIQUE INDEX idx_emp_email ON employees(email);

-- ========================================
-- 9. TRANSACTIONS
-- ========================================

-- Start transaction
BEGIN TRANSACTION;

-- Insert new order
INSERT INTO orders (customer_id, employee_id, order_date, required_date, order_status)
VALUES (2, 4, '2024-01-25', '2024-02-01', 'PENDING');

-- Get the last inserted order_id (assuming it's 6)
INSERT INTO order_items (order_id, product_id, quantity, unit_price, discount)
VALUES (6, 1, 1, 1299.99, 0.05);

-- Update product stock
UPDATE products
SET units_in_stock = units_in_stock - 1
WHERE product_id = 1;

-- Commit transaction
COMMIT;

-- Rollback example
BEGIN TRANSACTION;

UPDATE employees SET salary = salary * 1.20;
-- Oops, that's too much!
ROLLBACK;

-- ========================================
-- 10. VIEWS
-- ========================================

-- Create view for employee details
CREATE VIEW employee_details AS
SELECT
    e.emp_id,
    e.first_name,
    e.last_name,
    e.email,
    d.dept_name,
    e.salary,
    m.first_name AS manager_name
FROM employees e
LEFT JOIN departments d ON e.dept_id = d.dept_id
LEFT JOIN employees m ON e.manager_id = m.emp_id;

-- Use the view
SELECT * FROM employee_details WHERE salary > 100000;

-- ========================================
-- 11. COMMON TABLE EXPRESSIONS (CTEs)
-- ========================================

-- Recursive CTE for employee hierarchy
WITH RECURSIVE employee_hierarchy AS (
    -- Anchor member: top-level employees (no manager)
    SELECT
        emp_id,
        first_name,
        last_name,
        manager_id,
        0 AS level
    FROM employees
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive member
    SELECT
        e.emp_id,
        e.first_name,
        e.last_name,
        e.manager_id,
        eh.level + 1
    FROM employees e
    INNER JOIN employee_hierarchy eh ON e.manager_id = eh.emp_id
)
SELECT * FROM employee_hierarchy ORDER BY level, emp_id;

-- CTE for sales analysis
WITH monthly_sales AS (
    SELECT
        EXTRACT(MONTH FROM o.order_date) AS month,
        SUM(oi.quantity * oi.unit_price * (1 - oi.discount)) AS total_sales
    FROM orders o
    INNER JOIN order_items oi ON o.order_id = oi.order_id
    WHERE o.order_date >= '2024-01-01'
    GROUP BY EXTRACT(MONTH FROM o.order_date)
)
SELECT
    month,
    total_sales,
    total_sales / (SELECT SUM(total_sales) FROM monthly_sales) * 100 AS percentage
FROM monthly_sales;

-- ========================================
-- 12. WINDOW FUNCTIONS
-- ========================================

-- Rank employees by salary within departments
SELECT
    first_name,
    last_name,
    dept_id,
    salary,
    RANK() OVER (PARTITION BY dept_id ORDER BY salary DESC) AS salary_rank,
    DENSE_RANK() OVER (PARTITION BY dept_id ORDER BY salary DESC) AS dense_rank,
    ROW_NUMBER() OVER (PARTITION BY dept_id ORDER BY salary DESC) AS row_num
FROM employees;

-- Running totals
SELECT
    order_id,
    order_date,
    SUM(freight) OVER (ORDER BY order_date) AS running_total_freight,
    AVG(freight) OVER (ORDER BY order_date ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) AS moving_avg
FROM orders;

-- ========================================
-- 13. TIME TRAVEL QUERIES (LatticeDB Feature)
-- ========================================

-- Query data as of a specific timestamp
SELECT * FROM employees
FOR SYSTEM_TIME AS OF '2024-01-01 00:00:00';

-- See historical changes
SELECT * FROM products
FOR SYSTEM_TIME BETWEEN '2024-01-01' AND '2024-01-15';

-- ========================================
-- 14. VECTOR OPERATIONS (LatticeDB Feature)
-- ========================================

-- Create table with vector column
CREATE TABLE documents (
    doc_id INT PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    embedding VECTOR(768)  -- 768-dimensional vector for embeddings
);

-- Insert documents with embeddings
INSERT INTO documents (doc_id, title, content, embedding) VALUES
(1, 'Database Systems', 'Introduction to RDBMS', '[0.1, 0.2, ...]'),
(2, 'Machine Learning', 'Neural networks and AI', '[0.3, 0.4, ...]'),
(3, 'Data Science', 'Statistical analysis', '[0.5, 0.6, ...]');

-- Vector similarity search (cosine similarity)
SELECT doc_id, title,
       VECTOR_SIMILARITY(embedding, '[0.2, 0.3, ...]') AS similarity
FROM documents
ORDER BY similarity DESC
LIMIT 5;

-- K-nearest neighbors search
SELECT doc_id, title
FROM documents
WHERE VECTOR_KNN(embedding, '[0.2, 0.3, ...]', 10);

-- ========================================
-- 15. SECURITY FEATURES
-- ========================================

-- Create users and roles
CREATE USER 'analyst' IDENTIFIED BY 'password123';
CREATE ROLE 'data_analyst';

-- Grant privileges
GRANT SELECT ON employees TO 'data_analyst';
GRANT SELECT, INSERT ON orders TO 'data_analyst';
GRANT 'data_analyst' TO 'analyst';

-- Row-level security
CREATE POLICY emp_salary_policy ON employees
FOR SELECT
USING (salary < 150000 OR current_user = 'admin');

-- Column-level encryption
ALTER TABLE employees
MODIFY COLUMN salary ENCRYPTED;

-- ========================================
-- 16. PERFORMANCE MONITORING
-- ========================================

-- Show query execution plan
EXPLAIN SELECT e.*, d.dept_name
FROM employees e
INNER JOIN departments d ON e.dept_id = d.dept_id
WHERE e.salary > 100000;

-- Analyze query performance
EXPLAIN ANALYZE SELECT * FROM orders WHERE order_date > '2024-01-15';

-- Show table statistics
SHOW STATS FOR employees;

-- ========================================
-- 17. STORED PROCEDURES (Example)
-- ========================================

-- Create stored procedure
CREATE PROCEDURE increase_salary(
    IN emp_id_param INT,
    IN increase_percent DECIMAL(5,2)
)
BEGIN
    UPDATE employees
    SET salary = salary * (1 + increase_percent / 100)
    WHERE emp_id = emp_id_param;
END;

-- Call stored procedure
CALL increase_salary(5, 10.0);

-- ========================================
-- 18. TRIGGERS (Example)
-- ========================================

-- Create audit trigger
CREATE TRIGGER audit_employee_changes
AFTER UPDATE ON employees
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, operation, user_name, details)
    VALUES ('employees', 'UPDATE', USER(),
            CONCAT('Updated emp_id: ', NEW.emp_id));
END;

-- ========================================
-- 19. CLEANUP
-- ========================================

-- Drop views
DROP VIEW IF EXISTS employee_details;

-- Drop indexes (optional)
DROP INDEX idx_emp_salary;
DROP INDEX idx_emp_dept;

-- Show final statistics
SELECT 'Total Employees:' AS metric, COUNT(*) AS value FROM employees
UNION ALL
SELECT 'Total Orders:', COUNT(*) FROM orders
UNION ALL
SELECT 'Total Products:', COUNT(*) FROM products
UNION ALL
SELECT 'Total Customers:', COUNT(*) FROM customers;

-- ========================================
-- END OF DEMONSTRATION
-- ========================================