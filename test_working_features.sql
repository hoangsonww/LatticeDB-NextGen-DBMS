-- Create table with primary key
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    price DOUBLE,
    category VARCHAR(50)
);

-- Insert data
INSERT INTO products VALUES (1, 'Laptop', 999.99, 'Electronics');
INSERT INTO products VALUES (2, 'Phone', 599.99, 'Electronics');
INSERT INTO products VALUES (3, 'Desk', 299.99, 'Furniture');
INSERT INTO products VALUES (4, 'Chair', 199.99, 'Furniture');
INSERT INTO products VALUES (5, 'Monitor', 399.99, 'Electronics');

-- Basic queries
SELECT * FROM products;

-- Aggregate functions work
SELECT COUNT(*) FROM products;
SELECT MIN(price) FROM products;
SELECT MAX(price) FROM products;
SELECT AVG(price) FROM products;
SELECT SUM(price) FROM products;

-- GROUP BY with SUM works
SELECT category, SUM(price) FROM products GROUP BY category;

-- Clean up
DROP TABLE products;