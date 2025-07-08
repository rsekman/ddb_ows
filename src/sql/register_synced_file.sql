INSERT INTO synced_files (file_id, sync_id, timestamp, destination, conversion_preset)
SELECT
    id AS file_id,
    :sync_id AS sync_id,
    :timestamp AS timestamp,
    :destination AS destination,
    :conv_preset AS conversion_preset
FROM files
WHERE source = :source;
