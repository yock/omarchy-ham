# 0010 — RX and TX are targeted independently

## Status

Accepted.

## Context

JS8 is receive-only today, so one target is enough. That will not last: a
transmitting app needs Radio RX and Radio TX open at the same time, pointing at
different nodes. The easiest mechanism, the `PIPEWIRE_NODE` environment variable,
is process-global: setting it for RX also pins TX, and it silently falls back to
the default when the name is missing. PortAudio's JACK host API exposes nodes by
description with unstable indices and a strict graph-rate requirement. Native
PipeWire `target.object` and an ALSA PCM alias can each target a role
independently.

If the contract allowed "open the default and hope", apps would disagree about
which role they are following and the station would quietly transmit through the
wrong sink.

## Decision

The contract requires independent Radio RX and Radio TX targeting, including
simultaneous streams. An app must never rely on the system default for a bound
role, and a process-global environment variable is acceptable only as a
temporary, single-stream, RX-only implementation with a separate existence check.

The recommended implementations are native PipeWire `target.object` with
`node.dont-fallback`, or a per-role ALSA PCM alias. The contract names the
requirement; it does not mandate one API.

## Consequences

- JS8's current single-input implementation can use a shortcut now, but it must
  migrate before transmit lands.
- Apps must pre-validate the node, because PipeWire's silent fallback defeats
  "the stream opened" as an error test.
- The station never changes the system default route; the hazard of transmitting
  unrelated sound is reported, not hidden.
- A future shared stream helper, if added, must satisfy this requirement.

## Rejected

- **One `PIPEWIRE_NODE` for the whole process.** Cannot express two roles.
- **Setting the system default source/sink while the app runs.** Reconfigures
  every other app and puts unrelated sound on air.
- **JACK description matching.** Unstable indices, localized descriptions, and a
  graph-rate lock.
