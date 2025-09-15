-- Test: Schema Evolution and DDL Operations
-- Tests ALTER TABLE, schema modifications, and online DDL

-- Test 1: Add Column
CREATE TABLE evolving_table (
    id INT PRIMARY KEY,
    name TEXT
);
INSERT INTO evolving_table (id, name) VALUES (1, 'Original');
ALTER TABLE evolving_table ADD COLUMN email TEXT;
INSERT INTO evolving_table (id, name, email) VALUES (2, 'New', 'new@test.com');
SELECT * FROM evolving_table ORDER BY id;

-- Test 2: Drop Column
ALTER TABLE evolving_table DROP COLUMN email;
SELECT * FROM evolving_table ORDER BY id;

-- Test 3: Rename Column
ALTER TABLE evolving_table RENAME COLUMN name TO full_name;
SELECT * FROM evolving_table ORDER BY id;

-- Test 4: Modify Column Type
ALTER TABLE evolving_table ALTER COLUMN full_name TYPE VARCHAR(100);
INSERT INTO evolving_table (id, full_name) VALUES (3, 'Type Changed');
SELECT * FROM evolving_table ORDER BY id;

-- Test 5: Add Constraints
ALTER TABLE evolving_table ADD CONSTRAINT name_not_empty CHECK (full_name != '');
-- This should fail
INSERT INTO evolving_table (id, full_name) VALUES (4, '');
-- This should succeed
INSERT INTO evolving_table (id, full_name) VALUES (4, 'Valid Name');

-- Test 6: Add Foreign Key
CREATE TABLE referenced_table (
    ref_id INT PRIMARY KEY,
    ref_value TEXT
);
INSERT INTO referenced_table VALUES (1, 'Ref1'), (2, 'Ref2');
ALTER TABLE evolving_table ADD COLUMN ref_id INT;
ALTER TABLE evolving_table ADD FOREIGN KEY (ref_id) REFERENCES referenced_table(ref_id);
UPDATE evolving_table SET ref_id = 1 WHERE id = 1;
-- This should fail (invalid FK)
UPDATE evolving_table SET ref_id = 999 WHERE id = 2;

-- Test 7: Add CRDT Column to Existing Table
ALTER TABLE evolving_table ADD COLUMN counter INT MERGE sum;
INSERT INTO evolving_table (id, full_name, counter) VALUES (5, 'CRDT Test', 10)
    ON CONFLICT (id) DO UPDATE SET counter = EXCLUDED.counter MERGE;
SELECT * FROM evolving_table WHERE id = 5;

-- Test 8: Add Vector Column
ALTER TABLE evolving_table ADD COLUMN embedding VECTOR<4>;
UPDATE evolving_table SET embedding = [0.1, 0.2, 0.3, 0.4] WHERE id = 1;
SELECT id, full_name, embedding FROM evolving_table WHERE embedding IS NOT NULL;

-- Test 9: Add Temporal Columns
ALTER TABLE evolving_table ADD COLUMN valid_from TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
ALTER TABLE evolving_table ADD COLUMN valid_to TIMESTAMP;
UPDATE evolving_table SET valid_to = CURRENT_TIMESTAMP + INTERVAL '1 year' WHERE id > 2;
SELECT * FROM evolving_table WHERE valid_to IS NOT NULL;

-- Test 10: Rename Table
ALTER TABLE evolving_table RENAME TO evolved_table;
SELECT COUNT(*) FROM evolved_table;

-- Test 11: Create Table As Select (CTAS)
CREATE TABLE summary_table AS
SELECT
    ref_id,
    COUNT(*) as count,
    STRING_AGG(full_name, ', ') as names
FROM evolved_table
WHERE ref_id IS NOT NULL
GROUP BY ref_id;
SELECT * FROM summary_table;

-- Test 12: Add Computed Column
ALTER TABLE evolved_table ADD COLUMN name_length INT GENERATED ALWAYS AS (LENGTH(full_name)) STORED;
SELECT id, full_name, name_length FROM evolved_table ORDER BY name_length DESC;

-- Test 13: Partition Table
CREATE TABLE partitioned_events (
    event_id SERIAL,
    event_time TIMESTAMP,
    event_type TEXT,
    data JSONB
) PARTITION BY RANGE (event_time);

CREATE TABLE events_2024_q1 PARTITION OF partitioned_events
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');
CREATE TABLE events_2024_q2 PARTITION OF partitioned_events
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');

INSERT INTO partitioned_events (event_time, event_type, data) VALUES
    ('2024-02-15', 'click', '{"page": "home"}'),
    ('2024-05-20', 'purchase', '{"amount": 99.99}');
SELECT * FROM partitioned_events ORDER BY event_time;

-- Cleanup
DROP TABLE IF EXISTS evolved_table CASCADE;
DROP TABLE IF EXISTS referenced_table CASCADE;
DROP TABLE IF EXISTS summary_table;
DROP TABLE IF EXISTS partitioned_events CASCADE;
EXIT;