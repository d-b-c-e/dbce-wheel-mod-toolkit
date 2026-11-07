# dbce-mod.json

One file per mod (XC-1; `dbce-project-mgmt/docs/architecture/2026-11/00-overview.md` section 2). The game repo writes
it, the package ships it, and the installer copies it to the game folder and adds the `installed` section. The F6 panel
draws its pages from `settings`. Wheelkit reads settings, controls and cameras from it, so a new mod release needs no
Wheelkit change. The installer reads `package` and writes `installed`. That one section replaces the 15 receipt
names Wheelkit reads today.

- Schema: `schema/dbce-mod.schema.json` (JSON Schema 2020-12, strict: unknown fields are refused).
- Fixtures written from today's data: `schema/fixtures/dirt-4`, `super-woden-rally-edge` and `art-of-rally`
  (`.dbce-mod.json`), each with its canonical dump (`.dump.txt`).
- Readers:
  - PowerShell 5.1, for the installer: `installer/DbceModFile.ps1`, with `Read-DbceModFile` and `Format-DbceModFile`.
  - C# netstandard2.0 and Mono 2.0, for Wheelkit and the Unity mods: `shared/mod/DbceModFile.cs`, built by
    `dotnet/Dbce.Wheel.ModFile`. A Mono 2.0 plugin compiles it in by adding `mod` to `DbceSharedSources`
    (`unity/Dbce.Unity.props`). `Test-UnityConsumer.ps1` does that and runs the .NET 3.5 API check over it. No
    Mono 4.x or IL2CPP plugin references `Dbce.Wheel.ModFile.dll` yet; Sync-Toolkit's `dotnet` part adds it when the
    first mod reads the file.
  - C++, for the panel core: `native/panel/dbce_mod_file.h`. Its `settings` section is the UI-1 descriptor reader in
    `dbce_panel_descriptor.h`.
- Tests: `tools/tests/Test-DbceModFile.ps1` (schema and the PowerShell reader), `Dbce.Wheel.ModFile.Tests` and
  `native/panel/build_test.ps1`. All three readers must write the same dump as the `.dump.txt` files, line for line.

## Sections

| Section | Who writes it | Who reads it | Holds |
|---|---|---|---|
| identity: `schema`, `id`, `name`, `version`, `steamAppId`, `folder`, `exe`, `repository`, `log` | game repo | everyone | `schema` is `dbce.mod@1`. `id` is the catalog and grid id. `log.start` is the regular expression for the mod's first log line, with group `v` for the version. |
| `package` | game repo | packager, installer | `kind` (dinput8, bepinex5, bepinex6-il2cpp, umm, ue4ss, electron-asar, standalone), `loader` (name, version, bundle) or null, `payload` and `optional` (`from` relative to the repo root when packaging and the same path inside the zip, `to` in the game folder), `conflicts` (files that mean another loader owns the slot), `seedConfig` (copied only when absent, STD-044), `knownBuilds` (unknown means a warning, unless `strict`), `uiHost` (native-panel, dbce-ui-dll, dbce-ui-exe, or game for a mod that still draws its own F6), `oldReceipts` (installer only, not in the dump: the game's earlier installer's `receipt` and `backups` folder, read once for migration). |
| `settings` | game repo | F6 panel, `dbce-ui.exe --setup`, Wheelkit | The UI-1 block, unchanged (see `native/panel/dbce_panel_descriptor.h`), plus two optional fields: `format` (ini or xml) for `config`, and on each row a `role` and a `store`. |
| `controls` | game repo | Wheelkit | `store` is `game` (the game's own controls menu) or the shared profile adapter id. Also `profile`, `settings`, `optIn` (section, key, value, default, appliesAfterRestart), `delivered` actions, `unsupported` (action to reason), `conditions` (actions, transmission, settingsKey, settingsValues, settingsDefault, note) and `gameControls` (id, kind, label, group). |
| `cameras` | game repo | Wheelkit | STD-046 starting choices for triple screens: `camera` (cockpit, bonnet, bumper, chase, isometric), `start` (realistic or game), and where the choice lives (`file`, `section`, `key`), with the stored `realistic` and `game` values. |
| `selfTest` | game repo | test scripts | The self-test contract (`04-cross-cutting.md` D.2): the `folder`, the `steps` this build runs, and the `probes` its state dump reports (id, type, label). The script, result and C++ interpreter are in `docs/SELFTEST.md`. No mod ships the self-test yet, so DiRT 4's fixture block is an example. |
| `installed` | installer only | Wheelkit, installer | `version`, `commit`, `installedUtc`, `loaderInstalledByUs`, `files` (path and lower-case SHA-256, game-relative), `backups` (folder, game-relative) and `flags` (booleans the old receipts carried, read only during migration). |

### `settings.stored`: the stored-settings list

`settings.stored` is optional. It lists the stored values an editor outside the game reads and writes, one item per
value in file order, with the rules the mod's own code follows when it reads them. Wheelkit reads it (WK-4a); the F6
panel, the installer and the C++, C# and PowerShell readers ignore it, and it is not part of the canonical dump.

- `file` is the main settings file and `format` its layout: `ini`, `toplevel` (keys before any section), `xml`,
  `xml-path` (an XML file edited by element path) or `outrun-ini`.
- Each item is one stored value in the form Wheelkit's catalog rows used: `section`, `key`, `label`, `type` (bool,
  choice, int or number), `default` as stored text, `options`, `min`, `max`, `step`, `on` and `off`, `page`,
  `nextStart`, and the parsing rules `clamp`, `singlePrecision`, `strictBoolean` and `caseSensitive`.
- Relations between items: `hidden` and `composite` (one shown choice standing for several stored values), `readOnly`,
  `seedWhenCreated`, `enabledWhen`, `distinctFrom` and `distinctExcept`, and `requiresReceiptCapability` while
  receipts still gate rows.
- An item may name another `file` with its `fileFormat`; `external:<id>` names a game-owned file Wheelkit knows.
- Wheelkit's bundled `descriptors/<game>.json` files carry this block, written from its catalog. When a mod's own
  `dbce-mod.json` carries it, Wheelkit uses that instead, so a new mod release needs no Wheelkit change.

### Rows: `role` and `store`

- `role` says what a row means in every game, so Wheelkit maps rig profiles by role instead of guessing key names
  (`ProfileTargets`, STD-032 item 4). Roles in the fixtures: `ffb.enabled`, `ffb.strength`, `ffb.crashStrength`,
  `ffb.invert`, `ffb.smoothing`, `screens.mode`, `screens.eyeDistanceMm`, `screens.panelWidthMm`, `screens.bezelMm`,
  `telemetry.enabled` and `telemetry.port`. A role names the unit when the unit matters, like `Mm` and `Deg`.
- `store` says where the value lives: `file` (default: the block's `config`), `section` (empty for xml), `key`, `on`
  and `off` (a toggle's stored text), and `scale` (stored value = shown value × scale; art's eye distance is shown in
  cm and stored in mm, so its scale is 10).
- A row without `store` is drawn by the mod itself, like a composite such as art's Screens choice. Wheelkit sets it by
  role only through the mod's own setup, never by key.
- The installed build declares only the settings it reads. That replaces `RequiresReceiptCapability`,
  `ReviewedTripleSettings` and the DLL pins.

## Defaults

Every reader fills these in, so all three see the same values:

- Absent strings are empty. Absent lists are empty. `steamAppId` is 0. `package.strict` is false. `package.uiHost` is
  `game`. `settings.format` is `ini`. Absent `settings`, `controls`, `selfTest`, `installed`, `controls.optIn`,
  `package.loader` and a row's `store` are null.
- Rows follow the UI-1 reader. `level` defaults to simple and `apply` to live. `min` and `max` are 0, except for a
  percent, which is 0 to 100. `default` is a number: a toggle is 1 or 0, a choice is its index (0 when absent), and a
  percent, slider or number is its value (`min` when absent). `decimals` is 0. `store.scale` is 1.

## Canonical dump

Each line is `path=value`, in the order of `installer/DbceModFile.ps1` `Format-DbceModFile`, which this order follows.

- Lists write `name.count`, then `name[i]`. Objects that are null write `name=null`.
- Strings are written as-is, with `\` written as `\\`, and CR and LF written as `\r` and `\n`.
- Booleans are `true` or `false`. Numbers use the shortest round-trip form, invariant culture.

## The package and the installer (PKG-1)

`tools\package\New-ModPackage.ps1 -Repo <game repo> [-Out <folder>] [-SetupTool <dbce-ui.exe>]` builds
`<name> <version>.zip` (for example `DiRT 4 Wheel Mod 0.2.5.zip`) and a `.sha256` file beside it, in `<repo>\dist` by
default. It refuses uncommitted tracked source, a `dbce-mod.json` that is not committed, fails the schema or has an
`installed` section, a missing payload file and an existing zip. Kinds handled so far: `dinput8`, without
`knownBuilds`; the packager refuses the others until the installer handles them.

| In the zip | From |
|---|---|
| `Install.bat`, `Uninstall.bat`, `Setup.bat` (a mod with `settings`) | `installer\*.bat` |
| `README.txt` | written from the mod file: install, settings, keys, undo, log, repository |
| `dbce-mod.json` | the repo's file, unchanged |
| `release-manifest.json` | `schema` `dbce.release-manifest@1`, `id`, `name`, `version`, `commit` and the lower-case SHA-256 of every other file, all computed |
| payload, optional and seed files | at their `from` path |
| the loader bundle | `loader.bundle`, when set |
| `dbce\Install.ps1`, `dbce\DbceModFile.ps1` | `installer\` |
| `dbce\dbce-ui.exe` (a mod with `settings`) | `native\ui-exe\build\dbce-ui.exe` or `-SetupTool` |
| `licenses\` | the repo's `LICENSE*`, `THIRD-PARTY-NOTICES*` and `licenses\`, the toolkit's LICENSE and, with the setup tool or a toolkit F6 panel (`uiHost` other than game), Dear ImGui's |

`dbce\Install.ps1` runs under Windows PowerShell 5.1 and PowerShell 7. `Install.bat`, `Uninstall.bat` and `Setup.bat`
at the zip root call it.

| Argument | Meaning |
|---|---|
| `-GameDir <path>` | The game folder. Without it the installer finds the game in Steam's libraries from `steamAppId` (the app manifest's install folder) and `folder`, or asks when there are several. |
| `-Uninstall` | Removes the files `installed.files` lists while their hashes still match, puts back the game files the mod replaced and deletes the backups. Files changed after the install, such as a setting the player changed, stay. |
| `-Rollback` | Puts back the exact state before the last install from `installed.backups`: the previous install, or the game without the mod. |
| `-Setup` | Opens `dbce\dbce-ui.exe --setup <game folder>\dbce-mod.json`, for settings with the game closed. |
| `-Mode <Install\|Uninstall\|Rollback>` | The same three actions as one argument, so Wheelkit's `PackageScriptRunner` and `RepoScriptInstaller` call the installer unchanged. Combined with a switch, it must agree with the switch. |
| `-PackageRoot <path>` | The unpacked package. It defaults to the folder above `dbce\`. |

What install does, in order (`04-cross-cutting.md` B.2):

1. Finds the game, checks an exe from `exe` is there and that the game is not running.
2. Checks every package file against `release-manifest.json` before it touches anything.
3. Refuses a game folder, target or backup folder that is or sits under a link or junction.
4. Refuses a `conflicts` file that is present and not on this mod's install record (another mod's `dinput8.dll`).
5. Installing the same version and commit again, with every file still in place, changes nothing.
6. Backs up the exact state of every path it changes, `dbce-mod.json` included, into
   `dbce-<id>-backups\<yyyyMMdd-HHmmss>-<id8>` with a `backup.json` list (path and SHA-256, or null for absent). A game
   file it replaces for the first time is also kept in `dbce-<id>-backups\original` for uninstall.
7. Writes everything in one transaction: temp file, then replace, then read back. If one write fails, every path is
   put back as it was.
8. Seeds `seedConfig` files only when absent (STD-044). A seed stays on the install record while unchanged, so
   uninstall removes it; once the player changes it, it is theirs and is never touched.
9. Writes the game folder's `dbce-mod.json`: the package's file, unchanged, with the `installed` section appended
   (`loaderInstalledByUs` is false for dinput8). Files of the previous install that this build no longer ships are
   removed while unchanged.

Output is plain American English, and the last line says what to do next. Errors are one sentence that ends with
"Nothing changed." when nothing did.

Old receipts (`package.oldReceipts`, PKG-2). A game's installer before this one left a receipt such as
`dbce-dirt4-receipt.json` (`{file, sha256}`) and a backups folder such as `dbce-dirt4-backups`. The installer reads the
receipt once: the file it lists counts as this mod's while its hash matches, so the install replaces it instead of
refusing it as another mod's. Install deletes the old receipt and keeps it in its backup, so `-Rollback` puts the old
install back exactly; the old backups folder stays until uninstall, which removes it with the mod (it holds only earlier
builds of the mod). Uninstall also removes an old install that the shared installer never touched. A file that differs
from its old receipt is refused as another mod's. Other receipt shapes (`BepInEx\WodenWheel-install.json`, art's
`Info.json` flags) come with those games' switch.

Tested by `tools\tests\Test-ModPackage.ps1` (PowerShell 7; the installer in 5.1 and once in 7) on a fixture repo and
game folder: package contents and computed hashes, the packager's refusals, install, reinstall, uninstall, a kept
setting, upgrade and two rollbacks, a foreign `dinput8.dll`, a junction as the game folder and as a payload folder, a
changed package file, a write that fails halfway, and an old receipted install that is migrated, rolled back and
uninstalled. Each is compared byte for byte with a snapshot of the folder.

## How the fixtures map today's data

- DiRT 4: catalog row (identity, log, the two settings items), `dbce-dirt4-receipt.json` (installed: version, commit,
  dinput8.dll and its hash, the backup folder), and `tools/Install-Mod.ps1` (the payload and its dinput8.dll conflict).
- Super Woden: Rally Edge: catalog row (identity, log, 20 of its 23 settings rows; Mode and SpanSeparateMonitors fold
  into Screens, and MatchGameFov is hidden), `BepInEx\WodenWheel-install.json` (files, backup folder, controls block,
  `wodenFovSets` camera starts, the GameAssembly hash as a known build and the capability flags), and the BepInEx
  version in `Package.ps1`.
- art of rally: the UI-1 sample `native/panel/samples/art-of-rally.settings.json` with roles and stores from the catalog
  row, `lib/input-shim/CONTROLS.json` (controls), `Info.json` and `build.json` (version, commit, flags; `installedUtc`
  is the build time because art's installer writes no install time), and the installed files' hashes.
