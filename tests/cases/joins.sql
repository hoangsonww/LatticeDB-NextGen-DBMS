-- Test: Complex Join Operations
-- Tests various join types, conditions, and multi-table joins

-- Test 1: Inner Join
SELECT u.name, o.id as order_id, o.total_amount
FROM users u
INNER JOIN orders o ON u.id = o.user_id
WHERE o.status = 'completed'
ORDER BY o.total_amount DESC
LIMIT 10;

-- Test 2: Left Outer Join
SELECT u.id, u.name, COUNT(o.id) as order_count
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
GROUP BY u.id, u.name
HAVING COUNT(o.id) = 0
ORDER BY u.id;

-- Test 3: Right Outer Join
SELECT o.id as order_id, o.status, u.name as user_name
FROM orders o
RIGHT JOIN users u ON o.user_id = u.id
WHERE u.age > 30
ORDER BY u.id, o.id;

-- Test 4: Full Outer Join
SELECT
    COALESCE(u.id, -1) as user_id,
    COALESCE(u.name, 'No User') as user_name,
    COALESCE(o.id, -1) as order_id,
    COALESCE(o.status, 'No Order') as order_status
FROM users u
FULL OUTER JOIN orders o ON u.id = o.user_id
WHERE u.age < 25 OR o.total_amount > 500
ORDER BY user_id, order_id;

-- Test 5: Cross Join (Cartesian Product)
SELECT u.name, p.name as product_name, p.price
FROM users u
CROSS JOIN products p
WHERE u.age BETWEEN 25 AND 30
  AND p.price < 50
LIMIT 20;

-- Test 6: Self Join
SELECT
    u1.name as user_name,
    u2.name as similar_age_user,
    u1.age
FROM users u1
INNER JOIN users u2 ON u1.age = u2.age AND u1.id < u2.id
ORDER BY u1.age, u1.id, u2.id
LIMIT 15;

-- Test 7: Multi-Table Join (3+ tables)
SELECT
    u.name as customer,
    o.id as order_id,
    p.name as product,
    o.quantity,
    o.total_amount
FROM users u
INNER JOIN orders o ON u.id = o.user_id
INNER JOIN products p ON o.product_id = p.id
WHERE o.status = 'completed'
  AND p.price > 100
ORDER BY o.total_amount DESC
LIMIT 10;

-- Test 8: Join with Subquery
SELECT u.name, u.email, order_summary.total_spent
FROM users u
INNER JOIN (
    SELECT user_id, SUM(total_amount) as total_spent
    FROM orders
    WHERE status = 'completed'
    GROUP BY user_id
    HAVING SUM(total_amount) > 1000
) order_summary ON u.id = order_summary.user_id
ORDER BY order_summary.total_spent DESC;

-- Test 9: Join with Multiple Conditions
SELECT
    o.id as order_id,
    u.name as user_name,
    p.name as product_name,
    o.order_date
FROM orders o
INNER JOIN users u ON o.user_id = u.id
                   AND u.created_at < o.order_date
INNER JOIN products p ON o.product_id = p.id
                      AND p.price > 50
WHERE o.status IN ('pending', 'processing')
ORDER BY o.order_date DESC
LIMIT 20;

-- Test 10: Natural Join
SELECT *
FROM users
NATURAL JOIN orders
WHERE age > 25
LIMIT 10;

-- Test 11: Semi Join (EXISTS)
SELECT u.id, u.name, u.email
FROM users u
WHERE EXISTS (
    SELECT 1
    FROM orders o
    WHERE o.user_id = u.id
      AND o.total_amount > 200
      AND o.status = 'completed'
)
ORDER BY u.id;

-- Test 12: Anti Join (NOT EXISTS)
SELECT u.id, u.name, u.email
FROM users u
WHERE NOT EXISTS (
    SELECT 1
    FROM orders o
    WHERE o.user_id = u.id
      AND o.status = 'cancelled'
)
ORDER BY u.id;

-- Test 13: Join with USING clause
SELECT u.name, o.id as order_id, o.total_amount
FROM users u
JOIN orders o USING (id)
WHERE u.age >= 30
ORDER BY o.total_amount DESC
LIMIT 10;

-- Test 14: Lateral Join
SELECT u.name, recent_orders.order_id, recent_orders.order_date
FROM users u,
LATERAL (
    SELECT id as order_id, order_date
    FROM orders
    WHERE user_id = u.id
    ORDER BY order_date DESC
    LIMIT 3
) recent_orders
WHERE u.age > 25
ORDER BY u.id, recent_orders.order_date DESC;

-- Test 15: Join with Window Functions
SELECT
    u.name,
    o.id as order_id,
    o.total_amount,
    ROW_NUMBER() OVER (PARTITION BY u.id ORDER BY o.total_amount DESC) as order_rank,
    SUM(o.total_amount) OVER (PARTITION BY u.id) as user_total
FROM users u
INNER JOIN orders o ON u.id = o.user_id
WHERE o.status = 'completed'
ORDER BY u.id, order_rank;