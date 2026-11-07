# Shared Unity layer (skeleton)

This is source that compiles into each Unity game's plugin and is never edited in the game. Vendor it with
`tools/Sync-Toolkit.ps1 -Parts shared,unity,dotnet`. That copies it to `lib/toolkit/unity` and records its SHA-256 in
`lib/toolkit/MANIFEST.txt`. Then import the props file from the game's `Directory.Build.props`:

```xml
<Project>
  <PropertyGroup>
    <DbceRuntime>Il2Cpp</DbceRuntime>    <!-- Mono, Mono20 or Il2Cpp -->
    <DbceImgui>Rect</DbceImgui>          <!-- Layout or Rect -->
    <DbcePipeline>BuiltIn</DbcePipeline> <!-- BuiltIn or Urp -->
  </PropertyGroup>
  <Import Project="lib\toolkit\unity\Dbce.Unity.props" />
</Project>
```

| Runtime | Defines | Engine-free libraries |
|---|---|---|
| Mono (Unity Mono 4.x) | none | `lib/toolkit/dotnet/Dbce.Wheel.{Input,Display,Ui}.dll` (netstandard2.0) |
| Mono20 (HCT, .NET 3.5) | `DBCE_NET35`, `DBCE_INPUT_NET35`, `DBCE_SUPPORT_NET35` | the same sources from `lib/toolkit/shared`, compiled in |
| Il2Cpp (BepInEx 6) | `DBCE_IL2CPP` | the DLLs, as for Mono |

`DbceImgui=Rect` adds `DBCE_IMGUI_RECT`, and `DbcePipeline=Urp` adds `DBCE_URP`. Set the properties before the import.

Today the layer holds only `Runtime/ModDescriptor.cs`: the descriptor enums, and `DbceBuild`, which holds the choices the
plugin was compiled with. `ModDescriptor.MismatchWithBuild()` catches a descriptor that disagrees with the props
settings. The F6 host, FFB runtime, telemetry, cameras, triples and settings adapters move here in Phase B of
`dbce-project-mgmt/docs/architecture/2026-11/02-unity-family.md`.

After each build, run the static check for the runtime:
- Mono20: `tools/Check-Net35Api.ps1 -Dll <plugin.dll> -GameManaged <game>_Data/Managed`.
- Il2Cpp: `tools/Find-StrippedInteropCalls.ps1 -Plugin <plugin.dll> -InteropDir BepInEx/interop`.

Tests: `tools/tests/Test-UnityConsumer.ps1` syncs into a scratch game and compiles a dummy plugin for each runtime.
