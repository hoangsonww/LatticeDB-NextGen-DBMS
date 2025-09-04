CREATE TABLE people (
                        id TEXT PRIMARY KEY,
                        name TEXT MERGE lww,
                        tags SET<TEXT> MERGE gset,
                        credits INT MERGE sum_bounded(0, 1000000),
                        profile_vec VECTOR<4>
);

INSERT INTO people (id, name, tags, credits, profile_vec) VALUES
                                                              ('u1','Ada', {'engineer','math'}, 10, [0.1,0.2,0.3,0.4]),
                                                              ('u2','Grace', {'engineer'}, 20, [0.4,0.3,0.2,0.1]);

INSERT INTO people (id, credits, tags, name) VALUES
    ('u1', 15, {'leader'}, 'Ada Lovelace') ON CONFLICT MERGE;

SELECT id, name, credits FROM people;
SET DP_EPSILON = 0.4;
SELECT DP_COUNT(*) FROM people WHERE credits >= 10;
SAVE DATABASE 'snapshot.ldb';
EXIT;
