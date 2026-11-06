# 0011 — The profile is versioned at the root

## Status

Accepted.

## Context

The profile will be read by apps that ship on different schedules. A widget update
may land before a JS8 update, or the reverse. Without a version, a consumer cannot
tell whether an unfamiliar field is a typo, a newer feature, or a different
meaning. Silently ignoring unknown fields hides drift; guessing at a newer format
is worse.

## Decision

The root object carries an integer `version`. The current value is `1`. Version 1
is frozen: any change to fields, types, or semantics increments the version.

Consumers refuse a version they do not understand and tell the operator to upgrade.
They must not partially apply a newer profile. The CLI provides
`omarchy-ham station migrate`, which reads an older profile, writes the current
version, and keeps a `.bak.<epoch>` backup.

The schema `$id` is stable:

```
https://raw.githubusercontent.com/yock/omarchy-ham/master/schema/station.schema.json
```

## Consequences

- A consumer can make a clear decision before touching any field.
- New fields require a deliberate version bump and a migration, not an accidental
  schema drift.
- A newer profile on an older app is a visible error, not a silent misread.
- The strict schema catches typos within a known version.
- Early in the project, version bumps are cheap; the contract can still grow.

## Rejected

- **No version.** The consumer cannot distinguish a typo from a future field.
- **Semantic version strings.** More expressive than needed for a config file;
  an integer is enough.
- **Additive fields within version 1.** The strict schema would reject them in
  old consumers, and loosening the schema loses typo detection.
- **Ignoring unknown fields without a version check.** Drift becomes invisible.
