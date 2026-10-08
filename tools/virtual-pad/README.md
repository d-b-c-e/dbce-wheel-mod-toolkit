# Virtual pad for coordinated offline tests

Build with `dotnet build tools/virtual-pad/VirtualPad.csproj -c Release`.
Starting the executable connects a virtual Xbox controller. Use the shared rig
lease and the consumer's test recipe; this tool does not acquire the lease itself.
No physical wheel-force interface is present.

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
from 0 through 60000. A missing button duration defaults to150 ms. Stick and
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
