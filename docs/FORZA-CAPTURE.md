# Shared Forza wire capture (unreleased)

`tools/forza/forza_capture.py` receives telemetry for any consumer. It does not
load a game, read input devices or issue FFB commands. Python standard library
only. The published v0.12.0 artifact is unchanged.

```powershell
python tools/forza/forza_capture.py --port 8001 --seconds 120 --output curb-wire.jsonl
```

Bind defaults to localhost; use an unused diagnostic port or consumer mirror
instead of competing with SimHub on port 8000. The receiver uses exclusive socket
ownership on Windows. Duration defaults to 120 seconds (maximum 3600), file cap
to 256 MiB (maximum 1024). Output is created exclusively and never overwritten.
It records a header, raw base64 datagrams with fully decoded fields or validation
errors, and a footer with packet/invalid/driving/curb/rumble counts and ranges.
The toolkit profile decodes the standard sled/dash fields in232-byte sled,
311-byte dash and324-byte Horizon packets; Horizon's12-byte extension and final
marker byte are retained in the raw bytes. Numeric
corner order is FL/FR/RL/RR. Return code is nonzero for no packets or bad packets.
A duration/size stop only describes this receiver; it does not prove a whole race
was captured. A packet's mod marker is not proof of which game produced it.

## Native Horizon 4, 5 and 6

Explicit native profiles preserve source labels separately from toolkit markers:

```powershell
py -3 tools/forza/forza_capture.py --profile fh6 --port 8001 --seconds 90 `
  --source-exe '<Steam>/steamapps/common/ForzaHorizon6/forzahorizon6.exe' `
  --source-build 25588841 --output '<new private path>/fh6-native.jsonl'
```

Use `fh4` or `fh5` with their corresponding executable and observed Steam build.
The default `toolkit` profile retains the previous decoder behavior. Python3.11+
is required for optional executable hashing. `producer` in the header contains
the declared profile/build and optional SHA-256/name; each datagram also records
its sender address/port. These are provenance aids, not UDP authentication or
proof that a specific executable emitted a packet. Outputs remain exclusive,
receive-only and bounded. The tool never changes game settings or forwards to
a wheel, dashboard or motion platform.

All native profiles require324 bytes. FH4/FH5 retain the common decoder and
the unnamed extra block; their fields still need qualification on each installed
game. Only `fh6` gives that block the semantics from the
[official FH6 contract](https://support.forza.net/hc/en-us/articles/51744149102611-Forza-Horizon-6-Data-Out-Documentation):
unsigned CarGroup at232, finite SmashableVelDiff at236 and SmashableMass at240.
The profile also decodes integer0/1 WheelInPuddle flags, distinct from the
toolkit's legacy float depth fields. Unexpected values fail validation while
retaining the original bytes; do not silently reinterpret a conflicting live
capture. The native tail byte is recorded without inferring mod/keeper identity.

Data Out is one-way. FH6 documents emission at the game's frame rate during
driving, with no packets during menus, pauses, native replay or rewind. A silent
menu capture does not diagnose a receiver failure. The CLI refuses its reserved
5200..5300 port range. Configure an unused port (8001 here) or an existing mirror;
never take over a live SimHub port. Direct an unattended diagnostic away from
physical motion output, and separately verify native FFB is muted before driving.

This supports signal capture, not in-game route playback. Engine Torque is not
wheel torque. Smashable-object inputs do not establish the complete collision
stream or Art-equivalent crash delivery. Do not feed these exports into an
original-force comparison as if they contained signed steering requests.

October7:14 decoder/socket tests pass, including native profiles on private
loopback sockets, original-byte/peer/executable metadata, malformed-packet and
reserved-port refusals, and the current Art consumer's actual toolkit encoder.
These are synthetic protocol/transport checks; no native Forza game capture,
normalization or physical force acceptance is implied.

## Automated checks

```powershell
python -m unittest discover -s tools/forza -p 'test_forza_*.py' -v
```

The socket integration test skips unless `FORZA_TEST_FIXTURE` points to an actual
DLL-encoded Horizon324 packet. It starts its own bounded child receiver, sends
the fixture only to its ephemeral localhost port, verifies original bytes and
all decoded fields, then checks the footer. Windows child processes are hidden.
To exercise a particular consumer encoder from PowerShell 7:

```powershell
$null = [Reflection.Assembly]::LoadFrom((Resolve-Path '<consumer>/lib/toolkit/dotnet/Dbce.Wheel.Telemetry.dll'))
$frame = [Dbce.Wheel.Telemetry.TelemetryFrame]::new()
$frame.IsRaceOn = $true
$frame.Speed = 12.5
$frame.SurfaceRumble = [Dbce.Wheel.Telemetry.WheelValues]::new(.1,.2,.3,.4)
$bytes = [Dbce.Wheel.Telemetry.ForzaPacket]::CreateBuffer()
[Dbce.Wheel.Telemetry.ForzaPacket]::Write([ref]$frame, $bytes)
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('forza-' + [guid]::NewGuid() + '.bin')
[IO.File]::WriteAllBytes($fixture, $bytes)
$env:FORZA_TEST_FIXTURE = $fixture
python -m unittest discover -s tools/forza -p 'test_forza_*.py' -v
```

Six tests passed on September 12 with the iRacing Arcade consumer's v0.12.0 DLL.
These prove decoder/transport behavior. Curb detection and actual shaker feel
require a game capture and an attended consumer test.
