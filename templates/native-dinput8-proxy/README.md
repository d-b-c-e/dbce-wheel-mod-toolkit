# Template: native `dinput8.dll` proxy

Pick this when there is no managed runtime to hook: a native game you sit
beside. Windows loads your `dinput8.dll` instead of the system one, you forward
every call to the real one in `system32`, and in between you hook the game.

Vendor the shared native code (F6 panel, force layer, controls, settings,
support report, display, Dear ImGui, triple-screen) into `src/vendor` from one
toolkit commit and one triple-screen commit:

```powershell
pwsh <toolkit>\tools\Sync-Native.ps1 -GameRepo <this repo>          # toolkit master + triple-screen main
pwsh <toolkit>\tools\Sync-Native.ps1 -GameRepo <this repo> -Check   # in build.ps1: fails if a vendored file was edited
```

Build for the game's architecture, not your machine's: DiRT 3 and Sega Rally
Revo are x86, the others x64.

Working examples: GRID (2019), DiRT 4 and DiRT Rally 2.0 (x64), DiRT 3 (x86).
