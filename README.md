# DeaDBeeF One-Way Sync
One-way Sync Plugin for the DeaDBeeF Audio Player

For e.g. managing portable music players like FiiO, Zune, Walkman.

Essentially treats DeaDBeeF playlists as an authoritative source for the contents of the destination.
The contents of one or more playlists are copied to the destination, renaming files according to a configurable format.
For performance only files newer than existing destination files are copied.
Optionally, destination files that no longer have a source file can be purged from the destination.
This can be useful e.g. if you want the destination directory structure to reflect playlist structure, have replaced mp3 files with FLAC, or just don't want the files on the destination anymore.

Inspired by foo_ows

## License

GPL 3

## Requirements:

- DeaDBeeF >= 0.7.2 (API >= 1.9) Probably works fine with older versions. Caveat compiler.
