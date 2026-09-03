# Unattended testing of a game under a mod

How to drive a game from a script well enough to verify a mod without a human
at the keyboard, and where that stops. Provenance in brackets.

## Keyboard injection: it depends on how the game reads the keyboard

| Game / input stack | OS `SendInput` | Works via |
|---|---|---|
| art of rally (Unity 2019, Rewired Raw Input) | **yes** — probe saw every injected key at Unity and Rewired level | `tools/dev/press.ps1` |
| iRacing Arcade (Unity 6, Input System) | **no** — Raw Input drops events with no device handle | in-process: `StateEvent.From(keyboard)` + `InputSystem.QueueEvent`, fed by a command file (`DevInput`) `[iracing-arcade-wheel]` |
| MAME | **no** — `keybd_event` never reaches its rawinput | `ioport_field:set_value()` from a Lua plugin `[cruisn-collection]` |
| IMGUI mouse (any Unity) | **yes** — IMGUI takes the mouse from window messages | `mouse.ps1`, window visible and in front |

`SendInput` details that cost hours: the x64 `INPUT` struct is **40 bytes**
(`[StructLayout(Explicit, Size=40)]`, `KEYBDINPUT` at offset 8) or the call
fails with error 87; use scan codes (`KEYEVENTF_SCANCODE`); `FindWindow` by
title fails for Unity — use the process's `MainWindowHandle`, `ShowWindow(9)`
and `SetForegroundWindow`. `[art-of-sim-rally, 2026-09-03]`

## Process control

- **`Stop-Process` from a PowerShell spawned by Git Bash does not stop the
  game**; the same call from a native PowerShell does. Spawning `powershell`
  from Bash also opens a console that steals the foreground and minimises an
  exclusive-fullscreen game. `[iracing-arcade-wheel, art-of-sim-rally]`
- Launch via `steam://rungameid/<appid>`; poll for the process; the title
  screen takes 20–40 s.
- **Deploy only when the game is closed.** DLLs are locked while it runs; a
  copy that "succeeds" over a running game is the stale build you then test.
- **Never hard-kill a game with FFB live** (stranded torque). Prefer `WM_CLOSE`.
- A zombie game process holding the haptic device is the "FFB worked once per
  session" signature — count processes before launching. `[cruisn-collection]`

## Seeing the screen

- The game may run on a **second monitor**; a primary-screen capture will not
  show it. `[art-of-sim-rally]`
- GDI capture of a GL overlay renders **black**; capture inside the process
  (`MIDV_GL_SNAP`) or read state from logs instead. `[cruisn-collection]`
- Exclusive fullscreen minimises on any focus loss; Unity's Input System
  disables the keyboard while unfocused (`backgroundBehavior`) — set
  `IgnoreFocus` for dev builds only.

## Verify from logs, not from feel

- Make the mod log the quantities it acts on at a low rate (5 Hz `FFB trace`
  with speed, steer, slip, forces, output). The Pacejka reversal and the
  OutRun dead-steer input were both visible only this way.
- Count hook invocations (`prefix calls / 2 s`) before believing a patch is
  live — IL2CPP inlining makes dead hooks that patch cleanly.
- Probe both layers when input "does not work": Unity's `Input.anyKeyDown`
  versus the input middleware's view, per keypress. Menu death with input
  flowing at every layer is how the Rewired reset was diagnosed.
- Rewired's own errors and Unity exceptions are only in `Player.log`
  (`%USERPROFILE%\AppData\LocalLow\<Company>\<Game>\Player.log`); Unity Mod
  Manager's log is `<game>_Data\Managed\UnityModManager\Log.txt`; BepInEx
  truncates `LogOutput.log` itself at startup — never delete it from tooling.

## Menus and timing

- Menus drop a second key within ~1 s of the first; space presses out.
- Wait for the title screen before injecting; "press any key" logic often
  needs a 0.2 s delay after appearing.
- Don't steal focus from the desktop while the owner is using the machine —
  their clicks steal it back and the test silently fails. Ask them to press
  the keys instead when they are present; a human is the better probe.

## Measurement discipline

- Four decimal places on any "matches" claim. `[cruisn-collection]`
- Record the measurement next to the code that depends on it.
- A support bundle from a user beats a theory: their controllers section is
  the middleware's view, their ffb.log is DirectInput's. Compare them.
