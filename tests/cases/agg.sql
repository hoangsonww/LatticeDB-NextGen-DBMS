CREATE TABLE nums (
    id INT PRIMARY KEY,
    val INT
);
INSERT INTO nums (id, val) VALUES (1,10),(2,20),(3,30);
SELECT MIN(val), MAX(val) FROM nums;
EXIT;
