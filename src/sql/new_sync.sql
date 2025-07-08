INSERT INTO syncs (
    timestamp,
    ddb_ows_version,
    fn_format,
    cover_sync,
    cover_fname,
    rm_unref)
VALUES (
    unixepoch(),
    :ddb_ows_version,
    :fn_format,
    :cover_sync,
    :cover_fname,
    :rm_unref
)
RETURNING id
