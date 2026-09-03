# Support bundles — how remote diagnosis actually worked

Every FFB or binding bug from a user this month was solved from a bundle, not
from a conversation. The bundle is the most valuable feature after the mod
itself. Provenance: art-of-sim-rally `SupportBundle.cs`, cruisn-collection
`harness/support_bundle.py`, OutRun upstream crash bundle.

## What to collect

1. **Header**: mod version (not "latest"), game version, OS, engine version,
   generated timestamp. "Folder names lie."
2. **Every setting**, as the running mod sees it — a user's `PreferredDeviceIndex
   = 1` explained a Fanatec's dead FFB in one line.
3. **The input middleware's view of devices**: per joystick — name, recognised
   or not, assigned to the player or not, axis/button counts, hardware id,
   per-axis deadzones. Two `FANATEC Wheel` entries with identical hardware ids
   and 32/144 was the whole Fanatec diagnosis.
4. **The bindings** currently on each device — "nothing bound to any joystick"
   is the most common root cause and the cheapest to state.
5. **DirectInput's view** (the native log): every FFB-capable device with axes
   and buttons, which was chosen and why, effect creation result and axis
   count, `SetParameters` failure counts with the HRESULT, re-acquire events.
   Compare it with (3): the two views disagreeing *is* the finding.
6. **Force statistics**: updates sent, range, peak as % of full scale, sign
   balance — distinguishes "the mod computes nothing" from "the device refuses".
7. **A short FFB/input trace** (5 Hz) when diagnostics were on.
8. **Stale-process check**: a second game process holding the device is the
   "FFB worked once" signature.
9. **Tails of the relevant logs**: mod log, loader log, Unity `Player.log`
   (Rewired's own errors appear only there).
10. For native crashes: minidump + symbolised log, zipped (OutRun's upstream
    handler does this well).

## Write the reading into the bundle

Each section ends with a `READING:` line that states the conclusion in plain
words ("nothing is bound to any joystick; the wheel will not control the car
regardless of force feedback"). Users paste bundles into forums; the reading is
what other people can act on.

## One button, one file

"Create support file on Desktop" in the panel; one text file, timestamped.
Nothing to unzip, nothing to find. Never include game files.

## Analysis tools that turn a bundle into a verdict

cruisn's `ffb_trace_report.py`: bursts/s, peak |force|, direction flips, a
sliding-window oscillation count converted to degrees of wheel travel, and a
verdict with a prescription ("the classic force/position loop — lower FFB
STRENGTH or set slew / peak limit"). Generalise the CSV schema and this is the
toolkit's "why does my wheel oscillate" tool.
