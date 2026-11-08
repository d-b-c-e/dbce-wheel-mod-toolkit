# The shared store

One place in the user's folder holds the player's setup for every game (CFG-1; the design is
`dbce-project-mgmt/docs/architecture/2026-11/08-install-and-configuration.md`, items 3 to 5, and STD-053).

## Where it is and who writes it

| File | Holds | Written by | Read by |
|---|---|---|---|
| `%LOCALAPPDATA%\Wheelkit\store\profiles.json` | the profiles, the shared cockpit, which profile each game uses | Wheelkit only | Wheelkit, every mod, install scripts |
| `%LOCALAPPDATA%\Wheelkit\store\runtime\<game-id>.json` | one game's runtime settings: the values its F6 panel changes | that game's mod only | that mod, Wheelkit |

`<game-id>` is the game's `dbce-mod.json` id. Each file has one writer, so two programs never write the same file.

## The one compatibility rule

**Files only gain fields, and every reader ignores fields it does not know.** Mods are released separately from
Wheelkit, so a mod must read a file a newer Wheelkit wrote, and the other way round. The `Schema` numbers (2 for
profiles.json, 1 for runtime files) never change. Nothing is renamed, retyped or removed; a field that is no longer
needed stays in the schema and readers stop using it. Nothing else in the portfolio gets compatibility code.
`tools/tests/Test-DbceStore.ps1` enforces it: no schema object refuses unknown fields; every field listed in
`schema/fixtures/store/fields.txt` must still be in the schemas (`-Update` appends new ones, never removes); and each
`X.future.json` fixture, the file with fields a later version adds, must read exactly like `X.json` in all three readers.

## profiles.json

It is Wheelkit's existing `mapping-profiles.json` (schema 2, PascalCase, as System.Text.Json writes Wheelkit's records)
with two new members, `Cockpit` and `Games`. Schema: `schema/dbce-store-profiles.schema.json`.

- `Profiles`: each profile is a whole setup, copied rather than inherited (Wheelkit's `MappingPreset`): `Id`,
  `Profile` (`Name`, `Transmission` Automatic, Sequential or H-pattern, the `Wheel` and `Shifter` instance GUIDs, and
  `Bindings`, action id to binding as in `docs/controls-contract.md`), and `Rig` (`FfbStrength` 0 to 100, null meaning
  the default 50 that matches art of rally at 50, STD-021; `FfbOn`; `Screens`; and Wheelkit's other settings).
- `Cockpit`: the one screen layout and its measurements (Wheelkit's `ScreenSetup`: count, pixels, diagonal, aspect, eye
  distance, side angle, bezel gap, curve radius, measured width). Every profile uses it unless the profile's own
  `Rig.Screens` is set; a "Widescreen" profile sets one screen there. Null means no cockpit is saved, and each mod keeps
  its own screen settings.
- `Games`: game id to `{ "Profile": "<Id>" }`. The value is an object so it can gain fields later.
- `ActiveId`: the profile Wheelkit is editing. A game with no assignment, or one whose assignment names a deleted
  profile, uses it.

Every reader has the same resolution, `For(gameId)`. It returns the game's profile and the screens that go with it: the
profile's own screens, else the cockpit, else none.

## runtime\<game-id>.json

Schema: `schema/dbce-store-runtime.schema.json`. It holds `Game`, then `Profile` (the profile Id the game used when the
values were saved), then `Values`, which maps an F6 row id from the game's `dbce-mod.json` settings to a number, a
boolean or a string. Readers skip values of other types.

The profile only supplies a starting value. When F6 changes a value, the mod saves it here. A value that starts from
the profile, FFB strength today, applies only while the game still uses the profile named in `Profile`. After the
player picks another profile in Wheelkit, that profile's value is the start again. Values with no profile counterpart,
such as cameras and field of view, always apply. A runtime setting is added only when it is clearly needed.

## Readers

| Language | File | API |
|---|---|---|
| C# netstandard2.0 and Mono 2.0 (Wheelkit, Unity mods) | `shared/mod/DbceStore.cs`, built into `Dbce.Wheel.ModFile`; a Mono 2.0 plugin adds `mod` to `DbceSharedSources` | `DbceStore.Load/Parse`, `.For(gameId)`, `DbceRuntimeSettings.Load/Parse`, `.Get(key)`, `DbceStore.ProfilesPath()`, `RuntimePath(gameId)` |
| C++ (panel core, native mods) | `native/panel/dbce_store.h`, implemented in `dbce_panel_descriptor.cpp` with the dbce-mod.json reader's JSON parser | `LoadStore/ParseStore`, `StoreFor`, `LoadRuntimeSettings/ParseRuntimeSettings`, `StoreProfilesPath()`, `StoreRuntimePath(gameId)` |
| PowerShell 5.1 and 7 (install scripts) | `installer/DbceStore.ps1` | `Read-DbceStore`, `Get-DbceStoreChoice`, `Read-DbceRuntimeSettings`, `Get-DbceStoreFolder`, `Get-DbceRuntimePath` |

Each reader also writes a canonical dump, one `path=value` line per value read plus `for[<game>]` resolution lines.
`Test-DbceStore.ps1` (schemas and the PowerShell 5.1 reader), `dotnet run --project dotnet/Dbce.Wheel.ModFile.Tests`
and `native/panel/build_test.ps1` (x64 and x86) all compare it, line for line, with `schema/fixtures/store/*.dump.txt`.

## Fixtures

- `owner.profiles.json`: the owner's Moza KS Pro Triple Manual and Auto profiles, converted once from Wheelkit's
  `mapping-profiles.json` on 2026-11-08. Their shared screens became the cockpit, and each game's last applied profile
  became its assignment. Device instance GUIDs are placeholders (1111... is the R12 base, 2222... the DS-8X shifter)
  because this repo is public; product GUIDs and names are kept. No conversion code is kept.
- `widescreen.profiles.json`: synthetic. A profile with its own one-screen setup and calibrated bindings, a profile
  with no `Rig`, no cockpit, and an assignment to a deleted profile.
- `art-of-rally.runtime.json`: a runtime file with each value type.

## Writers

- Wheelkit writes profiles.json from 0.6.77 (CFG-2). Its first start copies the old
  `%LOCALAPPDATA%\Wheelkit\mapping-profiles.json` there unchanged. A game's page assigns its profile in `Games`, Apply
  records the profile it applied, and Rig profile > Screens moves a profile to the cockpit or gives it its own screens.
  Wheelkit's `StoreTests` checks that every game resolves to the same profile in Wheelkit and in `DbceStore`.
- Not here yet: each mod's runtime writer arrives with its move to the shared F6 core, and it must keep members it does
  not know when it rewrites its file.
