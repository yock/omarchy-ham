# 0001 — The station profile is a file contract, not a daemon

## Status

Accepted.

## Context

Several apps need the same station knowledge: which PipeWire node carries Radio
RX and Radio TX, which transport a TNC uses, which `rigctld` endpoint is the rig,
and what callsign and grid the operator uses. Today the Station Readiness widget
stores its answers in its entry in `~/.config/omarchy/shell.json`, while JS8
stores a single PortAudio input name in `~/.config/js8/settings.json`. The two
cannot see each other, and every new app would add another private copy.

A daemon or D-Bus broker could own the data and push changes. It would also add a
process to install, start, supervise and debug, and become a single point of
failure for data that changes when a human plugs in a cable.

## Decision

The station profile is a versioned JSON file plus a CLI and a thin library. There
is no daemon in version 1. A single writer path performs atomic read-modify-write;
consumers watch the containing directory and re-read after a replace.

## Consequences

- Apps can use the profile when the widget is not installed or not running.
- No service lifecycle, port, socket, or startup ordering to manage.
- Live propagation is a directory watch, not a push. Native apps get the same
  practical latency without a broker.
- A future daemon or D-Bus facade can be added in front of the same file without
  changing the contract. It would be an adapter, not the source of truth.
- Sandboxed consumers that cannot watch the config directory would need that
  adapter. None of the current consumers are sandboxed.

## Rejected

- **Embedding the profile in `shell.json`.** It couples every app to the shell's
  config lifecycle and to a bar widget's release.
- **A per-app config with a shared reader.** Still N copies of the truth.
- **A daemon as the source of truth.** More machinery than the data justifies in
  version 1, and a new failure mode for no current benefit.
