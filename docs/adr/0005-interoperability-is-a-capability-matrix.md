# 0005 — Interoperability is a capability matrix

## Status

Accepted.

## Context

JS8 is a receiver today: one PortAudio input, no transmit, no CAT, no TNC. It will
grow transmit and CAT later, and it will never consume a TNC transport because it
is itself the modem. Other apps will arrive with different capabilities. If
"interoperability" means "every app understands every field", the contract waits
for the slowest app and every consumer implements dead code.

If it means prose in each app's README, each app drifts toward its own
interpretation and nobody can tell whether the contract is actually followed.

## Decision

`SPEC.md` publishes a capability matrix. Each consumer states:

- which fields it reads;
- which fields it writes;
- which station path it requires;
- any explicit non-capability.

JS8 reads `operator.callsign`, `operator.grid` and `audio.rx` now; it will read
`audio.tx` and `cat` later; it never reads `tnc`. The Station Readiness widget
reads all wiring and identity, writes wiring, and updates the grid when
`gridSource` is `gps`.

A consumer whose required path is not declared must warn rather than pretend the
station is ready for it.

## Consequences

- Features can land independently without breaking the contract.
- The matrix is the shared target to develop toward.
- Every capability change updates the matrix in the same change.
- A host-audio app on a TNC-only station gets a clear warning instead of a
  confident green.
- No machine-readable per-app capability manifest in version 1; documentation is
  enough for two consumers.

## Rejected

- **Every app consumes every field.** Forces dead code and blocks on the slowest
  consumer.
- **Prose per app.** No shared target and no way to audit drift.
- **Waiting for JS8 to have TX/CAT/TNC.** The contract would be designed without
  its second real consumer.
