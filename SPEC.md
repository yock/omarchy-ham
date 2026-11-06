# Omarchy Ham Station Profile

**Status:** Version 1 · 2026-11-06 · CLI/library 0.1.0
**Schema:** [`schema/station.schema.json`](schema/station.schema.json)
**Related:** [Station Readiness](https://github.com/yock/omarchy-station) · [Omarchy JS8](https://github.com/yock/omarchy-js8)

## 1. Purpose

The station profile is the shared answer to one question: **which audio nodes,
modem transport, CAT endpoint and operator identity does this station use?**

One app can then choose a device and another app can follow it without keeping a
private copy of the same decision. The profile is a contract, not a runtime:
PipeWire carries audio, `rigctld` carries CAT, and the profile records which
nodes and endpoints the operator selected.

## 2. Scope

In scope:

- operator callsign, Maidenhead grid, and grid source
- Radio RX and Radio TX PipeWire `node.name` bindings
- the station paths the operator declares: host-audio, TNC, or both
- TNC transport and an optional baud rate
- the CAT endpoint: a `rigctld` host and port

Out of scope:

- dial frequency, mode, and per-app preferences
- rig model, serial speed, and PTT wiring; `rigctld` owns those
- audio stream implementation
- device auto-binding and system default-route changes
- readiness grading; the Station Readiness widget owns that
- a daemon, D-Bus service, or broker
- `gpsMode`; it stays widget-local in version 1 (see ADR 0012)

## 3. The profile file

### 3.1 Location

```
$XDG_CONFIG_HOME/omarchy/ham/station.json
```

When `XDG_CONFIG_HOME` is unset, the path is
`~/.config/omarchy/ham/station.json`.

The directory is `0755`; the file is `0644`. The profile contains a callsign and
grid, both public, and **never** contains passwords, API keys, or other secrets.

### 3.2 Encoding and shape

The file is UTF-8 JSON, two-space indented, with a trailing newline. The root is
an object with a required integer `version`.

```json
{
  "version": 1,
  "operator": {
    "callsign": "N0CALL",
    "grid": "FN31pr",
    "gridSource": "manual"
  },
  "station": {
    "paths": ["host-audio"]
  },
  "audio": {
    "rx": {
      "node": "alsa_input.usb-C-Media_Electronics_Inc._USB_Audio_Device-00.mono-fallback"
    },
    "tx": {
      "node": "alsa_output.usb-C-Media_Electronics_Inc._USB_Audio_Device-00.analog-stereo"
    }
  },
  "cat": {
    "kind": "rigctld",
    "host": "localhost",
    "port": 4532
  }
}
```

A missing file is a valid **unconfigured** state. A file that does not validate
is an error state; consumers keep their last good view and report it.

### 3.3 Atomic writes and watching

The CLI and library share one writer path:

1. take an advisory lock;
2. read the current file;
3. apply the change;
4. validate the result against the schema;
5. write a temporary file in the same directory, flush it, and rename it over
   `station.json`;
6. flush the directory;
7. release the lock.

Consumers watch the **directory**, not the file inode. An atomic rename replaces
the inode, so a watcher bound only to the old file would go silent. On a change,
re-read, validate, and apply the new profile. Ignore temporary files; only
`station.json` is the profile.

No app writes JSON by hand. The CLI is the reference writer; an app that writes
operator identity uses the shared library's writer, which is the same
implementation.

### 3.4 Unknown fields and versions

The schema is strict: unknown fields are validation errors, so a typo cannot
silently become a setting. Version 1 is frozen. Adding a field, changing a type,
or changing semantics requires a new `version`.

A consumer that does not understand the profile's `version` must refuse to apply
it and tell the operator to upgrade. It must not guess.

## 4. Field reference

| Field | Type | Required | Meaning |
|---|---|---|---|
| `version` | integer, `1` | yes | Profile format version. |
| `operator.callsign` | string, 3–16 chars | no | Amateur callsign, normalised uppercase. |
| `operator.grid` | string, Maidenhead 4/6/8 | no | Grid locator; canonical display is 6 characters. |
| `operator.gridSource` | `manual` or `gps` | no | Default `manual`. |
| `station.paths` | array of `host-audio`, `tnc` | yes if `station` exists | Declared paths; at least one, no duplicates. |
| `audio.rx.node` | string | yes if `audio.rx` exists | PipeWire `node.name` of the Radio RX source. |
| `audio.tx.node` | string | yes if `audio.tx` exists | PipeWire `node.name` of the Radio TX sink. |
| `tnc.transport` | object | yes if `tnc` exists | Serial or Bluetooth transport. |
| `tnc.baud` | integer ≥ 1 | no | Serial baud; ignored for Bluetooth. |
| `cat.kind` | `rigctld` | yes if `cat` exists | CAT endpoint kind. |
| `cat.host` | string | no | Default `localhost`. |
| `cat.port` | integer 1–65535 | no | Default `4532`. |

### 4.1 Operator identity

`callsign` is normalised to uppercase; a portable suffix such as `/P` or `/MM`
is retained. `grid` accepts 4, 6, or 8 characters. The canonical display is 6
characters with the field uppercase and the subsquare lowercase, for example
`FN31pr`.

`gridSource` defaults to `manual`. When it is `gps`, a station editor may update
the grid from a GPS fix; see §6.8.

### 4.2 Audio roles

Each role is independent. A station may have only Radio RX (receive-only), only
Radio TX (unusual but valid), both, or neither. An absent role is **unassigned**;
it is not an empty string and not a request to use the system default.

The value is a PipeWire `node.name` of a non-stream source or sink. Descriptions
are labels, never identity: a Digirig, an IC-7300 and a SignaLink can all
announce "USB Audio CODEC".

### 4.3 Station paths

`station.paths` is a declaration, never an inference from attached hardware. It
contains every path the operator expects:

- `["host-audio"]` — the host carries radio audio, with or without CAT.
- `["tnc"]` — a modem carries the audio and keys the rig; no host audio, no CAT.
- `["host-audio", "tnc"]` — both. This is two paths, not a third kind.

A station with no declared paths omits `station`. Consumers then treat the
station as unconfigured for path purposes.

### 4.4 TNC transport

A serial transport uses a stable path, preferably under `/dev/serial/by-id/`:

```json
{
  "transport": {
    "kind": "serial",
    "path": "/dev/serial/by-id/usb-...-if00-port0"
  },
  "baud": 38400
}
```

A Bluetooth transport uses a device address:

```json
{
  "transport": {
    "kind": "bluetooth",
    "address": "AA:BB:CC:DD:EE:FF"
  }
}
```

`baud` is meaningful only for a serial transport.

### 4.5 CAT endpoint

```json
{
  "cat": {
    "kind": "rigctld",
    "host": "localhost",
    "port": 4532
  }
}
```

The profile stores the endpoint only. `rigctld` owns the rig model, serial
device, serial speed, PTT wiring, and CI-V address. An absent `cat` section
means the station declares no CAT endpoint; a host-audio app may still work by
manual tuning or its local CAT setting, marked as not following the station.

## 5. Wiring and identity

The profile has two field classes:

| Class | Fields | Who writes |
|---|---|---|
| Wiring | `station`, `audio`, `tnc`, `cat` | Station editor and CLI only. Consumers read. |
| Operator identity | `operator` | Station editor, CLI, or a consumer on an explicit user action. |

A consumer must never write wiring. A device choice made inside an app is not a
station change; if the operator wants it to become one, the app must offer an
explicit "set as station default" action that calls the writer.

Identity writes are last-write-wins. There is no per-field lock, because callsign
and grid are low-frequency, user-initiated choices and a lock would add a failure
mode without protecting anything valuable.

## 6. Behaviour

### 6.1 Following the station

For every role an app uses:

- **Binding exists** — use it and ignore the app's local selection for that role.
- **No binding** — use the local selection and show **not following the station**.
- **Profile missing** — behave as no bindings, and show **station unavailable**.
- **Profile invalid** — keep the last good profile if there is one; otherwise
  behave as no bindings. Report the validation error.

An app must never infer a binding from the hardware it sees.

### 6.2 Configured-but-missing

If a binding exists but the node, transport, or endpoint is absent or unreachable
at runtime, the app must report **configured-but-missing** and stop. It must not
fall back to the system default, to its own local selection, or to the first
matching codec.

This is not pedantry. PipeWire silently links a missing target to the default
source unless `node.dont-fallback` is set, so an app that only checks whether the
stream opened will happily capture the wrong microphone.

A vanished assignment is never rebound automatically. The operator re-picks.

### 6.3 Live switch

Watch the profile directory. On a valid change, apply it without restarting where
possible. If a role's node changed, reopen that stream. If the new profile is
invalid, keep the last good bindings and surface the error; do not apply a
partial profile.

A change that removes a binding switches the app to its local selection, visibly
marked **not following the station**. A change that adds a binding makes the app
follow it.

### 6.4 Station paths and applicability

A consumer declares which station path it requires:

- A host-audio app requires `host-audio`. If the station declares only `tnc`, it
  must not pretend the station is ready for it; it may use a local fallback only
  with the visible **station does not declare host-audio** state.
- A TNC app requires `tnc`; the mirror rule applies.
- A consumer that can use either path declares that, and follows the declared
  paths it can serve.

An app must not widen its own applicability because a device happens to be
plugged in.

### 6.5 CAT

When `cat` is present:

- connect to `rigctld` at `host:port`;
- use the Hamlib NET rigctl protocol (model 2) or the equivalent rigctld command
  set;
- never open the CAT serial device directly.

`rigctld` is started and configured by the operator or a systemd unit. This layer
does not start, stop, or reconfigure it. Managed mode may be added later without
changing clients.

If `rigctld` is unreachable, that is configured-but-missing: report it and do not
silently use a local rig setting or a different port.

`rigctld` has no authentication. The station endpoint should bind to localhost
unless the operator has deliberately exposed it on a trusted network.

### 6.6 TNC transport

A TNC transport is exclusive. An app that opens it owns it until it closes it.
Apps must detect that the transport is already held and report **in use** rather
than stealing it or pretending it is ready.

The project does not yet define an arbitration protocol between two apps that
both want the same TNC; that is tracked as a deferred issue. The profile records
the transport, not a lease.

### 6.7 Grid source

`manual` means the grid is exactly what the operator entered. `gps` means a
station editor may replace it from a GPS fix. The update is opt-in: a station
with `gridSource: manual` is never overwritten.

### 6.8 GPS-to-grid updates

When `gridSource` is `gps` and a fix is available:

- compute a 6-character Maidenhead locator;
- write it through the profile writer;
- leave the grid unchanged when there is no fix; never clear it;
- stop updating if the operator edits the grid manually, which sets
  `gridSource` back to `manual`.

The Station Readiness widget performs the update while it is running. The CLI
provides `omarchy-ham grid --from-gps` for a systemd timer or headless use. There
is no daemon.

## 7. Capability matrix

The matrix is the shared target. An app may implement only its current row, but
it must not invent a different interpretation of a field it does read.

Version 1 defines the target, not the current state. Neither consumer reads the
profile yet: the Station Readiness widget still stores its wiring in `shell.json`,
and JS8 still stores one PortAudio input in its own settings. The matrix is what
each app implements toward.

| Consumer | Reads | Writes | Required path | Notes |
|---|---|---|---|---|
| Station Readiness widget | all wiring, operator identity | wiring, `operator.grid` when `gridSource` is `gps` | declares and grades all declared paths | Readiness only; never changes system defaults. |
| Omarchy JS8 | `operator.callsign`, `operator.grid`, `audio.rx`; later `audio.tx`, `cat` | `operator.callsign`, `operator.grid` when it has a text UI | `host-audio` | RX now; TX and CAT later; never TNC. |
| Future host-audio app | `audio.rx`, `audio.tx`, `cat`, operator | operator identity if it edits it | `host-audio` | Follow §6. |
| Future TNC app | `tnc.transport`, `station.paths`, operator | operator identity if it edits it | `tnc` | JS8 is not this app. |

The RX/TX/CAT notes describe the app's own audio and rig capabilities; profile
integration lands separately in that app's repository.

Rules:

- A consumer must not require fields it does not use.
- A consumer may ignore sections it does not understand, but it must honour
  `version` and the fields it does use.
- Adding a consumer does not change the schema; it changes this table.
- The matrix is updated in the same change that adds or removes a consumer
  capability.

## 8. Implementation guidance

### 8.1 Reading

Prefer the shared library. If an app reads the file directly, it must:

1. read `station.json` as UTF-8 JSON;
2. validate it against the published schema;
3. check `version`;
4. apply field defaults;
5. watch the directory for changes.

Do not cache a partially applied profile. Validate first, then apply.

### 8.2 Targeting a PipeWire node

The contract names a node; it does not prescribe the stream API. The verified
options on Omarchy/PipeWire are:

**Native PipeWire (recommended).** Create a stream with
`PW_KEY_TARGET_OBJECT` (`target.object`) set to the `node.name`, and set
`node.dont-fallback=true` so a missing target is a real error instead of a silent
link to the default. This is the only option that gives clean per-role targeting
and an explicit error path.

**ALSA PCM alias.** Define a per-role PCM in an app-managed ALSA configuration:

```
pcm.js8_rx {
  type pipewire
  capture_node "alsa_input.usb-..."
}
```

Use `capture_node` for Radio RX and `playback_node` for Radio TX. Open the alias
through PortAudio's ALSA host API. This works with the existing PortAudio app, but
adds an ALSA-config dependency and must be managed carefully so a user
`~/.asoundrc` is not lost.

**`PIPEWIRE_NODE` environment variable.** `pipewire-alsa` honours it with a
`node.name` or `object.serial`. It is process-global: it cannot give Radio RX and
Radio TX different targets in one process, and a missing name silently falls back
to the default. It is acceptable only as a temporary single-stream, RX-only
implementation, with a separate node-existence check first.

**JACK host API.** PortAudio's JACK backend exposes nodes by `node.description`,
with unstable indices and a strict graph-rate requirement. Do not use it to bind
a station role.

Always check that the node exists in the PipeWire registry before opening. Do not
rely on the stream open failing.

### 8.3 Independent RX and TX

An app that uses both roles must be able to target them independently, including
simultaneously. A process-global environment variable cannot satisfy this. The
contract therefore requires independent targeting even though JS8's current
RX-only implementation may use a temporary single-stream shortcut.

### 8.4 CAT client

Connect to the endpoint, use the Hamlib NET rigctl protocol (model 2) or the
rigctld command set, and leave the serial device alone. Poll at a modest rate;
`rigctld` is shared and has no authentication.

### 8.5 TNC client

Open the transport, detect holdership, and report in use rather than stealing.
Readiness checks may observe the transport but must not open a KISS dialogue on
it.

### 8.6 CLI

`omarchy-ham` is the reference writer and the integration point for QML apps. It
accepts `--profile <path>` before the command to override the default profile
path.

```
station path
station get [field]
station set <field> <value>
station unset <field>
station validate
station migrate
station watch
audio node-exists <rx|tx> <node>
audio list <rx|tx>
grid --from-gps [--force] [--dry-run]
```

Fields accepted by `get`, `set` and `unset`:

```
operator.callsign      operator.grid       operator.gridSource
station.paths          audio.rx.node       audio.tx.node
tnc.transport          tnc.baud            cat.kind
cat.host               cat.port
```

`station.paths` is comma-separated (`host-audio,tnc`). `tnc.transport` is
`serial:<path>` or `bluetooth:<address>`. `station set` and `station unset`
validate before writing; an invalid edit leaves the profile unchanged.

`station get` with no field prints the stored profile. With a field it prints the
stored value; `cat.host`, `cat.port` and `operator.gridSource` print their
effective defaults. A missing field prints nothing and exits successfully.
`station validate` treats a missing profile as the valid unconfigured state and
exits successfully. `station watch` prints the profile after each valid change
and `{"event":"missing"}` when the profile disappears.

`audio node-exists` exits 0 when the node is present and 1 when it is missing;
it pre-validates against `pw-dump` because PipeWire silently falls back on a
missing target. `grid --from-gps` requires `operator.gridSource: gps` unless
`--force` is passed; `--dry-run` prints the grid without writing it.

Exit codes: 0 success, 1 runtime or validation error, 2 usage error.

## 9. Versioning and migration

- `version` is an integer. The current value is `1`.
- Version 1 is frozen: any change to fields, types, or semantics increments it.
- Consumers refuse a version they do not understand.
- `omarchy-ham station migrate` reads an older profile, writes the current
  version, and keeps a `.bak.<epoch>` backup.
- The schema `$id` is stable:
  `https://raw.githubusercontent.com/yock/omarchy-ham/master/schema/station.schema.json`.

## 10. Security and privacy

- The profile contains a callsign and grid, both public. It contains no secrets.
- File mode is `0644`; directory mode is `0755`.
- `rigctld` has no authentication; keep its endpoint local unless the operator
  deliberately exposes it on a trusted network.
- Writers validate before applying and never interpolate profile values into a
  shell command.
- A malformed profile is an error to report, not a reason to guess.

## 11. Non-goals

- a daemon, D-Bus service, or config broker
- a shared audio engine or stream opener
- starting, stopping, or configuring `rigctld`
- TNC lease arbitration between apps
- dial frequency, mode, or per-app preferences
- logbook or message data
- device auto-binding or changing system default routes
- readiness grading; that belongs to the Station Readiness widget

## 12. Deferred decisions

These are tracked as GitHub issues:

- [#3](https://github.com/yock/omarchy-ham/issues/3) — migrating the widget's
  existing `shell.json` wiring keys to the profile
- [#4](https://github.com/yock/omarchy-ham/issues/4) — TNC serial transport
  arbitration
- [#5](https://github.com/yock/omarchy-ham/issues/5) — JS8 named-node targeting

## 13. Related decisions

- [`docs/adr/0001`](docs/adr/0001-the-profile-is-a-file-contract.md) — file
  contract, not a daemon
- [`docs/adr/0002`](docs/adr/0002-one-writer-path-and-directory-watch.md) — one
  writer path and directory watch
- [`docs/adr/0003`](docs/adr/0003-pipewire-node-name-is-identity.md) — `node.name`
  is identity
- [`docs/adr/0004`](docs/adr/0004-wiring-is-station-writable-identity-is-app-writable.md)
  — who writes what
- [`docs/adr/0005`](docs/adr/0005-interoperability-is-a-capability-matrix.md) —
  capability matrix
- [`docs/adr/0006`](docs/adr/0006-rigctld-is-operator-owned.md) — `rigctld`
  ownership
- [`docs/adr/0007`](docs/adr/0007-gps-grid-updates-are-opt-in.md) — GPS grid
  policy
- [`docs/adr/0008`](docs/adr/0008-v1-is-config-tooling-not-an-audio-engine.md) —
  config tooling, not an audio engine
- [`docs/adr/0009`](docs/adr/0009-the-profile-lives-in-the-omarchy-ham-namespace.md)
  — profile path
- [`docs/adr/0010`](docs/adr/0010-rx-and-tx-are-targeted-independently.md) —
  independent RX/TX
- [`docs/adr/0011`](docs/adr/0011-the-profile-is-versioned-at-the-root.md) —
  versioning
- [`docs/adr/0012`](docs/adr/0012-gps-mode-stays-widget-local.md) — `gpsMode`
  stays widget-local
- [`docs/adr/0013`](docs/adr/0013-the-cli-and-library-are-cpp20-qt6-cmake.md) —
  C++20, Qt 6 Core and CMake
- [`docs/adr/0014`](docs/adr/0014-v1-validation-is-hand-written.md) — hand-written
  v1 validation
