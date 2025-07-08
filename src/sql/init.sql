BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY,
    value TEXT
);
INSERT INTO meta (key, value) VALUES ('schema_version', '0') ON CONFLICT DO NOTHING;
INSERT INTO meta (key, value) VALUES ('app_version', '0.4.0') ON CONFLICT DO NOTHING;

SELECT
    CAST(a.value AS INTEGER) as schema_version, b.value as app_version
    FROM meta AS a
    JOIN meta AS b
    WHERE a.key = 'schema_version' AND b.key = 'app_version';

COMMIT;
