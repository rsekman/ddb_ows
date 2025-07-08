INSERT INTO files_in_playlists (file_id, playlist_uuid)
VALUES (:file_id, :playlist_uuid)
ON CONFLICT DO NOTHING;
