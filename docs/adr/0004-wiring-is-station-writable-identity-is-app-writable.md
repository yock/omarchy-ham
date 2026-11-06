# 0004 — Wiring is station-writable; operator identity is app-writable

## Status

Accepted.

## Context

The station profile holds two different kinds of data. Wiring — nodes, paths, TNC
transport, CAT endpoint — is a station-level decision. If an app silently changes
wiring because the user picked a device in its own settings, every other app is
reconfigured without consent. Operator identity — callsign and grid — is
different: it is personal, it is already edited in apps such as JS8, and a
last-write-wins conflict is harmless.

Making the whole profile read-only for apps would force JS8's existing callsign
and grid fields to become read-only and would push an identity editor into the
widget. Making the whole profile writable would bring back silent station
reconfiguration.

## Decision

Split writes by field class:

- **Wiring** (`station`, `audio`, `tnc`, `cat`) is written only by the station
  editor — the widget panel or the CLI.
- **Operator identity** (`operator`) may be written by a consumer on an explicit
  user action, through the shared writer. Last write wins.

A consumer never writes wiring. A device choice made inside an app is not a
station change; the app may offer an explicit "set as station default" action
that writes through the station editor path.

## Consequences

- JS8 can keep its callsign and grid editing experience.
- The widget and CLI can also edit identity, so a headless station remains
  configurable.
- No per-field locking; identity conflicts are low-frequency and user-initiated.
- The capability matrix must say which identity fields each consumer writes.

## Rejected

- **Everything read-only for apps.** Forces an identity-editor detour and removes
  a natural place to set a callsign.
- **Everything writable by apps.** A local device choice would silently
  reconfigure the station.
- **Per-field locks or a lease.** More failure modes than the data justifies.
