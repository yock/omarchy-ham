# 0003 — PipeWire `node.name` is the canonical audio identity

## Status

Accepted.

## Context

A radio codec's description is not identity. A Digirig, an IC-7300 and a
SignaLink can all announce "USB Audio CODEC". `object.serial` is only stable for
one PipeWire daemon run. ALSA card ids change with enumeration and profiles.
PortAudio exposes generic ALSA devices named `pipewire` or `default`, not the
individual nodes. The Station Readiness widget already chose `node.name` as the
identity and never rebinds automatically.

The shared profile must have one binding key. If it stores several identifiers,
they drift, and every consumer has to decide which one is true.

## Decision

A Radio RX or Radio TX binding is a PipeWire `node.name`. It is stored only after
an explicit operator choice. A configured-but-missing name is an error; it is
never rebound, and it never falls back to the system default. Descriptions,
serials, ALSA paths and card ids are diagnostic labels, not binding keys.

## Consequences

- One identity domain, shared by every consumer.
- Apps must resolve or target the node themselves; the profile does not open a
  stream.
- `node.name` can change if a serial-less USB device moves port or an ALSA
  profile changes. The operator re-picks; the station does not guess.
- Pre-validation is mandatory because PipeWire silently links a missing target to
  the default source.
- A future schema can add diagnostic fields without making them binding keys.

## Rejected

- **Description or nickname.** Shared by unrelated codecs and localized.
- **`object.serial`.** Boot-scoped; not durable.
- **ALSA card id or `/dev/snd` path.** Enumeration-dependent and not what
  PipeWire targets.
- **Property-based auto-binding.** Silently picks the wrong one of two identical
  codecs. A wrong binding that turns green is worse than a grey unassigned role.
