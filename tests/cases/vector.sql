CREATE TABLE vecs (
                      id TEXT PRIMARY KEY,
                      v VECTOR<3>
);
INSERT INTO vecs (id, v) VALUES
                             ('a', [0.1,0.0,0.0]),
                             ('b', [0.3,0.0,0.0]);
SELECT id FROM vecs WHERE DISTANCE(v, [0.0,0.0,0.0]) < 0.2;
EXIT;
