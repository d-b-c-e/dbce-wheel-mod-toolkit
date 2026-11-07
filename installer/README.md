# Shared installer

The one installer engine every mod package ships (PKG-1; `dbce-project-mgmt/docs/architecture/2026-11/04-cross-cutting.md`
B.2 and B.6). `tools\package\New-ModPackage.ps1` copies these files into each zip:

| File | In the zip | What it does |
|---|---|---|
| `Install.ps1` | `dbce\Install.ps1` | Finds the game, checks the package, backs up, copies in one transaction and writes the `installed` section of the game folder's `dbce-mod.json`. `-Uninstall`, `-Rollback`, `-Setup`, and `-Mode`/`-PackageRoot` for Wheelkit. Windows PowerShell 5.1 and PowerShell 7. Kinds: dinput8. |
| `DbceModFile.ps1` | `dbce\DbceModFile.ps1` | The PowerShell reader of `dbce-mod.json` (XC-1). |
| `Install.bat`, `Uninstall.bat`, `Setup.bat` | zip root | What the player double-clicks. |

The rules, the package layout and the command line are in `docs/DBCE-MOD-JSON.md`, "The package and the installer".
Tests: `tools\tests\Test-ModPackage.ps1` and `tools\tests\Test-DbceModFile.ps1`.

Sync-Toolkit's `installer` part vendors this folder to `lib/toolkit/installer`. Games move to it one family at a time
(B.7): the dinput8 games first (DiRT 4 is PKG-2), then BepInEx, UE4SS, UMM and the rest. Until a game moves, it keeps
its own installer.
