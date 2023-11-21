# DeaDBeeF One-Way Sync
One-way Sync Plugin for the DeaDBeeF Audio Player

For e.g. managing portable music players like FiiO, Zune, Walkman.

Essentially treats DeaDBeeF playlists as an authoritative source for the contents of the destination.
The contents of one or more playlists are copied to the destination, renaming files according to a configurable format.
For performance only files newer than existing destination files are copied.
Optionally, destination files that no longer have a source file can be purged from the destination.
This can be useful e.g. if you want the destination directory structure to reflect playlist structure, have replaced mp3 files with FLAC, or just don't want the files on the destination anymore.

Inspired by foo_ows

## Building

Requirements
- `ddb_ows` requires DeaDBeeF headers >= [`788d277`](https://github.com/DeaDBeeF-Player/deadbeef/commit/788d277ac08ecaed5b8a215b0e7146d7630c71df) to build as-is.
It can be built with older headers by cloning the DeaDBeeF repository to your `CPATH` and modifying the relevant includes to read from there.
- gtk headers
- `meson`
- `clang`
- [`fmt`](https://github.com/fmtlib/fmt)

```sh
export DDB_OWS_LOGLEVEL=n
meson setup build && cd build
meson install
```
Set `n = 0, 1, 2, 3` for increasingly verbose console logging on `stderr`;
the default is `3`.
To install for the current user only, pass `--libir ~/.local/lib` to `meson setup`.

`ddb_ows` is Linux only with no plans to support other operating systems.

## Handling unallowed characters

File systems generally do not allow file names to contain all bytes; e.g. ext[2-4] reserve `/` and exFAT does not allow the characters `/\:*?"<>|`.
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
the output is `The Ocean/Phanerozoic I- Palaeozoic/Cambrian II- Eternal Recurrence`.

As a consequence, if your title formatting string contains functions whose outputs include `/` (e.g., `$replace(%title%, -, /), $if(..., /)` these will be interpretated literally;
that is, so as to sub-path.

This behaviour replicates `foo_fileops` as far as I can tell without source access.

## Case-sensitivity

If your metadata is inconsistent in capitalisation, on a case-sensitive file system the output directory structure may be similarly inconsistent.
That is, if you have files tagged both `Simon and Garfunkel` and `Simon And Garfunkel` and the destination format is something like `%artist%/%title%.%filename_ext%`, `ddb_ows` will (attempt to) create *two* directories.
This is not an issue on case-insensitive file systems such as vfat.
On, e.g., ext3, though, this may not be what you want.
You can mogrify your format with string replacing functions, but the best solution is probably to retag your files using consistent capitalisation.

## Sorting

Many portable music players do not sort filesystem entries in any way, going entirely by directory entry order in the filesystem.
As you add tracks or move them between playlists, or even just as a result of multithreading, this may increasingly diverge from what you want.
If your target filesystem is FAT the [`fatsort`](https://fatsort.sourceforge.io/) tool addresses precisely this problem.
Run `fatsort` on the target filesystem after each sync.

## License

GPL 3
