# Game-mod feasibility research

Reports distinguish existing features, static code/configuration evidence,
public-source reports, proposed implementation routes and actual runtime tests.
None of these feasibility studies establishes a working mod or tested wheel feel.

| Assessment | Games | Findings |
|---|---|---|
| 2026-09-07 | Horizon Chase Turbo, Horizon Chase 2, Slipstream, Rush Rally Origins, Rush Rally 3, V-Rally 4 | [Combined summary](2026-09-07/SUMMARY.md), [four detailed reports](2026-09-07/README.md) |
| 2026-09-12 | Super Woden: Rally Edge | [Full assessment](2026-09-12/super-woden-rally-edge.md) |
| 2026-09-16 | art of rally, Super Woden: Rally Edge, #DRIVE Rally, iRacing Arcade | [Separate VR mods and cockpit feasibility](2026-09-16/vr-feasibility.md) |

**September 12 assessment:** Super Woden: Rally Edge belongs near the top of the
new-mod shortlist alongside Turbo. Static inspection found Unity 6 IL2CPP with
supported metadata parsing, analog control fields/signatures, Rigidbody and
front/rear WheelCollider data, and native configurable camera presets. A hidden
chase view exists in community documentation; bonnet rendering and native wheel
activation remain untested. Start with one diagnostic/input/camera proof before
committing to a release. [dbce-wheel-mod-toolkit, 2026-09-12]

The original six-game assessment remains dated to 2026-09-07. Its conclusion that
RR3 and V-Rally already provide native input/FFB/bonnet features is separate from
testing those features on the owner's mixed rig. Telemetry remains their main
proposed addition.
