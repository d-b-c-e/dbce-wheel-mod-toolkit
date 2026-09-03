# ADR-0001 — FFB output backends: DirectInput primary, SDL2 secondary, one interface

*Status: proposed 2026-09-03. Decides the shape of `native/wheelffb`.*

## Context

Six force-feedback implementations exist across the family. Four talk to
DirectInput 8 directly (art-of-sim-rally / iracing-arcade-wheel, OutRun2006Tweaks-FFB,
cannonball-se on Windows, xoutput-redux in C#). One uses **SDL2 haptics**
(cruisn-collection, inside a MAME fork, modelled on Cannonball DX and Flycast).
One tried **SDL3 haptics** and removed it (OutRun2006Tweaks-FFB, commit `eddaa28`).

cruisn-collection pivoted on 2026-09-03 from the FFB Arcade Plugin to its own
SDL2 engine on another developer's advice that the approach is better. That
advice (Endprodukt: "don't use the FFB plugin when you work at driver level")
is about **where forces are produced** — in-process from the emulator, with
the game's own motor byte, rather than through an external `dinput8.dll`
hijack with per-game handlers and a fixed effect length — not about SDL versus
DirectInput as APIs. SDL2 was the convenient API in a codebase that already
carries SDL2; the improvement comes from ownership of the signal and lifecycle.

## What is actually true about the two APIs

On Windows, **SDL2 haptics is a wrapper over DirectInput** (`SDL_dinputhaptic.c`;
XInput rumble via `SDL_xinputhaptic.c`). It opens the device itself with
`DISCL_EXCLUSIVE | DISCL_BACKGROUND`, creates `DIEFFECT`s, and maps
`SDL_HapticUpdateEffect` onto `SetParameters`. So SDL2 **cannot be better than
DirectInput at the hardware**: every force it applies goes through the same
calls. The trade is:

| | DirectInput directly | SDL2 haptics |
|---|---|---|
| Portability | Windows only | Windows (DirectInput/XInput), Linux (evdev), macOS |
| Boilerplate | COM, data formats, cooperative levels, effect structs | small C API, device enumeration and hot-plug handled |
| Control | full: cooperative level, own exclusive handle beside a non-exclusive poll handle, effect axis count, `DIEP_*` flags, autocentre timing | none of that is exposed; SDL decides |
| Diagnosability | raw HRESULTs — the way every FFB bug this week was found (`0x80040205`, `0x80040154`, `0x800700AA`) | strings; the underlying HRESULT is lost |
| Recovery | re-acquire / recreate-and-ramp under our control | SDL's, opaque |
| Version risk | the OS API, stable since 2002 | SDL 2.26 changed the joystick GUID layout (cruisn was bitten); SDL3 preview ignored `SDL_UpdateHapticEffect` on MOZA/DD bases (OutRun was bitten) |
| Dependency | none | `SDL2.dll` shipped or already present |
| Proven here | MOZA R12, MOZA R5 (user), Fanatec CSL DD / CSW 2.5 (users) | MOZA R12, Fanatec CSL DD / CSW 2.5 (cruisn testers) |

The measured cruisn work stands on its own: the damper law, the slew and peak
clamp, the hold watchdog and the sign calibration are all real and were done
*through* SDL2. None of them depend on SDL2.

## Decision

`native/wheelffb` exposes **one C-ABI interface** — open by VID:PID or name,
set constant force level (signed, normalised), optional periodic effects,
stop / panic-stop, hold watchdog, capability query, trace logging — and has
**two backends behind it**:

1. **DirectInput** (primary on Windows). Seeded from art-of-sim-rally's DLL
   (the superset), with OutRun's lifecycle rules folded in. This is the
   reference implementation and the one the Unity mods ship.
2. **SDL2 haptics** (secondary). Seeded from cruisn's `mvffb` engine. For
   processes that already carry SDL2 (MAME forks, cannonball) and for any
   future Linux / Steam Deck target. SDL2 is loaded at runtime, never linked,
   so the DLL works without it.

Everything above the backend is shared: signal shaping (gain, invert, clamp,
smoothing, slew, low-speed fade), device identification (VID:PID, FFB-first
ranking, virtual-pad blocklist), sign conventions per brand, the hold
watchdog, the panic-stop on exit, logging and the trace format the
oscillation-verdict tool reads.

**No project pivots for the sake of cohesion.** The Unity mods gain nothing
from SDL2 and would lose HRESULT-level diagnosis. cruisn keeps SDL2 because
its process already owns SDL and the GPL boundary keeps the MAME side separate
anyway; it can adopt the shared shaping/identification code and, later, the
DLL's SDL2 backend.

## How "which is better" gets settled

By measurement, not claim. `tools/ffbprobe` (from the art-of-sim-rally session)
already drives a base with scripted constant-force sequences and records what
was accepted. Extended to run the same sequence through both backends and log
the wheel's position response, it gives a like-for-like comparison on the
owner's R12 — latency, step response, sustained hold, behaviour on focus loss
— recorded in `knowledge/` with the wheel and date. If SDL2 wins on any axis
that matters, this ADR gets a successor.

## Consequences

- One more thing to maintain (two backends), paid for by one place to fix
  lifecycle bugs instead of six.
- cruisn's next FFB bug is fixed in this repo's SDL2 backend, not in
  `midvunit_v.cpp`, once it consumes the DLL. Until then its engine is the
  seed and stays where it is.
- The interface must not leak DirectInput types; `HRESULT`s are surfaced
  through a last-error query so the DirectInput backend keeps its
  diagnosability without shaping the API around it.
