INSERT INTO files_in_playlists (file_id, playlist_uuid)
SELECT
    id AS file_id,
    :playlist_uuid AS playlist_uuid
FROM files
WHERE source = :source
ON CONFLICT DO NOTHING;
