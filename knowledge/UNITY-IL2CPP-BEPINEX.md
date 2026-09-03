# Unity IL2CPP with BepInEx 6 — rules learned the hard way

Source: iracing-arcade-wheel (Unity 6000.3.19f1, metadata v39, BepInEx 6
Bleeding Edge #788, Doorstop 4.5, .NET 6 CoreCLR), 2026-09-02/03.

## Loader

- BepInEx 6 BE **≥ #786** for metadata v39; builds before #755 fail outright.
  Download in chunks (HTTP Range) — the CDN cuts long downloads.
- Pre-seed `BepInEx\config\BepInEx.cfg` with `[IL2CPP] UnityBaseLibrariesSource =`
  (blank) **before first launch**; otherwise the chainloader dies in
  `SetupUnityLogging` on a corrupt unstripped `UnityEngine.CoreModule`
  (BepInEx#1350). Ship the whole cfg with the installer.
- First launch generates interop assemblies into `BepInEx\interop` (~70 s,
  154 assemblies). `Class::Init signatures have been exhausted` is survivable.
- Build against `lib\core` + `lib\interop\*.dll` with `Private="false"`;
  target `net6.0`. A `sync-lib.ps1` copies them from the game.
- Interop `UnityEngine.*` contains **only what the game did not strip**. Check
  with `ilspycmd -t <Type> lib\interop\UnityEngine.<Module>.dll` before using
  any API; probe optional members defensively.

## Plugin shape

- `BasePlugin` registers an **injected `MonoBehaviour` that contains only
  Unity messages** (`FixedUpdate`, `Update`, `OnGUI`) and forwards to a plain
  static `Bridge`. Il2CppInterop registers *every* method of the type with
  IL2CPP; a helper taking `in CarState` or a settings object throws a
  `NullReferenceException` during registration and the component silently
  never exists.
- Log `Harmony.PatchAll` + `GetPatchedMethods()`, and a periodic `status:` line
  (every 10 s for two minutes, then every 60 s) so a run is diagnosable from
  the log alone.
- Unity `const` fields are **not emitted** into interop (`Mathf.Deg2Rad`);
  define locally.
- Interfaces on interop classes are not real C# interfaces; extension methods
  over `IInputActionCollection2` etc. are unusable — write your own loop.
- Identity: compare `.Pointer`, not references. Casting: `TryCast<T>()`.
  Types: `Il2CppType.Of<T>()`, `Il2CppReferenceArray<T>`,
  `Il2CppSystem.Collections.Generic.List<T>`, `Unbox<float>()`.
- Delegates: prefer polling from the frame tick over callbacks, "to keep
  managed delegates out of IL2CPP's hands"; poll
  `SceneManager.GetActiveScene()` rather than subscribing.

## Patching

- **A patched method can be dead.** `AutomobileMotor.PowerDrive` patched
  cleanly, Harmony listed it, and its prefix was never called — MSVC inlined it
  into the caller. Small single-caller methods are suspects; methods with `out`
  parameters and more code (`UpdateRpm`) were not inlined. **Count prefix calls
  before believing a hook**, and hook a method with several callers or too
  big to inline.
- The last managed point before Burst physics is where physics-affecting
  changes go (`AutomobileWheelsSetup.PushWheelInputs`); caps alone did not
  limit speed — the mod had to supply its own rev limiter.
- Prefix/postfix pairs that hand the game its own unscaled numbers and then
  write scaled ones keep the game's smoothing stable.

## IMGUI is stripped

No `GUILayout.Toggle`, `HorizontalSlider`, `TextField`, `BeginScrollView`, no
`GUIStyle.fontStyle` setter. What survives: `GUILayoutUtility.GetRect`,
`GUI.Button`, `GUI.DoControl`, `GUILayout.DoTextField`, `GUI.DrawTexture` +
`Texture2D.whiteTexture`, `Event.current`. iRacing's `Widgets.cs` rebuilt
Toggle, Slider (hot-control handling), ValueBar, TextField, Radio, Section,
Rule, Fill from those; scrolling via `GUI.BeginGroup` offset + `BeginArea`.
That file is the shared drawing layer for any IL2CPP panel.

## Native DLL loading

`NativeLibrary.SetDllImportResolver(assembly, …)` maps the import name to an
absolute path (CoreCLR). The Mono sibling preloads with `LoadLibraryW`. One
binding with both paths behind a runtime check.

## Environment gotchas

- `Stop-Process` from a Bash-spawned PowerShell does not stop the game.
- BepInEx truncates `LogOutput.log` at startup (`AppendLog = false`).
- The game ignores OS `SendInput` for the keyboard; see `UNATTENDED-TESTING.md`.
- The main menu drops a second Down within ~1 s.
