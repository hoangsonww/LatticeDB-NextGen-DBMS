CREATE TABLE test (id INT, name VARCHAR(50));
INSERT INTO test VALUES (1, 'Alice');
INSERT INTO test VALUES (2, 'Bob'), (3, 'Charlie');
SELECT * FROM test;
UPDATE test SET name = 'Bobby' WHERE id = 2;
SELECT * FROM test WHERE id = 2;
DELETE FROM test WHERE id = 3;
SELECT * FROM test;
DROP TABLE test;
