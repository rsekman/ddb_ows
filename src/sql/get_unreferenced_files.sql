SELECT
    files.source AS source,
    synced.destination AS destination,
    MAX(synced.timestamp) as timestamp
FROM files
INNER JOIN synced_files AS synced ON files.id = synced.file_id
WHERE NOT EXISTS (
    SELECT 1
    FROM files_in_playlists AS ref
    WHERE ref.file_id = files.id
)
GROUP BY synced.file_id
HAVING synced.destination IS NOT NULL
ORDER BY synced.destination;
