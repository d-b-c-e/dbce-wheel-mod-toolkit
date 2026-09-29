# Sonic Racing: CrossWorlds wheel project

The owner requested a dedicated research/implementation task on 2026-09-28,
coordinated from this toolkit. The goal is usable wheel input, game-informed FFB
and meaningful telemetry, with a mod UI or launcher. Digital wheel controls,
especially the POV hat, must be able to supply right-stick directions.

## Ownership and checkpoints

- Implementation task: **Sonic CrossWorlds wheel support**,
  `01a0eb21-1a99-7320-a305-a752b6064a28`, local host.
- Assigned private repository: `E:\Source\sonic-crossworlds-wheel`. The task owns
  creating it, its source, research notes, tests, installer and documentation.
- Coordinator: this toolkit task, `01a07ac3-e639-7762-a698-5365d447ee7e`.
  Two-way task messaging is authorized by the owner.
- Hourly heartbeat: `sonic-crossworlds-wheel-coordination`, active. Review
  substantive evidence/checkpoints, remain quiet on unchanged status, and pause
  when a usable first candidate or a confirmed blocker needs owner handoff.
- Checkpoints: installed-build evidence; integration choice; first offline
  implementation/build; reversible deployment; live input/telemetry/force checks.

This is a **research candidate**, not a verified toolkit consumer. Neither the
task's creation nor the planned repository establishes working game support.
Inspect its `docs/STATE.md` and `docs/RESEARCH.md` for subsequent progress.

The private [repository](https://github.com/d-b-c-e/sonic-crossworlds-wheel) is
now created and verified private. The task proposes a companion launcher with
calibrated wheel-to-virtual-controller input, followed by a game-state bridge
for telemetry. ViGEmBus availability is a local prerequisite observation, not
proof that this game accepts the output. The coordinator accepted this as an
investigation route, with the following integration gate: v0.15.0's toolkit
reader does not expose POV state, so a separate DirectInput reader must not
silently create a second DirectInput instance alongside toolkit FFB in the
same process. Resolve a single-owner reader/output design before enabling FFB;
separate processes alone do not prove competing wheel handles will coexist.
An additive shared-reader API or a verified HID/RawInput route needs its own
identity, lifecycle and hardware evidence. Input/UI work can proceed meanwhile.

## Initial evidence

The coordinator read the installed Steam manifest on 2026-09-28:

| Item | Observed value |
|---|---|
| Steam app | `2486820`, Sonic Racing: CrossWorlds |
| Installed build | `23540645`, manifest StateFlags `4` |
| Install root | `D:\Program Files (x86)\Steam\steamapps\common\SonicRacingCrossWorlds` |
| Shipping executable | `UNION\Binaries\Win64\SonicRacingCrossWorldsSteam.exe` |
| Root bootstrapper | `SonicRacingCrossWorlds.exe` |

The implementation task reported the shipping executable SHA-256 as
`923FFDEA7265FE20736728AD2045278EAC46ED755D6503C81DDFF3FFFCF7521A`.
It found an Unreal-style Engine/UNION/Paks layout and no existing mod directory;
exact engine version, loader compatibility and input/telemetry hooks remain
unverified at this checkpoint. The
[official Steam listing](https://store.steampowered.com/app/2486820/Sonic_Racing_CrossWorlds/)
states Denuvo Anti-tamper; that does not establish a separate anti-cheat driver.

The [SEGA manual](https://manual.sega.jp/sonicracingcrossworlds/en/) embeds its
content in public `pC.push` data despite showing a browser-support shell to the
page reader. Its Steam tables assign steering, air tricks and flight movement
to the left stick across the listed layouts. Acceleration bindings differ by
layout and brake/reverse uses a button. These are manual observations, not
verification of the owner's bindings or this build's analog behavior.

Keep the requested right-stick mapping. Reconcile the actual actions and add
independent flight vertical control where needed; do not assume that right-stick
remapping alone makes flight usable or that analog pedal readings imply
proportional game acceleration/braking.

## Acceptance priorities

1. Stable wheel/pedal/button/POV identities, useful calibration, save/rebind/cancel
   and disconnect recovery. Preserve steering while using digital stick controls.
2. Four bindable stick directions with bounded diagonals, a defined opposing-input
   rule and neutral on release/focus loss/disconnect/cancel. Verify held versus
   fresh-press/flick semantics; no automatic trick repetition by assumption.
3. Real game-state telemetry with units, frames, timestamps, validity, freshness
   and field provenance. Identify land/water/flight and ownership transitions.
   Label derived fields and leave unavailable RPM/gears/contact data unavailable.
4. FFB based on verified game signals. Distinguish calculation, driver acceptance
   and physical feel. Any fallback must be explicit; controller rumble alone is
   not proof of useful steering-wheel FFB. Stop output on inactive/stale state.
5. Simple setup by default, Advanced tuning, bounded binding groups, compact
   commands and a device dropdown that follows Steering unless explicitly
   overridden. Apply the current toolkit UX guidance and document real exceptions.

Use the official [v0.15.0 artifacts and provenance](../RELEASE-0.15.0.md) where
applicable. The toolkit root has unrelated dirty work and is not a build source
for that frozen release. It does not provide a shared input/UI implementation.
No game physics changes belong here.

The prior shared-desktop pause remains in effect. The task can research and
build offline; game launch, input injection and force testing need a coordinated
slot. No game/device was opened for this kickoff. Preserve installed files and
owner settings; do not bypass DRM/anti-cheat or redistribute game assets.
