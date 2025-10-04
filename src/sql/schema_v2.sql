BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS "playlists" (
    "uuid"	TEXT NOT NULL UNIQUE,
    "title"	TEXT,
    PRIMARY KEY("uuid")
);

CREATE TABLE IF NOT EXISTS "synced_playlists" (
    "playlist_uuid"	BLOB NOT NULL,
    "sync_id"	INTEGER NOT NULL,
    FOREIGN KEY("playlist_uuid") REFERENCES "playlists"("uuid"),
    FOREIGN KEY("sync_id") REFERENCES "syncs"("id")
);

CREATE TABLE IF NOT EXISTS "files_in_playlists" (
    "file_id"	INTEGER NOT NULL,
    "playlist_uuid"	BLOB NOT NULL,
    CONSTRAINT "uniqueness" UNIQUE("playlist_uuid","file_id"),
    FOREIGN KEY("file_id") REFERENCES "files"("id"),
    FOREIGN KEY("playlist_uuid") REFERENCES "playlists"("uuid")
);

CREATE INDEX IF NOT EXISTS "idx_playlist_entries_file" ON files_in_playlists(file_id);
CREATE INDEX IF NOT EXISTS "idx_playlist_entries_playlist" ON files_in_playlists(playlist_uuid);

INSERT INTO meta (key, value) VALUES ('schema_version', '2')
    ON CONFLICT DO UPDATE SET value=excluded.value;
INSERT INTO meta (key, value) VALUES ('app_version', '0.6.0')
    ON CONFLICT DO UPDATE SET value=excluded.value;

COMMIT;
