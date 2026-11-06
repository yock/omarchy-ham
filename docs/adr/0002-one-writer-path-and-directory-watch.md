# 0002 — One writer path and directory watch

## Status

Accepted.

## Context

Three kinds of writer exist: the QML widget, a C++ app writing operator identity,
and the CLI. Each doing its own read-modify-write of JSON is how fields get lost
and files get corrupted. Omarchy already writes its own config atomically — a
temporary file followed by a rename — and Qt's `QFileSystemWatcher` stops
watching a path after that rename, because the inode is replaced.

Polling would avoid the watcher problem but adds latency and wakeups. Watching
the file inode is the obvious implementation and the wrong one.

## Decision

The CLI and library share one writer implementation:

1. take an advisory lock;
2. read the current profile;
3. apply the change;
4. validate against the schema;
5. write a temporary file in the same directory, flush it, and rename it over
   `station.json`;
6. flush the directory;
7. release the lock.

Consumers watch the **directory**, re-read on change, validate, and apply. They
ignore temporary files and only react to `station.json`.

## Consequences

- Concurrent identity edits cannot lose each other's fields.
- A reader never observes a half-written profile.
- The watcher complexity lives in one place instead of every app.
- A writer that bypasses the implementation can still corrupt the file; code
  review and the strict schema are the defence.
- A rename is observed as a directory event, so a live reload works across
  editors and CLI versions.

## Rejected

- **Direct file writes by each app.** Lost updates and inconsistent formatting.
- **Watching the file inode.** Goes silent after the first atomic replace.
- **Polling.** Works, but adds latency and wakeups for a change that is rare.
- **SIGHUP-only reload.** Not usable from a QML app and not discoverable by a
  desktop app that did not start under a supervisor.
