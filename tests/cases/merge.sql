CREATE TABLE people (
                        id TEXT PRIMARY KEY,
                        name TEXT MERGE lww,
                        tags SET<TEXT> MERGE gset,
                        credits INT MERGE sum_bounded(0, 1000000)
);
INSERT INTO people (id, name, tags, credits) VALUES
                                                 ('u1','Ada', {'engineer','math'}, 10),
                                                 ('u2','Grace', {'engineer'}, 20);
INSERT INTO people (id, credits, tags, name) VALUES
    ('u1', 15, {'leader'}, 'Ada Lovelace') ON CONFLICT MERGE;
SELECT id, name, credits, tags FROM people;
EXIT;
