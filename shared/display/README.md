# Racing-row selector (vendorable source)

`RacingRow.cs` decides which displays a triple-screen mod spans, from the observed monitor rectangles and the primary
display alone. `WindowsMonitors.cs` reads those rectangles (`EnumDisplayMonitors` + `GetMonitorInfoW`); it is kept
apart so the policy stays pure and testable anywhere. Neither file changes a display setting, profile or window.

The policy (owner rig, 2026-10-13: three 2560x1440 displays at x = -2560, 0, 2560 and a 1920x1080 diagnostic
display at (314, -1080)):

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
blob), keep the MIT header and record the toolkit commit. Do not copy the geometry into a second implementation.
`tests/RacingRow` checks the rules, including the rig's actual layout and a read-only print of the live displays;
`tests/RacingRow/Compat` compiles both files for netstandard2.0.
