-- Test: Index Operations and Performance
-- Tests index creation, usage, and query optimization

-- Test 1: Create Basic B-Tree Index
CREATE INDEX idx_users_age ON users(age);
CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_orders_status ON orders(status);
CREATE INDEX idx_orders_user_id ON orders(user_id);

-- Test 2: Create Unique Index
CREATE UNIQUE INDEX idx_users_email_unique ON users(email);
-- This should fail due to unique constraint
INSERT INTO users (id, name, email, age) VALUES (9001, 'Duplicate Email', 'alice@example.com', 30);

-- Test 3: Create Composite Index
CREATE INDEX idx_orders_composite ON orders(user_id, status, order_date);
-- Query that should use composite index
SELECT * FROM orders
WHERE user_id = 1 AND status = 'completed' AND order_date > '2024-01-01'
ORDER BY order_date;

-- Test 4: Create Partial Index
CREATE INDEX idx_orders_pending ON orders(id, user_id, total_amount)
WHERE status = 'pending';
-- Query that should use partial index
SELECT * FROM orders WHERE status = 'pending' AND total_amount > 100;

-- Test 5: Create Expression Index
CREATE INDEX idx_users_lower_email ON users(LOWER(email));
-- Query using expression index
SELECT * FROM users WHERE LOWER(email) = 'alice@example.com';

-- Test 6: Index-Only Scan
-- This query should be satisfied entirely from index
SELECT email FROM users WHERE age BETWEEN 25 AND 35 ORDER BY email;

-- Test 7: Index Scan vs Sequential Scan
-- Small result set - should use index
EXPLAIN SELECT * FROM users WHERE age = 30;
SELECT * FROM users WHERE age = 30;

-- Large result set - might use sequential scan
EXPLAIN SELECT * FROM users WHERE age > 20;
SELECT COUNT(*) FROM users WHERE age > 20;

-- Test 8: Index on Vector Column
CREATE INDEX idx_users_vector ON users USING vector_cosine(profile_vector);
-- Vector similarity search using index
SELECT id, name FROM users
WHERE VECTOR_DISTANCE(profile_vector, [0.1, 0.2, 0.3, 0.4]) < 0.5
ORDER BY VECTOR_DISTANCE(profile_vector, [0.1, 0.2, 0.3, 0.4])
LIMIT 10;

-- Test 9: Drop and Recreate Index
DROP INDEX idx_users_age;
-- Query without index (should be slower)
SELECT COUNT(*) FROM users WHERE age BETWEEN 25 AND 35;
-- Recreate index
CREATE INDEX idx_users_age ON users(age);
-- Same query with index (should be faster)
SELECT COUNT(*) FROM users WHERE age BETWEEN 25 AND 35;

-- Test 10: Index Statistics
ANALYZE users;
ANALYZE orders;
-- View index statistics
SELECT
    tablename,
    indexname,
    idx_scan,
    idx_tup_read,
    idx_tup_fetch
FROM pg_stat_user_indexes
WHERE schemaname = 'public'
ORDER BY idx_scan DESC;

-- Test 11: Covering Index
CREATE INDEX idx_orders_covering ON orders(user_id) INCLUDE (product_id, total_amount, status);
-- Query that can be satisfied from covering index
SELECT user_id, product_id, total_amount, status
FROM orders
WHERE user_id IN (1, 2, 3, 4, 5)
ORDER BY user_id;

-- Test 12: Bitmap Index Scan
-- Create indexes for bitmap operations
CREATE INDEX idx_orders_status_bitmap ON orders(status);
CREATE INDEX idx_orders_amount_bitmap ON orders(total_amount);
-- Query combining multiple conditions (bitmap AND)
SELECT * FROM orders
WHERE status = 'completed' AND total_amount > 500;

-- Test 13: Index Maintenance
-- Check index size before operations
SELECT
    indexname,
    pg_size_pretty(pg_relation_size(indexname::regclass)) as index_size
FROM pg_indexes
WHERE tablename = 'orders';

-- Perform bulk insert
INSERT INTO orders (id, user_id, product_id, quantity, total_amount, status, order_date)
SELECT
    10000 + generate_series,
    (generate_series % 100) + 1,
    (generate_series % 50) + 1,
    (generate_series % 5) + 1,
    (random() * 1000)::numeric(10,2),
    CASE (generate_series % 4)
        WHEN 0 THEN 'pending'
        WHEN 1 THEN 'processing'
        WHEN 2 THEN 'completed'
        ELSE 'cancelled'
    END,
    CURRENT_DATE - (generate_series % 365)
FROM generate_series(1, 1000);

-- Check index size after bulk insert
SELECT
    indexname,
    pg_size_pretty(pg_relation_size(indexname::regclass)) as index_size
FROM pg_indexes
WHERE tablename = 'orders';

-- Test 14: Index Rebuild
REINDEX TABLE users;
REINDEX TABLE orders;
REINDEX INDEX idx_orders_composite;

-- Test 15: Functional Index Performance Test
-- Create function-based index
CREATE INDEX idx_users_age_group ON users(
    CASE
        WHEN age < 25 THEN 'young'
        WHEN age < 40 THEN 'adult'
        ELSE 'senior'
    END
);

-- Query using functional index
SELECT
    CASE
        WHEN age < 25 THEN 'young'
        WHEN age < 40 THEN 'adult'
        ELSE 'senior'
    END as age_group,
    COUNT(*) as count
FROM users
GROUP BY age_group
ORDER BY count DESC;