# 0007 — GPS grid updates are opt-in and manual-overridable

## Status

Accepted.

## Context

Callsign and grid are now shared operator identity. The station already has access
to GPS timing through gpsd, so a fix can also produce a Maidenhead locator. Always
overwriting the grid from GPS would destroy a deliberately entered grid, and a
wrong or stale fix would silently move the operator's location. Never offering the
update wastes a source that is already present.

## Decision

The profile carries `operator.gridSource`:

- `manual` — the grid is exactly what the operator entered. Default.
- `gps` — a station editor may replace the grid from a GPS fix.

When `gridSource` is `gps` and a fix exists, compute a **6-character** Maidenhead
locator and write it through the profile writer. With no fix, leave the grid
unchanged; never clear it. A manual edit sets `gridSource` back to `manual`.

The Station Readiness widget performs the update while it is running. The CLI
provides `omarchy-ham grid --from-gps` for a systemd timer or headless use. There
is no daemon.

## Consequences

- The operator opts in once and can take the grid back by editing it.
- GPS loss does not erase a grid.
- Six characters is the usual JS8 grid and is precise enough for the mode;
  4 is too coarse and 8 is false precision for a GPS fix.
- The widget must know how to read gpsd and how to write the profile; the CLI
  covers the case where the widget is not running.
- A future station with no GPS simply leaves `gridSource` at `manual`.

## Rejected

- **Always overwrite from GPS.** Surprising and destructive.
- **Never update automatically.** Wastes an available fix and makes the operator
  transcribe coordinates.
- **4-character default.** Too coarse for a station locator.
- **A daemon to watch GPS.** The widget and a timer cover the real cases.
