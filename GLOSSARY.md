# Omarchy Ham

The shared station contract for Omarchy ham radio apps.

## Language

**Station profile**:
The versioned JSON document that records station wiring and operator identity,
shared by every app that follows the station. _Avoid_: config, settings, station
file.

**Wiring**:
The station-level bindings: Radio RX and Radio TX, declared station paths, TNC
transport, and the CAT endpoint. Wiring is written by the station editor, never
by a consumer app. _Avoid_: devices, setup, configuration.

**Operator identity**:
The operator's callsign, Maidenhead grid, and grid source. Identity is not
wiring: an app may write it on an explicit user action. _Avoid_: profile, user
settings, station identity.

**Station editor**:
The component that writes wiring: the Station Readiness panel or the
`omarchy-ham` CLI. _Avoid_: config UI, settings app.

**Consumer**:
An app that reads the profile and follows the bindings it understands. _Avoid_:
client, subscriber.

**Follow the station**:
Use the profile's binding for a role instead of the app's local selection.
_Avoid_: auto-detect, sync.

**Not following the station**:
Use the app's local selection because the profile has no binding for that role.
The app must say so visibly; it is not an error. _Avoid_: fallback, default.

**Configured-but-missing**:
A binding exists in the profile, but the node, transport, or endpoint is absent
or unreachable at runtime. An error, never a reason to fall back. _Avoid_: lost
device, disconnected.

**Unassigned**:
No binding exists for a role. Consumers may use local settings, marked as not
following the station. _Avoid_: empty, disabled.

**Profile path**:
`$XDG_CONFIG_HOME/omarchy/ham/station.json`, defaulting to
`~/.config/omarchy/ham/station.json`. _Avoid_: config path, settings file.

**Profile version**:
The integer `version` at the root of the profile. Version 1 is frozen; a change
to fields, types, or semantics requires a new version. _Avoid_: schema version,
format revision.

**Single writer path**:
One implementation performs read-modify-write: the CLI and library share it,
take an advisory lock, write a temporary file in the same directory, and rename
it into place. Apps do not write JSON by hand. _Avoid_: config service, daemon.

**Directory watch**:
Watching the directory that contains the profile, rather than the file inode, so
an atomic rename is observed and the watcher can re-read. _Avoid_: file watch,
polling.

**Live switch**:
Re-reading and applying a changed profile without restarting the app. _Avoid_:
hot reload, refresh.

**Radio RX**:
The receive role in radio audio: the PipeWire source a digital-mode app captures
the rig's audio from. A role, not a device. _Avoid_: input, microphone, capture
device.

**Radio TX**:
The transmit role in radio audio: the PipeWire sink a digital-mode app plays its
tones into. A role, not a device. _Avoid_: output, speaker, playback device.

**Node name**:
The PipeWire `node.name` that identifies a Radio RX or Radio TX binding. The
canonical audio identity; never a description, serial, or ALSA card id. _Avoid_:
device name, node id.

**Station path**:
One complete way the host reaches the transceiver: **Host-audio path** or **TNC
path**. A station declares every path it expects. _Avoid_: mode, profile,
station class.

**Host-audio path**:
The path where the host carries the audio: a Radio RX role and a Radio TX role,
with CAT as the convenience that comes with it. _Avoid_: soundcard mode, audio
setup.

**TNC path**:
The path where a modem carries the audio and keys the rig itself, so no host
audio role and no CAT link exist. _Avoid_: hardware TNC, KISS mode.

**TNC transport**:
The link from the host to a modem: a serial device or a Bluetooth device.
_Avoid_: TNC port, modem device.

**CAT endpoint**:
The `rigctld` host and port the station uses for rig control. The endpoint only;
the rig model, serial speed, and PTT wiring belong to `rigctld`. _Avoid_: CAT
config, rig settings.

**Grid source**:
Whether the Maidenhead grid is `manual` or may be updated from a GPS fix.
_Avoid_: grid mode, GPS grid.

**Capability matrix**:
The table in `SPEC.md` that says which profile fields each consumer reads and
writes, and which station paths it requires. _Avoid_: compatibility list,
support table.
