# Omarchy Ham

Shared station profile, schema and tooling for Omarchy ham radio apps.

**Status:** spec v1 draft. This repository currently contains the contract and
schema; the CLI and library land after review. There is no executable code yet.

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
- a thin **C++/Qt library** that reads, writes, watches and validates the profile.

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

## Who consumes it

| Project | Role |
|---|---|
| [Station Readiness](https://github.com/yock/omarchy-station) | Bar widget; edits station wiring and grades readiness. |
| [Omarchy JS8](https://github.com/yock/omarchy-js8) | JS8 receiver; follows Radio RX now, Radio TX and CAT later. |

The capability matrix in [`SPEC.md`](SPEC.md#7-capability-matrix) is the
authoritative list of what each app reads and writes.

## Repository layout

```
schema/          JSON Schema for the profile
examples/        Valid example profiles
docs/adr/        Architecture decisions
docs/agents/     Agent workflow conventions
test/            Schema and documentation checks
SPEC.md          The contract
GLOSSARY.md      Domain language
```

`cli/` and `lib/` are reserved for the implementation once the contract is
reviewed. The CLI/library toolchain is itself a deferred decision.

## Development

Validate the schema and examples:

```bash
python3 -m pip install jsonschema
python3 test/validate.py
python3 test/check_links.py
```

CI runs the same checks on every push and pull request.

## License

MIT. See [`LICENSE`](LICENSE).
