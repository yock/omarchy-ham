# Omarchy Ham

Shared station profile, schema and tooling for Omarchy ham radio apps.

**Status:** version 1 contract implemented. The `omarchy-ham` CLI and the
`omarchy_ham` C++/Qt library read, write, watch and validate the profile. The
consumer apps are not migrated yet; the capability matrix is their target.

## Why

Ham apps on one machine keep rediscovering the same station: which PipeWire node
is the radio's receive audio, which is transmit, which serial device is the TNC,
which `rigctld` is the rig, and what callsign and grid the operator uses. Each app
stores its own answer, in its own format, and they drift.

This repository defines one answer:

- a versioned JSON **station profile** at
  `$XDG_CONFIG_HOME/omarchy/ham/station.json`;
- a **JSON Schema** that makes typos and unknown versions fail loudly;
- a **CLI** that is the reference writer and the integration point for QML apps;
- a **C++/Qt library** that reads, writes, watches and validates the profile.

PipeWire remains the audio runtime, and `rigctld` remains the CAT runtime. The
profile only records which nodes and endpoints the operator selected.

## The contract

- [`SPEC.md`](SPEC.md) — field semantics, behaviour rules, capability matrix and
  implementation guidance.
- [`schema/station.schema.json`](schema/station.schema.json) — the machine-readable
  schema.
- [`GLOSSARY.md`](GLOSSARY.md) — the vocabulary the contract uses.
- [`docs/adr/`](docs/adr/) — the decisions behind the contract.

A minimal profile:

```json
{
  "version": 1,
  "operator": {
    "callsign": "N0CALL",
    "grid": "FN31pr"
  },
  "station": {
    "paths": ["host-audio"]
  },
  "audio": {
    "rx": {
      "node": "alsa_input.usb-C-Media_Electronics_Inc._USB_Audio_Device-00.mono-fallback"
    }
  }
}
```

## Using the CLI

```bash
# Where is the profile?
omarchy-ham station path

# Pick the two radio audio roles.
omarchy-ham station set audio.rx.node alsa_input.usb-codec-00.mono-fallback
omarchy-ham station set audio.tx.node alsa_output.usb-codec-00.analog-stereo

# Declare the station paths and the CAT endpoint.
omarchy-ham station set station.paths host-audio,tnc
omarchy-ham station set cat.kind rigctld
omarchy-ham station set cat.host localhost
omarchy-ham station set cat.port 4532

# TNC transport: serial or Bluetooth.
omarchy-ham station set tnc.transport serial:/dev/serial/by-id/usb-...
omarchy-ham station set tnc.baud 38400

# Operator identity.
omarchy-ham station set operator.callsign N0CALL
omarchy-ham station set operator.grid FN31pr
omarchy-ham station set operator.gridSource gps

# Read, validate, watch.
omarchy-ham station get
omarchy-ham station get audio.rx.node
omarchy-ham station validate
omarchy-ham station watch

# Check PipeWire presence before opening a stream.
omarchy-ham audio node-exists rx alsa_input.usb-codec-00.mono-fallback
omarchy-ham audio list rx

# Update the grid from a GPS fix.
omarchy-ham grid --from-gps
```

`station get` prints stored values; `cat.host`, `cat.port` and
`operator.gridSource` print their effective defaults. `station set` and
`station unset` validate before writing, so an invalid edit leaves the profile
unchanged.

## Building

Requirements: a C++20 compiler, CMake 3.20 or newer, Qt 6.2 Core, and Python 3
for the CLI integration test.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Installed consumers can use either:

```cmake
find_package(omarchy_ham CONFIG REQUIRED)
target_link_libraries(app PRIVATE omarchy_ham::omarchy_ham)
```

or:

```bash
pkg-config --cflags --libs omarchy-ham
```

## Who consumes it

| Project | Role |
|---|---|
| [Station Readiness](https://github.com/yock/omarchy-station) | Bar widget; edits station wiring and grades readiness. |
| [Omarchy JS8](https://github.com/yock/omarchy-js8) | JS8 receiver; follows Radio RX now, Radio TX and CAT later. |

The capability matrix in [`SPEC.md`](SPEC.md#7-capability-matrix) is the
authoritative list of what each app reads and writes.

## Repository layout

```
cli/             omarchy-ham command-line tool
include/omarchy_ham/  public library headers
src/             library implementation
schema/          JSON Schema for the profile
examples/        Valid example profiles
docs/adr/        Architecture decisions
docs/agents/     Agent workflow conventions
tests/           C++ unit tests, CLI integration test, docs checks
SPEC.md          The contract
GLOSSARY.md      Domain language
```

## Development checks

```bash
python3 -m pip install jsonschema
python3 tests/validate_schema.py
python3 tests/check_links.py
```

CI runs those checks and the C++/CLI tests on every push and pull request.

## License

MIT. See [`LICENSE`](LICENSE).
