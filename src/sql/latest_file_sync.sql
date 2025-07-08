SELECT
    files.source AS source,
    synced.destination AS destination,
    synced.conversion_preset as conversion_preset,
    synced.timestamp AS timestamp,
    synced.sync_id AS sync_id
FROM files
INNER JOIN synced_files AS synced
ON files.id = synced.file_id
WHERE files.source = :source
ORDER BY timestamp DESC, destination DESC
LIMIT 1
