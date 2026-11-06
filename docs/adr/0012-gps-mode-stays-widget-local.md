# 0012 — `gpsMode` stays widget-local

## Status

Accepted.

## Context

The Station Readiness widget has a `gpsMode` setting — `Off`, `Optional`, or
`Required` — that says how much GPS timing loss should affect readiness. It is a
judgement about the station's time policy, not a wiring fact. No other current
consumer needs it: JS8 decodes without GPS, and a future app that needs precise
time can read the host clock.

The shared profile is deliberately scoped to wiring and operator identity. Moving
`gpsMode` in would make the profile a partial readiness configuration and couple
consumers to the widget's grading model.

## Decision

`gpsMode` stays in the widget's entry in `~/.config/omarchy/shell.json` in version
1. It is not part of the station profile. If another consumer needs the station's
time policy, a future version can add a `time` section with its own schema and
ADR.

## Consequences

- Station wiring and operator identity have one home; readiness policy has
  another.
- The widget still owns time-source grading.
- A headless consumer cannot ask the profile whether GPS timing is expected; it
  can inspect gpsd directly if it ever needs to.
- The profile is not a complete description of every station setting, which is
  intentional.

## Rejected

- **Move it into the profile now.** No second consumer, and it mixes readiness
  policy into a wiring contract.
- **Duplicate it in both places.** Two sources of truth for one policy.
- **Add a `time` section opportunistically.** Wait for a real consumer and a
  separate decision.
