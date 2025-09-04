CREATE TABLE t (
                   id TEXT PRIMARY KEY,
                   v INT
);
INSERT INTO t (id, v) VALUES ('a', 1);
UPDATE t SET v=2 WHERE id='a';
SELECT id, v FROM t FOR SYSTEM_TIME AS OF TX 1;
EXIT;
