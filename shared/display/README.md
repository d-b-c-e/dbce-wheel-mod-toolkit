# Racing-row selector (vendorable source)

`RacingRow.cs` decides which displays a triple-screen mod spans, from the observed monitor rectangles and the primary
display alone. `WindowsMonitors.cs` reads those rectangles (`EnumDisplayMonitors` + `GetMonitorInfoW`); it is kept
apart so the policy stays pure and testable anywhere. Neither file changes a display setting, profile or window.

Three racing screens plus a dashboard or other auxiliary display is a common sim-racing setup, not a one-off: the
owner's rig (2026-10-13) has three 2560x1440 displays at x = -2560, 0, 2560 and a 1920x1080 display at (314, -1080),
which is the fixture. The selector reports the observed racing row only. It is distinct from auxiliary screens and
from any profile geometry a consumer has stored, and a consumer never rewrites a saved rig choice from it
automatically.

The policy:

- **Surround** (the mods' existing wide-primary path). A primary at least 2.9:1 wide is used as one surface: the row
  is the primary alone, and no side display is ever added to it. This is a shape test, not proof that the driver has
  grouped displays (NVIDIA Surround, AMD Eyefinity).
- **Separate.** Otherwise the row is the primary plus exactly one display touching each side, with the same width,
  height, top and bottom. Displays wholly above or below the row, such as the diagnostic display, are ignored.
- **No row** (`None`, with a reason): a missing side, a gap, an overlap, a vertical offset or a different size on a
  side; two candidates for a side; any other display sharing the row's band, including a fourth panel in the same row;
  not exactly one primary. The policy never picks another triple and never spans more than three displays.

This replaces the mods' earlier test, which took the virtual-desktop rectangle as the span and failed once the desktop
was taller than the primary. A mod adopting it spans `RacingRow.X/Y/Width/Height` when `Kind == Separate`, keeps its
Surround path for `Surround`, and does nothing for `None` (log `Reason`).

Consumers copy the files byte-identically (`git rev-parse <commit>:shared/display/RacingRow.cs` equals the consumer's
blob), keep the MIT header and record the toolkit commit. Do not copy the geometry into a third implementation.
`tests/RacingRow` checks the rules, including the rig's actual layout and a read-only print of the live displays;
`tests/RacingRow/Compat` compiles both files for netstandard2.0.

**Native mods (C++).** `dbce_racing_row.hpp` is the C++ counterpart of `RacingRow.cs`: the same rules and the same
reason sentences, header-only C++17, with no Windows calls. `dbce_windows_monitors.hpp` is the counterpart of
`WindowsMonitors.cs`. The two implementations are kept equal by `racing-row-vectors.txt`. Every vector gives the
monitors, the expected row and the exact reason, and both test harnesses run every vector they can.
`tests/RacingRow` runs the vectors that use the 2.9:1 rule. `tests/RacingRow/Native/build_test.ps1` runs all of them
with MSVC x64 and x86 as C++17, and x64 as C++20, with warnings as errors. A rule change updates both files and the
vectors together.

The C++ selector takes the wide-primary aspect from its caller, `select_racing_row(monitors, aspect)`.
`kSurroundAspect` (2.9) is the toolkit's rule. A mod that already has another rule, such as the 4:1 rule in the Sonic
CrossWorlds renderer and ADRIFT, passes it explicitly, so adopting the row changes only its separate-display test.
Native consumers copy both headers byte-identically, like the C# files, and record the toolkit commit and blob ids.

`SpanTarget.cs` is the optional shared cached decision for consumers that check the span every frame. It preserves
the saved span opt-out and triple-mode gate, selects only a `Separate` row, and reports a changed reason for logging.
The injected observation and monotonic clock make it engine independent. Observations, including failures, are
cached for two seconds; a failed refresh drops the old span. Window creation, size application and placement remain
in the game adapter. Extracted without behavior changes from Woden `a58f8d9`; the frozen Woden package still contains
its original equivalent class. DRIVE adopts this toolkit source rather than another private copy.

Extraction validation, 2026-10-13: source review confirmed the Woden decision logic is unchanged. The combined
selector/cache harness passed 159 assertions and read the active four-display rig as the expected 7680x1440 row at
(-2560,0); the netstandard2.0 compatibility build passed without warnings. Consumer window placement and rendering
still require consumer evidence. A transient failed query is cached too: a one-shot startup caller may keep its
normal window if it runs during that failure interval, rather than using an old or invented span.
