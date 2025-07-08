BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS "files" (
    "id"	INTEGER NOT NULL UNIQUE,
    "source"	TEXT NOT NULL UNIQUE,
    PRIMARY KEY("id" AUTOINCREMENT)
);
CREATE TABLE IF NOT EXISTS "synced_files" (
    "file_id"	INTEGER NOT NULL,
    "sync_id"	INTEGER NOT NULL,
    "timestamp"	INTEGER NOT NULL,
    "destination"	TEXT,
    "conversion_preset"	TEXT,
    FOREIGN KEY("file_id") REFERENCES "files"("id"),
    FOREIGN KEY("sync_id") REFERENCES "syncs"("id")
);
CREATE TABLE IF NOT EXISTS "syncs" (
    "id"	INTEGER NOT NULL UNIQUE,
    "timestamp"	INTEGER NOT NULL,
    "ddb_ows_version"	TEXT NOT NULL,
    "fn_format"	TEXT,
    "cover_sync"	INTEGER,
    "cover_fname"	TEXT,
    "rm_unref"	INTEGER,
    PRIMARY KEY("id" AUTOINCREMENT)
);
CREATE INDEX IF NOT EXISTS "idx_source" ON "files" (
    "source"	ASC
);
CREATE INDEX IF NOT EXISTS "idx_synced_files" ON "synced_files" (
    "file_id"	ASC,
    "timestamp" DESC
);

INSERT INTO meta (key, value) VALUES ('schema_version', '1')
    ON CONFLICT DO UPDATE SET value=excluded.value;
INSERT INTO meta (key, value) VALUES ('app_version', '0.4.0')
    ON CONFLICT DO UPDATE SET value=excluded.value;

COMMIT;
