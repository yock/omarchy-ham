# 0008 — Version 1 is config tooling, not an audio engine

## Status

Accepted.

## Context

Making an app follow a node name is harder than it looks. PortAudio has no
PulseAudio host API; its ALSA path sees only generic `pipewire`/`default`
devices. `PIPEWIRE_NODE` targets a node by name but is process-global and silently
falls back when the node is missing. A per-role ALSA PCM alias works but adds an
ALSA-config dependency. Native PipeWire `target.object` plus
`node.dont-fallback` is the only clean per-role path, and JS8 has not yet chosen a
transmit backend.

A shared audio-stream helper would make every app behave identically, but it would
lock in a backend before JS8's TX design exists, and the QML widget cannot link a
C++ audio library anyway.

## Decision

Version 1 ships config tooling only: schema, CLI, and a library that reads,
writes, watches, validates, migrates, and checks whether a node exists. It does
not open audio streams. `SPEC.md` documents the verified targeting recipes and
requires independent RX/TX targeting, but each app owns its stream.

## Consequences

- The contract can ship before the audio backend question is settled.
- Apps may implement targeting differently; the behaviour rules keep them
  observably consistent.
- The library still earns its keep by centralizing the writer, watcher,
  validation, migration and existence check.
- A shared stream helper can be added later as a separate module once JS8 has a
  real TX path.
- The widget, being QML, only ever needs the CLI.

## Rejected

- **A shared audio engine in version 1.** Premature backend lock-in and unusable
  from QML.
- **A daemon that owns streams.** More lifecycle and a single point of failure for
  no current need.
- **WirePlumber per-application routing only.** Depends on deployment, can be
  overridden by stream restore, and does not give an app a clear error when its
  configured node is missing.
