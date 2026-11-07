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
    `dotnet/Dbce.Wheel.Mod`.
  - C++, for the panel core: `native/panel/dbce_mod_file.h`. Its `settings` section is the UI-1 descriptor reader in
    `dbce_panel_descriptor.h`.
- Tests: `tools/tests/Test-DbceModFile.ps1` (schema and the PowerShell reader), `Dbce.Wheel.Mod.Tests` and
  `native/panel/build_test.ps1`. All three readers must write the same dump, byte for byte, as the `.dump.txt` files.

## Sections

| Section | Who writes it | Who reads it | Holds |
|---|---|---|---|
| identity: `schema`, `id`, `name`, `version`, `steamAppId`, `folder`, `exe`, `repository`, `log` | game repo | everyone | `schema` is `dbce.mod@1`. `id` is the catalog and grid id. `log.start` is the regular expression for the mod's first log line, with group `v` for the version. |
| `package` | game repo | packager, installer | `kind` (dinput8, bepinex5, bepinex6-il2cpp, umm, ue4ss, electron-asar, standalone), `loader` (name, version, bundle) or null, `payload` and `optional` (`from` in the package, `to` in the game folder), `conflicts` (files that mean another loader owns the slot), `seedConfig` (copied only when absent, STD-044), `knownBuilds` (unknown means a warning, unless `strict`), `uiHost` (native-panel, dbce-ui-dll, dbce-ui-exe, or game for a mod that still draws its own F6). |
| `settings` | game repo | F6 panel, `dbce-ui.exe --setup`, Wheelkit | The UI-1 block, unchanged (see `native/panel/dbce_panel_descriptor.h`), plus two optional fields: `format` (ini or xml) for `config`, and on each row a `role` and a `store`. |
| `controls` | game repo | Wheelkit | `store` is `game` (the game's own controls menu) or the shared profile adapter id. Also `profile`, `settings`, `optIn` (section, key, value, default, appliesAfterRestart), `delivered` actions, `unsupported` (action to reason), `conditions` (actions, transmission, settingsKey, settingsValues, settingsDefault, note) and `gameControls` (id, kind, label, group). |
| `cameras` | game repo | Wheelkit | STD-046 starting choices for triple screens: `camera` (cockpit, bonnet, bumper, chase, isometric), `start` (realistic or game), and where the choice lives (`file`, `section`, `key`), with the stored `realistic` and `game` values. |
| `selfTest` | game repo | test scripts | The self-test contract (`04-cross-cutting.md` D.2): the `folder`, the `steps` this build runs, and the `probes` its state dump reports (id, type, label). No mod ships the self-test yet, so DiRT 4's fixture block is an example. |
| `installed` | installer only | Wheelkit, installer | `version`, `commit`, `installedUtc`, `loaderInstalledByUs`, `files` (path and lower-case SHA-256, game-relative), `backups` (folder, game-relative) and `flags` (booleans the old receipts carried, read only during migration). |

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

## The installer's command line

`dbce\Install.ps1` in every package runs under Windows PowerShell 5.1. `Install.bat` and `Uninstall.bat` at the zip
root call it.

| Argument | Meaning |
|---|---|
| `-GameDir <path>` | The game folder. Without it the installer finds the game from `steamAppId` and `folder`, or asks. |
| `-Uninstall` | Removes the files `installed.files` lists, when their hashes still match, and the loader only when `loaderInstalledByUs`. Restores the backups. Keeps settings and bindings. |
| `-Rollback` | Puts back the previous install from `installed.backups`. |
| `-Mode <Install\|Uninstall\|Rollback>` | The same three actions as one argument, so Wheelkit's `PackageScriptRunner` and `RepoScriptInstaller` call the installer unchanged. Combined with a switch, it must agree with the switch. |
| `-PackageRoot <path>` | The unpacked package. It defaults to the folder above `dbce\`. |

The installer reads `package` from the package's own `dbce-mod.json`. It copies it to the game folder with the
`installed` section added. Old receipts, such as `dbce-dirt4-receipt.json`, `BepInEx\WodenWheel-install.json` and art's
`Info.json` flags, are read once for migration and deleted on the next install. Output is plain American English, and
the last line says what to do next.

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
