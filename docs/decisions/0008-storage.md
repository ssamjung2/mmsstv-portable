# 0008: SQLite index, content-addressed image files, TOML settings

- Status: Proposed
- Date: 2026-09-19

## Context

A station accumulates thousands of received pictures, each with metadata worth
searching (`R-LIB-2`), plus a contact log that must survive power loss on a Pi
that is switched off at the wall.

## Decision

- **Pictures**: ordinary files on disk, content-addressed, in a directory the
  operator can back up or point other tools at.
- **Index, metadata and log**: one SQLite database, with separate schemas for
  the library and the log.
- **Settings**: a single documented TOML file per platform convention.

## Consequences

- Pictures remain usable without our software, which matters for a hobby
  archive.
- Search is a query rather than a directory walk; ADIF export is a join.
- SQLite gives durability across power loss and exists everywhere.
- The index can drift from the files, so a repair pass that rebuilds the index
  from the directory is a required feature, not a maintenance script.
- Deduplication comes free from content addressing; renaming does not.
