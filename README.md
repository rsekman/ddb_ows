# DeaDBeeF One-Way Sync
One-way Sync Plugin for the DeaDBeeF Audio Player

For e.g. managing portable music players like FiiO, Zune, Walkman.

Essentially treats DeaDBeeF playlists as an authoritative source for the contents of the destination.
The contents of one or more playlists are copied to the destination, renaming files according to a configurable format.
For performance only files newer than existing destination files are copied.
Optionally, destination files that no longer have a source file can be purged from the destination.
This can be useful e.g. if you want the destination directory structure to reflect playlist structure, have replaced mp3 files with FLAC, or just don't want the files on the destination anymore.

Inspired by foo_ows

## Handling unallowed characters

File systems generally do not allow file names to contain all bytes; e.g. ext[2-4] reserve `/` and 
exFAT does not allow the characters `/\:*?"<>|`.
As id3 tags may contain these characters, the following escaping scheme is implemented

1. id3 tags undergo the replacement `/ -> -` before they are seen by title formatting
2. After title formatting unallowed characters *other than `/`* are replaced with `-`.

Thus, for example, the formatting string `%artist/%album%/%title%` outputs, for the fields
```
artist: Ulver
album: Perdition City
title: Nowhere/Catastrophe
```
the string. `Ulver/Perdition City/Nowhere-Catastrophe`.
With the fields
```
artist: The Ocean
album: Phanerozoic I: Palaeozoic
title: Cambrian II: Eternal Recurrence
```
the output is `The Ocean/Phanerozoic I- Palaezoic/Cambrian II- Eternal Recurrence`.

If your title formatting string contains functions whose outputs include `/` (e.g., `$replace(%title%, -, /), $if(..., /)` these will be interpretated literally;
that is, so as to sub-path.

This behaviour replicates `foo_fileops` as far as I can tell without source access.

## License

GPL 3

## Requirements:

- DeaDBeeF >= 0.7.2 (API >= 1.9) Probably works fine with older versions. Caveat compiler.
