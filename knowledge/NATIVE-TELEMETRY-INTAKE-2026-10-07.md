# Native game telemetry: preserve producer contracts

[dbce-project-mgmt / dbce-wheel-mod-toolkit, 2026-10-07]

The owner requested progress on DiRT 3/4 and Forza Horizon 4/5/6. Exact local
game identities and the DiRT asset findings live in the portfolio hub's
`docs/research/2026-10-07-dirt-forza-intake.md`. The shared code change is the
existing `tools/forza/forza_capture.py`; commands are in `docs/FORZA-CAPTURE.md`.

Three reusable lessons from the native intake:

- **Packet length does not identify its producer.** A 324-byte Horizon packet
  cannot be labelled as a mod solely because byte 323 matches a toolkit marker.
  Native profiles retain that byte without assigning source semantics. Record
  caller-declared game/build, executable hash and sender endpoint separately;
  none authenticates UDP or proves which executable sent a datagram.
- **Shared layout does not imply identical fields.** The current FH6 publisher
  contract defines the 12-byte extension and integer puddle presence flags.
  Preserve the toolkit's historical float-depth interpretation in its own
  profile. FH4/FH5 retain the unnamed extension until their own evidence supports
  meanings. Never silently reinterpret a contradictory capture or discard its
  original bytes. FH6 non-finite collision inputs and incompatible puddle flags
  produce a validation error with raw bytes retained.
- **Native telemetry is not original FFB.** Engine Torque is not signed steering
  force; smashable-object data is not all body contacts. A received packet stream
  neither controls the game nor qualifies deterministic replay. Receive-only
  capture does not mute native FFB or downstream motion. Those outputs need
  separate verified gates before unattended driving.

Primary source, checked October 7:
[FH6 Data Out documentation](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation).
Its frame-rate emission and inactive menus/replay/rewind matter when designing
tests: do not infer a 60 Hz physics tick or receiver failure from title silence.
It reserves receiver ports 5200 through 5300; the explicit FH6 CLI refuses them.

Validation: 14 decoder/socket tests, including native profile loopback captures,
raw/metadata retention, malformed packets, reserved-port refusal and the Art
consumer's actual managed encoder. Only private ephemeral receivers were sent
synthetic fixtures. No game, device, motion destination or native FFB opened.
No native Forza runtime or equal-at-50 acceptance is claimed.

DiRT intake also provides a read-only NeFS helper in the hub. It uses the
upstream library and keeps archive outputs private. Executable-embedded archive
headers must be read as a headless source; missing loose XML does not prove the
feature's data is absent. Conversely, a configuration file alone does not prove
live renderer behavior or camera ownership. Static xref/hash misses cannot
exclude indirect dispatch.
