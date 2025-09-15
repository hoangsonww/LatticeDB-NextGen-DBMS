-- Test: Advanced CRDT Merge Operations
-- Tests complex conflict resolution and merge semantics

-- Test 1: Last-Writer-Wins (LWW) Register
CREATE TABLE lww_test (
    id TEXT PRIMARY KEY,
    value TEXT MERGE lww,
    updated_at TIMESTAMP MERGE lww
);

-- Simulate concurrent writes
INSERT INTO lww_test VALUES ('item1', 'version_a', '2024-01-01 10:00:00');
INSERT INTO lww_test VALUES ('item1', 'version_b', '2024-01-01 10:00:01') ON CONFLICT MERGE;
INSERT INTO lww_test VALUES ('item1', 'version_c', '2024-01-01 09:59:59') ON CONFLICT MERGE;
SELECT * FROM lww_test; -- Should show version_b (latest timestamp wins)

-- Test 2: Grow-Only Set (G-Set)
CREATE TABLE gset_test (
    id TEXT PRIMARY KEY,
    tags SET<TEXT> MERGE gset,
    followers SET<INT> MERGE gset
);

INSERT INTO gset_test VALUES ('user1', {'tech', 'ai'}, {100, 200});
INSERT INTO gset_test VALUES ('user1', {'ml', 'tech'}, {200, 300}) ON CONFLICT MERGE;
INSERT INTO gset_test VALUES ('user1', {'data'}, {400}) ON CONFLICT MERGE;
SELECT * FROM gset_test; -- Should show union of all sets

-- Test 3: Two-Phase Set (2P-Set) - Add and Remove
CREATE TABLE twoset_test (
    id TEXT PRIMARY KEY,
    active_items SET<TEXT> MERGE gset,
    removed_items SET<TEXT> MERGE gset
);

INSERT INTO twoset_test VALUES ('cart1', {'item_a', 'item_b'}, {});
INSERT INTO twoset_test VALUES ('cart1', {'item_c'}, {'item_a'}) ON CONFLICT MERGE;
-- Computed active set would be active_items - removed_items
SELECT id, active_items, removed_items,
       array_agg(elem) FILTER (WHERE elem NOT IN (SELECT unnest(removed_items))) as final_items
FROM twoset_test, unnest(active_items) as elem
GROUP BY id, active_items, removed_items;

-- Test 4: Counter CRDTs
CREATE TABLE counter_test (
    id TEXT PRIMARY KEY,
    views INT MERGE sum,
    likes INT MERGE sum_bounded(0, 1000000),
    balance DECIMAL MERGE sum_bounded(-1000, 10000)
);

INSERT INTO counter_test VALUES ('post1', 100, 50, 500.00);
INSERT INTO counter_test VALUES ('post1', 50, 25, 100.50) ON CONFLICT MERGE;
INSERT INTO counter_test VALUES ('post1', -10, 10, -50.25) ON CONFLICT MERGE;
SELECT * FROM counter_test; -- Should show sum of all values

-- Test 5: Max/Min CRDTs
CREATE TABLE maxmin_test (
    id TEXT PRIMARY KEY,
    high_score INT MERGE max,
    low_temp FLOAT MERGE min,
    latest_version TEXT MERGE max  -- Lexicographic max
);

INSERT INTO maxmin_test VALUES ('game1', 1000, 15.5, 'v1.0.0');
INSERT INTO maxmin_test VALUES ('game1', 1500, 10.2, 'v0.9.0') ON CONFLICT MERGE;
INSERT INTO maxmin_test VALUES ('game1', 800, 18.0, 'v1.1.0') ON CONFLICT MERGE;
SELECT * FROM maxmin_test; -- high_score=1500, low_temp=10.2, latest_version='v1.1.0'

-- Test 6: Map CRDT (Nested Structure)
CREATE TABLE map_test (
    id TEXT PRIMARY KEY,
    config MAP<TEXT, INT> MERGE map_lww,
    settings MAP<TEXT, TEXT> MERGE map_lww
);

INSERT INTO map_test VALUES ('app1', {'timeout': 30, 'retries': 3}, {'theme': 'dark'});
INSERT INTO map_test VALUES ('app1', {'timeout': 60, 'max_conn': 100}, {'lang': 'en'}) ON CONFLICT MERGE;
SELECT * FROM map_test; -- Merged map with all keys

-- Test 7: Multi-Value Register (MVR)
CREATE TABLE mvr_test (
    id TEXT PRIMARY KEY,
    value TEXT MERGE mv_register,
    versions SET<TEXT> MERGE gset
);

-- Simulate concurrent writes without causal ordering
INSERT INTO mvr_test VALUES ('doc1', 'content_a', {'v1'});
INSERT INTO mvr_test VALUES ('doc1', 'content_b', {'v2'}) ON CONFLICT MERGE;
-- Both values should be preserved until causally resolved
SELECT * FROM mvr_test;

-- Test 8: Observed-Remove Set (OR-Set)
CREATE TABLE orset_test (
    id TEXT PRIMARY KEY,
    items MAP<TEXT, SET<UUID>> MERGE orset
);

-- Add items with unique tags
INSERT INTO orset_test VALUES ('list1', {'apple': {'uuid1'}, 'banana': {'uuid2'}});
-- Add same item with different tag
INSERT INTO orset_test VALUES ('list1', {'apple': {'uuid3'}, 'cherry': {'uuid4'}}) ON CONFLICT MERGE;
-- Remove specific instance
INSERT INTO orset_test VALUES ('list1', {'apple': {}}) ON CONFLICT MERGE; -- Removes all apple instances
SELECT * FROM orset_test;

-- Test 9: Complex Merge with Multiple CRDT Types
CREATE TABLE complex_merge (
    id TEXT PRIMARY KEY,
    name TEXT MERGE lww,
    score INT MERGE sum,
    tags SET<TEXT> MERGE gset,
    max_level INT MERGE max,
    metadata MAP<TEXT, TEXT> MERGE map_lww
);

-- Multiple concurrent updates
INSERT INTO complex_merge VALUES ('player1', 'Alice', 100, {'beginner'}, 5, {'status': 'active'});
INSERT INTO complex_merge VALUES ('player1', 'Alice_Pro', 50, {'intermediate'}, 8, {'badge': 'silver'}) ON CONFLICT MERGE;
INSERT INTO complex_merge VALUES ('player1', 'Alice', 25, {'expert'}, 3, {'status': 'premium'}) ON CONFLICT MERGE;
SELECT * FROM complex_merge;

-- Test 10: Merge with Temporal Aspects
CREATE TABLE temporal_merge (
    id TEXT PRIMARY KEY,
    value INT MERGE sum,
    valid_from TIMESTAMP,
    valid_to TIMESTAMP
);

INSERT INTO temporal_merge VALUES ('metric1', 10, '2024-01-01', '2024-12-31');
INSERT INTO temporal_merge VALID PERIOD ['2024-06-01', '2024-06-30')
    VALUES ('metric1', 5, '2024-06-01', '2024-06-30') ON CONFLICT MERGE;
SELECT * FROM temporal_merge FOR SYSTEM_TIME AS OF CURRENT_TIMESTAMP;

-- Cleanup
DROP TABLE IF EXISTS lww_test;
DROP TABLE IF EXISTS gset_test;
DROP TABLE IF EXISTS twoset_test;
DROP TABLE IF EXISTS counter_test;
DROP TABLE IF EXISTS maxmin_test;
DROP TABLE IF EXISTS map_test;
DROP TABLE IF EXISTS mvr_test;
DROP TABLE IF EXISTS orset_test;
DROP TABLE IF EXISTS complex_merge;
DROP TABLE IF EXISTS temporal_merge;
EXIT;