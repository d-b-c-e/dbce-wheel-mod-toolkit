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
All 232-byte sled, 311-byte dash and 324-byte Horizon fields are decoded; numeric
corner order is FL/FR/RL/RR. Return code is nonzero for no packets or bad packets.
A duration/size stop only describes this receiver; it does not prove a whole race
was captured. A packet's mod marker is not proof of which game produced it.

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
