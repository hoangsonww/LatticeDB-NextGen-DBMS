CREATE TABLE users (id INTEGER PRIMARY KEY, name VARCHAR(50));
INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob');
SELECT * FROM users;
SELECT * FROM users WHERE id = 1;
UPDATE users SET name = 'Charlie' WHERE id = 2;
SELECT * FROM users;
DELETE FROM users WHERE id = 1;
SELECT * FROM users;
DROP TABLE users;
