CREATE TABLE p (
                   id TEXT PRIMARY KEY,
                   v INT
);
INSERT INTO p (id, v) VALUES ('x', 42);
SAVE DATABASE 'tmp.ldb';
EXIT;
