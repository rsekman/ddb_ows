INSERT INTO synced_playlists (playlist_uuid, sync_id)
SELECT
    uuid AS playlist_uuid,
    :sync_id AS sync_id
FROM playlists
WHERE uuid = :uuid;
