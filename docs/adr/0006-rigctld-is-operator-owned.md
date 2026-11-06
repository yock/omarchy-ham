# 0006 — `rigctld` is operator-owned; the profile stores the endpoint

## Status

Accepted.

## Context

Hamlib's `rigctld` is the de facto shared CAT hub on Linux. Clients connect to
`localhost:4532` using the NET rigctl backend (model 2). It has no config file and
no authentication: the rig model, serial device, serial speed, PTT type and PTT
device are command-line options to the daemon. ARDOP's documentation makes the
consequence explicit: when using `rigctld`, clients must not also open the CAT
serial device.

The Station Readiness widget already attaches to an existing `rigctld` and never
starts or reconfigures it. If the station profile duplicated the model and PTT
wiring, two sources of truth would disagree, and the station layer would become a
service manager.

## Decision

The profile stores only the CAT endpoint:

```json
{
  "cat": {
    "kind": "rigctld",
    "host": "localhost",
    "port": 4532
  }
}
```

The operator or a systemd unit owns the `rigctld` process, including the rig
model, serial device, speed, PTT wiring and CI-V address. Consumers attach to the
endpoint and never open the CAT serial device themselves.

If `rigctld` is unreachable, that is configured-but-missing: report it and do not
fall back to a local rig setting or a different port.

## Consequences

- One process owns the exclusive CAT serial device.
- The station layer stays a config contract, not a service manager.
- The operator configures the radio once, in `rigctld`, and every app follows the
  same endpoint.
- A future managed mode can add `cat.managed: true` and start the daemon without
  breaking clients.
- `rigctld`'s lack of authentication is a deployment concern; the endpoint should
  stay local unless deliberately exposed.

## Rejected

- **The station layer starts and configures `rigctld`.** Duplicates Hamlib's
  launch configuration and adds service management to a config contract.
- **Each app starts its own `rigctld`.** The serial device is exclusive; clients
  would fight.
- **Storing rig model and PTT wiring in the profile.** Two sources of truth for
  the same launch arguments.
