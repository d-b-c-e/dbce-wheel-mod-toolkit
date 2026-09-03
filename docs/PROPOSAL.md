# Proposal: dbce-wheel-mod-toolkit

*Written 2026-09-03 from a survey of the racing-mod repos. Decision pending.*

## The question

Five projects now do variations of "add force feedback, telemetry and wheel
support to a racing game that lacks them", and every one is chasing the same
class of binding and FFB bugs separately. Can the shared knowledge and code be
pulled into one repository that fixes those bugs in one place and bootstraps
the next game?

**Answer: yes for a well-bounded core, no for a grand framework.** The
game-side hooks — how you get speed, steer and slip out of a particular
engine — are irreducibly per game. Everything on the *other* side of that
seam (device → forces, frame → telemetry, device identification, diagnostics,
installers, knowledge) is shared today only by copy-paste, and the copies have
already drifted.

## What exists (survey, 2026-09-03)

| Repo | Game / engine | Loader | FFB output stack | Telemetry | Input |
|---|---|---|---|---|---|
| art-of-sim-rally | art of rally, Unity 2019 Mono | Unity Mod Manager + Harmony | native `UnityForceFeedback.dll` (DirectInput constant force, exclusive) | Forza 324 B (C#) | Rewired fixes + direct DirectInput read (`WheelInput`) |
| iracing-arcade-wheel | iRacing Arcade, Unity 6 IL2CPP | BepInEx 6 + Harmony | **same DLL, copied and diverged** (783 vs 937 lines) | Forza 324 B (C#, **byte-identical copy**) | Unity Input System kit (rebinder, binding store, processors, HID layouts) |
| cruisn-collection (+ mame-src) | Cruis'n USA/World/Off Road/Exotica, MAME fork | source fork, env-var gated | SDL2 haptics constant force, inside MAME (GPL side) | Forza 324 B (C++, 8 fields) + JSON UDP | MAME ctrlr XML from a glfw/RawInput/DirectInput wizard |
| OutRun2006Tweaks-FFB (+ outrun2006-redux) | OutRun 2006, native x86 | `dinput8.dll` proxy + safetyhook | DirectInput native, constant + 2 periodics, own exclusive handle | Forza **311 B** (C++) + shared memory | DirectInput remap, 3 slots, H-pattern synthesiser |
| milestone-wheel-tools | Gravel (UE 4.17) | `dinput8.dll` proxy | **consumes** the game's FFB (vtable tap); SIXDOF→DRIVING re-type | Forza 232/311/324 (C++) from FMOD + UE4 reflection | game's own DirectInput, tapped; PowerShell bind wizard |

Adjacent and load-bearing: `cannonball-se` (SDL2 FFB direction/range fix),
`mame-racing` (SDL2 haptics in the MAME OSD), `xoutput-redux` (DirectInput
FFB routing, .NET), `dinputto8`, `launchbox/devreorder-bridge` and
`Launchbox-Racing` (two more dinput8 proxies), `launchbox-ez-config`
(DirectInput detection, FFB plugin config writer), `wheel-db` /
`controller-input-db` (per-game wheel data), `dbce-retro-gaming-mcp`
(hardware knowledge JSON), `dbce-mod-toolkit` (engine fingerprinter, loader
backends, dependency installer).

## Duplication, counted

| Thing | Copies | State |
|---|---|---|
| DirectInput constant-force FFB implementation | 6 | art-of-sim-rally (937), iracing-arcade-wheel (783, same origin), OutRun (1,540 incl. periodics), cannonball-se (378, also Linux evdev), milestone (consumer-side tap), xoutput-redux (C#); each re-derives effect creation, acquisition and recovery |
| Forza "Data Out" encoder | 5 (C# ×2 identical, C++ ×3) | 324 B in art of rally, iRacing, cruisn and the MAME driver itself; 311 B in OutRun; offsets validated once, in cruisn, and propagated by comment. Three of them independently synthesise a plausible RPM for games that have none |
| Forza probe / listener / synth | 4 Python scripts in 4 repos | two are the same file, diverged |
| `UnityForceFeedback.cpp` | 2 | product name, env var and log path are the only intended differences; art-of-sim-rally has since gained the crash fix, Fanatec-twin fallback, capability labels and axis reading |
| dinput8 proxy scaffold | 3 | milestone, devreorder-bridge, Launchbox-Racing |
| DirectInput device enumeration | 8+ | C++ ×4, C# ×4 (xoutput, ez-config, ReduxConfig, InputBridge), Python ctypes ×1, PowerShell Add-Type ×1 |
| Steam library discovery (registry + `libraryfolders.vdf`) | 6+ | triplicated inside milestone alone |
| Support / crash bundle | 3 | C# (art of rally), Python (cruisn), C++ minidump zip (OutRun upstream) |
| `wheelprobe` vs `dinput-enum` | 2 | same purpose, different repos |
| InputBridge companion | 2 | byte-duplicated between two MCP repos |

## Knowledge, scattered

Each of these was paid for once and is written down in exactly one repo:

- **DirectInput FFB lifecycle** — `0x80040205` is `NOTEXCLUSIVEACQUIRED`
  (focus loss; re-acquire) [art of rally]; `E_HANDLE`/`NOTDOWNLOADED` → recreate
  and ramp in from zero over 250 ms, never step back [OutRun]; autocentre may
  only be written unacquired (`0x800700AA` otherwise) [OutRun]; never re-acquire
  on a failed `Poll` — it destroys downloaded effects [OutRun]; give FFB its own
  exclusive handle, poll on a non-exclusive one [OutRun]; zero the wheel while
  the window still exists — by `DLL_PROCESS_DETACH` nothing reaches it
  [OutRun]; hold-watchdog releases force when the game stops writing [cruisn];
  never hard-kill a game with FFB live [cruisn, milestone]; one DirectInput
  instance per session — a released "temporary" instance crashed a Fanatec
  user [art of rally].
- **Sign and encoding** — signed magnitude with direction length = |magnitude|
  is the only encoding that works on MOZA R5 (honours both), R12 (magnitude
  sign only) and single-axis Fanatec [art of rally]; positive game byte pushes
  right, positive SDL level turns the R12 left [cruisn].
- **Signal design** — Pacejka aligning torque reverses sign past ~8° slip and
  the game runs 12–29°, so use lateral force × pneumatic trail, fade below
  12 km/h [art of rally]; arcade FFB: dual-rate EMA, `tanh` soft saturation,
  slew limiter with event bypass, hardware periodics for texture (synthesised
  vibration above 15 Hz dies to zero-order hold), wheel *lightens* in a drift
  [OutRun]; a damper loop on a strong low-friction base is unstable — strength,
  slew, peak clamp ladder [cruisn].
- **Device identification** — Fanatec DD presents two identical `FANATEC Wheel`
  devices, one without the actuator (`0x80040154`), and Rewired's Raw Input
  cannot parse either [art of rally]; ≥6 axes types as SIXDOF, never DRIVING,
  patch both read sites [milestone]; vJoy reports 8 axes/128 buttons like an
  R12 and advertises FFB — blocklist virtual pads and rank FFB-capable first
  [OutRun]; MOZA pedals idle low, Logitech/Fanatec idle high — copying a block
  leaves a pedal "held" and breaks press-to-bind listeners [milestone]; MOZA
  pedals rest at centre → half-axis bindings [cruisn]; SDL 2.26 changed the
  joystick GUID layout, so GUIDs harvested under one SDL build match nothing
  under another [cruisn]; VID:PID targeting (`guidProduct.Data1 =
  (PID<<16)|VID`) is the precise, portable device key and the natural bridge
  to wheel-db [cannonball-se, milestone].
- **Rules with conditions, not absolutes** — OutRun needs its own
  `EXCLUSIVE|BACKGROUND` FFB handle beside a non-exclusive polling handle;
  cannonball-se deliberately opens *non-exclusive* because SDL owns the device
  in that process. Both are right in context; the knowledge base must record
  the condition. Likewise a toolkit shipped as `dinput8.dll` cannot coexist
  with DevReorder (`launchbox/devreorder-bridge`) in the same game folder, and
  two LaunchBox repos deploy both — a design constraint for the proxy module.
- **Telemetry** — Forza Horizon is 324 B (232 sled + 12 + 79 dash + 1); 323 is
  rejected silently, SimHub logs it 19,880 times and never connects
  [milestone]; keep emitting zeroed frames between races or dashboards freeze,
  but never a fully-zero packet [cruisn]; report `IsRaceOn` from the countdown
  so shakers follow the engine on the line [art of rally]; missing data is -1,
  not a confident zero [OutRun]; MOZA Pit House and SimHub both want
  `127.0.0.1:8000` [OutRun].
- **Engine-specific** — IL2CPP: injected MonoBehaviours must contain only Unity
  messages; consts not emitted; patched methods can be inlined and dead; count
  prefix calls [iRacing]. Unity Input System: `supportedDevices` silently
  drops devices; rebind on a private capture action; HID layouts only cover
  the Generic Desktop page [iRacing]. Rewired: never switch the input source at
  runtime — `ResetAll()` kills the menus [art of rally]. UE4: `Offset_Internal`
  is `+0x50` in 4.17, `+0x44` in 4.19+, `+0x4C` in old SDK dumps; scan for
  structures that validate, not code signatures [milestone]. FMOD: the
  delay-load thunk self-unhooks; audio parameters *are* telemetry and call
  count classifies them [milestone]. MAME: `winhybrid` ≠ `dinput`; buttons
  33–48 are `ADDSW1-16`, 49+ unaddressable; glfw compacts axes, DirectInput
  uses fixed slots [cruisn].
- **Unattended testing** — art of rally accepts `SendInput` keys, iRacing
  Arcade and MAME do not (Raw Input drops events without a device handle);
  the x64 `INPUT` struct is 40 bytes; `Stop-Process` from a Bash-spawned
  PowerShell does not stop the game; GDI capture shows a GL overlay as black.

None of this is in `abm-personal`: the dashboard was last generated
2026-08-15 and tracks none of the five repos except OutRun2006Tweaks-FFB.

## What the toolkit should be

A repository `dbce-wheel-mod-toolkit` (MIT, no game files, publishable),
with **one native core, thin bindings, shared tooling, and a knowledge base**
— because the consumers run in three runtimes (Unity 2019 Mono, .NET 6
CoreCLR under BepInEx, native C++/Python) and only a C ABI serves all three.

```
native/wheelffb/        C-ABI DLL: the unified FFB + device layer
native/dinput-proxy/    dinput8 proxy scaffold, SIXDOF re-type, FFB/input taps (optional)
dotnet/Dbce.Wheel.Telemetry/   netstandard2.0: Forza 232/311/324, IsRaceOn semantics, idle keeper, tests
dotnet/Dbce.Wheel.Ffb/         netstandard2.0: P/Invoke (Mono + CoreCLR loaders), force-shaping chain, device selection, diagnostics, support bundle
dotnet/Dbce.Wheel.Input/       direct-read binding (assign-by-moving, rest/far calibration); Unity Input System kit as a sub-package
tools/                  wheelprobe, forza-probe/synth, ffb_trace_report, dinput_axes, rawjoy; PowerShell module (Steam discovery, device enumeration, CRLF-safe INI, installer scaffold, unattended-test helpers)
knowledge/              the sections above as documents; feeds wheel-db
templates/              unity-mono-umm/, unity-il2cpp-bepinex/, native-dinput8-proxy/ with CLAUDE.md skeletons carrying the non-negotiables
```

**The native core** is `art-of-sim-rally`'s current 937-line DLL (the
superset), with the product name and log path made runtime parameters —
today the only intentional difference between the two copies — plus OutRun's
lifecycle rules (exit-time zeroing, recreate-and-ramp, poll rule, separate
exclusive handle, periodic effects) and cruisn's hold watchdog. The seven
export names the art of rally game itself declares stay as aliases.

**Not in scope**: game hooks and samplers, per-game force signals and tuning
constants, physics changes and their online gates, camera implementations
(the Cinemachine-vcam-at-priority technique and bounds-based placement become
a document plus a small Unity helper), UI content. Engine reflection (UE4
scanner, FMOD tap) can live under `native/` as optional modules.

**Distribution**: GitHub releases of built artifacts (native DLL, netstandard
DLLs, tools), vendored into each consumer by a `Sync-Toolkit.ps1` that pins a
version — explicit and diffable, the way the repos already vendor BepInEx.
Source-level sharing via submodule is the alternative for the .NET parts;
the native DLL should always be a built artifact. The C ABI is the contract.

## Sequence

1. **Repo + knowledge + portfolio cards** (half a day). Pure win, no code
   risk. Also puts the five repos into `abm-personal`.
2. **Telemetry library** (a day). Collapse two identical C# copies into one,
   add the 311 B variant and the idle keeper, move the existing 210-line test
   suite, one probe/synth pair. Consume from art-of-sim-rally and
   iracing-arcade-wheel first.
3. **Native FFB core** (two to three days). Unify on the superset, fold in
   OutRun's and cruisn's lifecycle rules, keep legacy exports; both Unity mods
   ship the same binary. This is the step that makes "fix an FFB bug once"
   true.
4. **Device identification + wheel-db** (a day). Capability-scored
   enumeration, virtual-pad blocklist, per-brand quirk table (Fanatec twins,
   pedal polarity, R5/R12 sign semantics) as data in wheel-db.
5. **Input kit + templates** (when the next game arrives). Direct-read core,
   Unity Input System kit, IL2CPP-safe IMGUI widgets, scaffolds and the
   bootstrap checklist: fingerprint engine (`dbce-mod-toolkit` already does
   this) → pick template → find car state → choose force signal → wire
   telemetry → panel → installer → support bundle.
6. **Deferred**: merging the three dinput8 proxy scaffolds and the two
   InputBridge copies — do it when a proxy-style game is next.

## Risks and costs

- A shared repo adds a release step and version pinning; the payoff only
  arrives once both Unity mods consume the same binary (step 3).
- Licence boundary: MAME-side code is GPL-2.0+ and stays in `mame-src`; the
  toolkit is MIT; OutRun upstream code (crash bundle) is emoose's — check
  before lifting.
- The two Unity runtimes differ (Mono preload vs CoreCLR resolver); the
  netstandard2.0-with-no-Unity-references rule already solves this and must
  stay.
- Knowledge rots when it moves: every knowledge page needs the repo and date
  it came from, and each consumer's CLAUDE.md should point at the toolkit's
  pages instead of repeating them.
