CREATE TABLE events (
                        id TEXT PRIMARY KEY,
                        kind TEXT MERGE lww
);
INSERT INTO events (id, kind) VALUES ('e1','open');
UPDATE events SET kind='closed' VALID PERIOD ['2025-01-01T00:00:00Z','2025-02-01T00:00:00Z') WHERE id='e1';
SELECT id FROM events;
EXIT;
