# 0009 — The profile lives in the omarchy/ham namespace

## Status

Accepted.

## Context

The profile needs a path that every Omarchy ham app can find. Candidates were a
sibling of `shell.json`, a neutral `~/.config/station/`, or a new ham namespace.
Omarchy keeps user config under `~/.config/omarchy/` and generated state under
`~/.local/state/omarchy/`; its own docs reserve `~/.config/omarchy/` for files a
user may version. There is no existing station namespace.

Putting `station.json` next to `shell.json` invites confusion with the shell's
config and couples the station contract to the shell's file. A neutral
`~/.config/station/` is not wrong, but the contract is deliberately
Omarchy-tailored and a future shared component will want a home.

## Decision

The profile lives at:

```
$XDG_CONFIG_HOME/omarchy/ham/station.json
```

with `~/.config/omarchy/ham/station.json` as the fallback. The directory is
`0755`, the file `0644`. The CLI honours `XDG_CONFIG_HOME`; it does not hardcode
`$HOME/.config`.

## Consequences

- The path is discoverable and clearly grouped with ham configuration.
- `ham/` leaves room for future shared files without inventing another top-level
  namespace.
- The widget must migrate its wiring keys out of `shell.json`; widget-only
  preferences such as the refresh interval stay there.
- Users who version `~/.config/omarchy/` get the profile in the same tree.
- A future non-Omarchy consumer can still read the path; the contract is not
  secret.

## Rejected

- **`~/.config/omarchy/station.json`.** Too close to `shell.json` and no room to
  grow.
- **`~/.config/station/station.json`.** Neutral, but loses the Omarchy context
  and creates a second top-level ham namespace later.
- **Inside `shell.json`.** Couples every consumer to the shell's config and
  release cycle.
- **`~/.local/state/omarchy/`.** State is for generated runtime data; the profile
  is user-editable config.
