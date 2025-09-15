-- Test: Streaming and Materialized Views
-- Tests streaming data processing and real-time materialized views

-- Test 1: Create Stream Table
CREATE STREAM events_stream (
    event_id SERIAL,
    user_id INT,
    event_type TEXT,
    event_data JSONB,
    event_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) WITH (retention_period = '7 days');

-- Test 2: Insert Streaming Data
INSERT INTO events_stream (user_id, event_type, event_data) VALUES
    (1, 'page_view', '{"page": "/home", "duration": 45}'),
    (2, 'click', '{"element": "buy_button", "product_id": 101}'),
    (1, 'purchase', '{"product_id": 101, "amount": 99.99}'),
    (3, 'page_view', '{"page": "/products", "duration": 120}'),
    (2, 'cart_add', '{"product_id": 102, "quantity": 2}');

-- Test 3: Simple Materialized View
CREATE MATERIALIZED VIEW user_activity_summary AS
SELECT
    user_id,
    COUNT(*) as total_events,
    COUNT(DISTINCT event_type) as unique_event_types,
    MAX(event_time) as last_activity
FROM events_stream
GROUP BY user_id
WITH (refresh_interval = '1 minute');

SELECT * FROM user_activity_summary ORDER BY user_id;

-- Test 4: Windowed Aggregation
CREATE MATERIALIZED VIEW hourly_events AS
SELECT
    DATE_TRUNC('hour', event_time) as hour,
    event_type,
    COUNT(*) as event_count,
    COUNT(DISTINCT user_id) as unique_users
FROM events_stream
GROUP BY DATE_TRUNC('hour', event_time), event_type
WITH (refresh_interval = '5 minutes');

-- Test 5: Tumbling Window
CREATE MATERIALIZED VIEW events_5min_window AS
SELECT
    TUMBLE_START(event_time, INTERVAL '5 minutes') as window_start,
    TUMBLE_END(event_time, INTERVAL '5 minutes') as window_end,
    COUNT(*) as event_count,
    ARRAY_AGG(DISTINCT event_type) as event_types
FROM events_stream
GROUP BY TUMBLE(event_time, INTERVAL '5 minutes');

-- Test 6: Hopping Window
CREATE MATERIALIZED VIEW events_hopping AS
SELECT
    HOP_START(event_time, INTERVAL '2 minutes', INTERVAL '5 minutes') as window_start,
    HOP_END(event_time, INTERVAL '2 minutes', INTERVAL '5 minutes') as window_end,
    user_id,
    COUNT(*) as events_in_window
FROM events_stream
GROUP BY HOP(event_time, INTERVAL '2 minutes', INTERVAL '5 minutes'), user_id;

-- Test 7: Session Window
CREATE MATERIALIZED VIEW user_sessions AS
SELECT
    user_id,
    SESSION_START(event_time, INTERVAL '30 minutes') as session_start,
    SESSION_END(event_time, INTERVAL '30 minutes') as session_end,
    COUNT(*) as events_per_session,
    ARRAY_AGG(event_type ORDER BY event_time) as event_sequence
FROM events_stream
GROUP BY user_id, SESSION(event_time, INTERVAL '30 minutes');

-- Test 8: Stream-Table Join
CREATE TABLE users (
    user_id INT PRIMARY KEY,
    username TEXT,
    email TEXT,
    created_at TIMESTAMP
);

INSERT INTO users VALUES
    (1, 'alice', 'alice@example.com', '2024-01-01'),
    (2, 'bob', 'bob@example.com', '2024-01-15'),
    (3, 'charlie', 'charlie@example.com', '2024-02-01');

CREATE MATERIALIZED VIEW enriched_events AS
SELECT
    e.event_id,
    e.event_time,
    e.event_type,
    u.username,
    u.email,
    e.event_data
FROM events_stream e
JOIN users u ON e.user_id = u.user_id
WITH (refresh_interval = '30 seconds');

-- Test 9: Complex Event Processing (CEP)
CREATE MATERIALIZED VIEW purchase_patterns AS
WITH event_sequences AS (
    SELECT
        user_id,
        event_type,
        event_time,
        LAG(event_type, 1) OVER (PARTITION BY user_id ORDER BY event_time) as prev_event,
        LAG(event_time, 1) OVER (PARTITION BY user_id ORDER BY event_time) as prev_time
    FROM events_stream
)
SELECT
    user_id,
    prev_event || ' -> ' || event_type as pattern,
    COUNT(*) as pattern_count,
    AVG(EXTRACT(EPOCH FROM (event_time - prev_time))) as avg_time_between
FROM event_sequences
WHERE prev_event IS NOT NULL
GROUP BY user_id, pattern
HAVING COUNT(*) > 1;

-- Test 10: Real-time Alerts
CREATE MATERIALIZED VIEW high_activity_alerts AS
SELECT
    user_id,
    COUNT(*) as event_count,
    'HIGH_ACTIVITY' as alert_type,
    CURRENT_TIMESTAMP as alert_time
FROM events_stream
WHERE event_time > CURRENT_TIMESTAMP - INTERVAL '5 minutes'
GROUP BY user_id
HAVING COUNT(*) > 10
WITH (refresh_interval = '10 seconds');

-- Test 11: Stream Deduplication
CREATE MATERIALIZED VIEW deduplicated_events AS
SELECT DISTINCT ON (user_id, event_type, DATE_TRUNC('minute', event_time))
    event_id,
    user_id,
    event_type,
    event_time,
    event_data
FROM events_stream
ORDER BY user_id, event_type, DATE_TRUNC('minute', event_time), event_time DESC;

-- Test 12: Incremental Materialized View
CREATE MATERIALIZED VIEW revenue_tracker
WITH (incremental = true) AS
SELECT
    DATE(event_time) as date,
    SUM((event_data->>'amount')::DECIMAL) as daily_revenue,
    COUNT(*) as transaction_count
FROM events_stream
WHERE event_type = 'purchase'
GROUP BY DATE(event_time);

-- Manual refresh
REFRESH MATERIALIZED VIEW revenue_tracker;

-- Test 13: Stream Fork (Multiple Consumers)
CREATE STREAM events_analytics AS
SELECT * FROM events_stream
WHERE event_type IN ('purchase', 'cart_add');

CREATE STREAM events_monitoring AS
SELECT * FROM events_stream
WHERE event_data->>'error' IS NOT NULL;

-- Test 14: Watermark and Late Data Handling
CREATE MATERIALIZED VIEW events_with_watermark AS
SELECT
    *,
    event_time - INTERVAL '1 minute' as watermark
FROM events_stream
WHERE event_time > (
    SELECT MAX(event_time) - INTERVAL '5 minutes'
    FROM events_stream
)
WITH (handle_late_data = 'include', late_data_threshold = '2 minutes');

-- Test 15: Stream State Management
CREATE TABLE stream_checkpoints (
    view_name TEXT PRIMARY KEY,
    last_processed_time TIMESTAMP,
    processed_count BIGINT,
    checkpoint_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Simulate checkpoint
INSERT INTO stream_checkpoints
SELECT
    'user_activity_summary' as view_name,
    MAX(event_time) as last_processed_time,
    COUNT(*) as processed_count
FROM events_stream;

-- Cleanup
DROP MATERIALIZED VIEW IF EXISTS user_activity_summary CASCADE;
DROP MATERIALIZED VIEW IF EXISTS hourly_events CASCADE;
DROP MATERIALIZED VIEW IF EXISTS events_5min_window CASCADE;
DROP MATERIALIZED VIEW IF EXISTS events_hopping CASCADE;
DROP MATERIALIZED VIEW IF EXISTS user_sessions CASCADE;
DROP MATERIALIZED VIEW IF EXISTS enriched_events CASCADE;
DROP MATERIALIZED VIEW IF EXISTS purchase_patterns CASCADE;
DROP MATERIALIZED VIEW IF EXISTS high_activity_alerts CASCADE;
DROP MATERIALIZED VIEW IF EXISTS deduplicated_events CASCADE;
DROP MATERIALIZED VIEW IF EXISTS revenue_tracker CASCADE;
DROP MATERIALIZED VIEW IF EXISTS events_with_watermark CASCADE;
DROP STREAM IF EXISTS events_stream CASCADE;
DROP STREAM IF EXISTS events_analytics CASCADE;
DROP STREAM IF EXISTS events_monitoring CASCADE;
DROP TABLE IF EXISTS users CASCADE;
DROP TABLE IF EXISTS stream_checkpoints CASCADE;
EXIT;