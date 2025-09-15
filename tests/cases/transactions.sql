-- Test: Transaction Isolation and ACID Properties
-- Tests concurrent transactions, isolation levels, and rollback scenarios

-- Test 1: Basic Transaction Commit
BEGIN;
INSERT INTO users (id, name, email, age) VALUES (1001, 'Transaction Test', 'tx@test.com', 25);
SELECT COUNT(*) as count FROM users WHERE id = 1001;
COMMIT;
-- Verify commit persisted
SELECT * FROM users WHERE id = 1001;

-- Test 2: Transaction Rollback
BEGIN;
INSERT INTO users (id, name, email, age) VALUES (1002, 'Rollback Test', 'rollback@test.com', 30);
SELECT COUNT(*) as before_rollback FROM users WHERE id = 1002;
ROLLBACK;
-- Verify rollback worked
SELECT COUNT(*) as after_rollback FROM users WHERE id = 1002;

-- Test 3: Savepoints
BEGIN;
INSERT INTO orders (id, user_id, product_id, quantity, total_amount, status)
VALUES (5001, 1, 1, 2, 199.98, 'pending');
SAVEPOINT sp1;
UPDATE orders SET status = 'processing' WHERE id = 5001;
SAVEPOINT sp2;
UPDATE orders SET status = 'completed' WHERE id = 5001;
-- Rollback to second savepoint
ROLLBACK TO SAVEPOINT sp2;
SELECT status FROM orders WHERE id = 5001;
COMMIT;

-- Test 4: Isolation Level - READ COMMITTED
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
BEGIN;
SELECT COUNT(*) as initial_count FROM products WHERE price > 100;
-- Simulate concurrent insert (would be from another transaction)
-- INSERT INTO products (id, name, price) VALUES (901, 'Concurrent Product', 150);
SELECT COUNT(*) as final_count FROM products WHERE price > 100;
COMMIT;

-- Test 5: Isolation Level - REPEATABLE READ
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
BEGIN;
SELECT * FROM users WHERE age BETWEEN 25 AND 35 ORDER BY id LIMIT 5;
-- Simulate concurrent update (would be from another transaction)
-- UPDATE users SET age = 36 WHERE id = 1;
SELECT * FROM users WHERE age BETWEEN 25 AND 35 ORDER BY id LIMIT 5;
COMMIT;

-- Test 6: Isolation Level - SERIALIZABLE
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
BEGIN;
SELECT SUM(total_amount) as total_revenue FROM orders WHERE status = 'completed';
UPDATE orders SET total_amount = total_amount * 1.1 WHERE status = 'completed';
SELECT SUM(total_amount) as new_revenue FROM orders WHERE status = 'completed';
COMMIT;

-- Test 7: Deadlock Detection
-- Transaction 1 (simulated)
BEGIN;
UPDATE users SET credits = credits + 10 WHERE id = 1;
-- Waiting for lock on orders...
UPDATE orders SET status = 'shipped' WHERE user_id = 2;
COMMIT;

-- Transaction 2 (simulated)
BEGIN;
UPDATE orders SET status = 'shipped' WHERE user_id = 2;
-- Waiting for lock on users... DEADLOCK!
UPDATE users SET credits = credits + 10 WHERE id = 1;
COMMIT;

-- Test 8: Nested Transactions
BEGIN;
INSERT INTO analytics_events (id, user_id, event_type, timestamp, properties)
VALUES (10001, 1, 'transaction_start', CURRENT_TIMESTAMP, '{"nested": false}');
  BEGIN; -- Nested transaction
  INSERT INTO analytics_events (id, user_id, event_type, timestamp, properties)
  VALUES (10002, 1, 'nested_transaction', CURRENT_TIMESTAMP, '{"nested": true}');
  COMMIT; -- Commit nested
COMMIT; -- Commit outer
SELECT COUNT(*) as nested_events FROM analytics_events WHERE id IN (10001, 10002);

-- Test 9: Transaction with Multiple Tables
BEGIN;
-- Insert new user
INSERT INTO users (id, name, email, age) VALUES (2001, 'Multi Table', 'multi@test.com', 28);
-- Create order for new user
INSERT INTO orders (id, user_id, product_id, quantity, total_amount, status)
VALUES (6001, 2001, 1, 1, 99.99, 'pending');
-- Log the event
INSERT INTO analytics_events (id, user_id, event_type, timestamp, properties)
VALUES (10003, 2001, 'new_order', CURRENT_TIMESTAMP, '{"order_id": 6001}');
-- Verify all inserts
SELECT 'users' as table_name, COUNT(*) as count FROM users WHERE id = 2001
UNION ALL
SELECT 'orders', COUNT(*) FROM orders WHERE id = 6001
UNION ALL
SELECT 'events', COUNT(*) FROM analytics_events WHERE id = 10003;
COMMIT;

-- Test 10: Read-Only Transactions
BEGIN READ ONLY;
SELECT COUNT(*) as user_count FROM users;
SELECT COUNT(*) as order_count FROM orders;
-- This should fail in read-only transaction
INSERT INTO users (id, name, email, age) VALUES (3001, 'ReadOnly Test', 'readonly@test.com', 25);
COMMIT;