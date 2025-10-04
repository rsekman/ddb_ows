INSERT INTO playlists (uuid, title) VALUES (:uuid, :title) ON CONFLICT DO NOTHING;
