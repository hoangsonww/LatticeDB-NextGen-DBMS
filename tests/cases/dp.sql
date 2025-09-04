CREATE TABLE users (
                       id TEXT PRIMARY KEY,
                       age INT
);
INSERT INTO users (id, age) VALUES ('a', 20), ('b', 35);
SET DP_EPSILON = 1.0;
SELECT DP_COUNT(*) FROM users WHERE age >= 30;
EXIT;
