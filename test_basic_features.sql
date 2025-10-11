-- Create simple tables
CREATE TABLE users (id INTEGER, name VARCHAR(50), age INTEGER);
CREATE TABLE orders (id INTEGER, user_id INTEGER, amount DOUBLE);

-- Insert data
INSERT INTO users VALUES (1, 'Alice', 25);
INSERT INTO users VALUES (2, 'Bob', 30);
INSERT INTO users VALUES (3, 'Charlie', 35);

INSERT INTO orders VALUES (1, 1, 100.50);
INSERT INTO orders VALUES (2, 1, 250.00);
INSERT INTO orders VALUES (3, 2, 175.25);
INSERT INTO orders VALUES (4, 2, 300.00);
INSERT INTO orders VALUES (5, 3, 500.00);

-- Test basic SELECT
SELECT * FROM users;
SELECT * FROM orders;

-- Test WHERE clause
SELECT * FROM users WHERE age > 28;
SELECT * FROM orders WHERE amount > 200;

-- Test aggregate functions
SELECT COUNT(*) FROM users;
SELECT COUNT(*) FROM orders;
SELECT MIN(age), MAX(age) FROM users;
SELECT SUM(amount) FROM orders;
SELECT AVG(amount) FROM orders;

-- Test GROUP BY
SELECT user_id, COUNT(*) FROM orders GROUP BY user_id;
SELECT user_id, SUM(amount) FROM orders GROUP BY user_id;

-- Clean up
DROP TABLE orders;
DROP TABLE users;