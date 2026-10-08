# Virtual pad for coordinated offline tests

Build with `dotnet build tools/virtual-pad/VirtualPad.csproj -c Release`.
Starting the executable connects a virtual Xbox controller. Use the shared rig
lease and the consumer's test recipe; this tool does not acquire the lease itself.
No physical wheel-force interface is present.

Wait for **neutral readback verified** in the log before launching a game or
sending commands. Startup checks the controller's actual XInput user index and
requires three consecutive complete neutral states. A failure disconnects the
controller within the two-second admission window. `--slot` chooses the command
directory; it does not force an XInput user index.

Commands go one at a time into `%LOCALAPPDATA%/dbce/vpad/cmd.txt`; the tool
consumes the file. `--slot N` selects a separate `vpad-N` directory for N=1..3.

```
press A ms=200
press A 200
stick L -0.35 0 750
trigger R 0.4 1000
quit
```

Both duration spellings work. Explicit durations must be whole milliseconds
from 0 through 60000. A missing button duration defaults to150 ms; explicit
`press A 0` sends down then up immediately, without that default delay. Stick and
trigger duration0 or omission retains the value until another command; use a
positive duration for bounded tests. Commands execute sequentially: a timed
command blocks later commands until its release. Exiting removes the controller.
Inspect current game state before each input; file consumption is not game
acceptance. The command log records completion, after the timed release.

## October 8 duration correction

The earlier parser advertised `press A ms=200` but passed `ms=200` to integer
parsing, silently falling back to150 ms. Historical logs echoing that command
do not prove200 ms. Numeric stick/trigger durations were unaffected. The new
parser accepts the documented spelling and rejects malformed/negative/excessive
explicit durations before mutating controller state. It does not change axis,
button or virtual-controller mappings.

`dotnet run --project tools/virtual-pad/tests/Duration.Tests.csproj -c Release`
checks the production parser without referencing ViGEm or accessing devices.
The 16 checks and Release build pass; live timing remains a separate check.

## October 8 neutral startup correction

The rig's ViGEmBus initially reported small stick offsets even though the
managed controller report was zero. Repeated reads retained packet 1 and
LX/LY/RX/RY = -3356/-1869/-3255/-848; a zero-only submission did not clear them.
These bytes match the driver's initial USB blob, while its internal cache
starts at zero and discards identical submissions. See the upstream
[initial blob and SubmitReportImpl](https://github.com/nefarius/ViGEmBus/blob/master/sys/XusbPdo.cpp).

When neutral readback is unavailable or nonzero, startup submits one signed
integer unit on LX, then a complete neutral report. This may repeat while the
user index is unavailable. No button or trigger is pressed; a game polling
during admission could see that one-unit flicker, so runners must await the
readiness log. A submit return alone is never treated as neutral input.

Fifteen production-policy checks cover all state fields, delayed index,
interrupted neutrality, invalid index and timeout. Three independent fresh
virtual-controller starts then read exactly neutral through XInput and exited
0, with no game running. Evidence is private under
`_archive/2026-10-08/vpad-neutral-1646`. This does not explain Sonic motion after
earlier menu-button reports; input acceptance by a game remains a separate check.

```powershell
dotnet run --project tools/virtual-pad/tests/Neutral.Tests.csproj -c Release
```
