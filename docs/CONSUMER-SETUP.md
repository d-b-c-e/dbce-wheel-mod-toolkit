# Install to first drive: consumer guide

Part of [UX-1](CONSUMER-UX.md), revised 2026-09-16. This is a release/installer contract
and a reusable player-guide template. It does not claim these installers or a
shared setup wizard already exist. Replace examples with the consumer's verified
names, defaults and capabilities; ship only instructions that its package supports.

## UX-09 — One install/update path

For Windows releases, offer one clearly named ZIP and a visible **Install.bat**
entry point. Running it again updates an existing compatible installation. Keep
**Uninstall.bat** beside it. An equivalent packaged graphical entry point may
use Install / Update / Uninstall with the same behavior. A source archive,
PowerShell command or dependency setup script is not the main player download.

1. Ask the player to close the game normally; the installer checks before writing
   and never kills it. Show the game it found and its full path.
2. Discover Steam libraries, not just the default C: drive. If discovery fails or
   finds several valid installs, offer folder selection with a useful validation
   message. Do not require editing a script to set a path.
3. Validate the game build, architecture, package integrity, supported loader and
   known proxy conflicts before making changes. Preserve other mods and shared
   loader files. A different proxy needs a supported coexistence route or a clear
   explanation of the conflicting file; never silently overwrite it.
4. Bundle redistributable prerequisites where permitted, or download a pinned,
   verified version with visible progress and recoverable errors. Do not require
   a compiler, SDK, runtime backend choice or manual library-copy step from players.
   Keep loader configuration workarounds internal to installation.
5. Stage and validate the whole payload. Back up files being replaced and the
   existing configuration; record ownership/hashes and the installed version.
   A failed update restores the working installation or reports the exact retained
   recovery directory. Preserve unknown files and other mods.
6. Preserve settings, bindings, calibrations, recordings and explicit opt-outs.
   Migrate old formats with a backup; explain any action the player must take.
   A default change applies only where the user has no saved value.
   If the settings-view preference is absent, default it to Simple without
   resetting any existing tune or rerunning first-use defaults. Preserve an
   explicit Simple/Advanced choice on subsequent updates.
7. End with the installed version, game path, outcome, and **Launch normally,
   then press F6 for Wheel settings**. An optional launch action is fine; do not
   unexpectedly start the game or acquire the wheel during installation.

Do not introduce unnecessary administrator prompts, antivirus exclusions or
security-setting instructions. Request elevated rights only for an actual required
write and explain the specific location. Keep routine success output short;
save detailed logs locally and provide their location on failure.

### Failure copy examples

| Condition | Message and recovery |
|---|---|
| Game running | `Close [Game] normally, then choose Retry. Your installation has not changed.` |
| Wrong folder | `This folder does not contain [Game.exe]. Choose the game's installation folder.` |
| Unsupported build | `This game update is not supported by this mod version. Existing files were left in place. Check for a mod update.` |
| Conflicting loader/proxy | `An existing [filename] belongs to another installation. See the supported coexistence steps before continuing.` |
| Update rolled back | `Update failed. Version [old] was restored. Details: [log].` |
| Recovery incomplete | `Update failed and automatic recovery was incomplete. Your previous files are at [path]. Follow [specific recovery steps].` |

Only say “unchanged” or “restored” when the installer verified it. A developer
build with a manual update route must call itself a development build and record
that UX gap; a refusal to overwrite is not a complete user update experience.

## UX-10 — Uninstall is predictable

Uninstall removes only files owned by this mod. It preserves personal settings
and recordings by default and offers a separate, explicit removal of those data.
Remove shared loader files only when ownership and other-consumer checks prove
that is appropriate. Verify modified owned files before removal; retain/report
unexpected user changes. Restore a replaced original file from a verified backup
when applicable. Never recursively delete the game's BepInEx/Mods folder just
because this mod uses it. Show what remains and how to reinstall.

## UX-11 — First use, without a manual beside the wheel

Make **Simple → Setup** the first-use view/page. The header offers
**View: [Simple] [Advanced]**; remember an explicit choice as specified in
[UX-01-S](CONSUMER-SETTINGS-VIEWS.md). Complete the normal first-drive sequence
entirely in Simple. On later opens, remember the last page and keep
Setup easy to find. Do not force a wizard over every launch or assume one physical
device contains all pedals and controls.

| Step | Player action | What the mod shows / checks |
|---|---|---|
| Connect | Connect/power wheel, pedals and any shifter/handbrake; open F6 | Friendly names, independent devices, retained disconnected selections, Refresh; no force |
| Steering | Bind, centre, turn both ways, Save calibration | Left/Centre/Right bar, direction and range; old binding retained until save |
| Pedals | Bind each, release, press fully, Save calibration | Separate 0–100% bars; inversion and resting noise visible |
| Optional handbrake | Bind Handbrake (axis), release/pull fully, Save calibration; keep any button assignment | Independent device and 0–100% bar; progressive game input where supported, explicitly labeled threshold/release fallback if the game is digital-only |
| Buttons | Bind Change camera and Pause, then any desired controls | Press feedback, held/pressed semantics, no accidental menu activation |
| Feedback | Confirm the FFB device and Strength; FFB defaults On and remembers Off | Clear ready/inactive state and Stop FFB; one saved On/Off preference, no session-start step |
| First drive | Close panel; enter an appropriate driving session | Inputs reach the car, normal camera button includes Bonnet/Bumper, output starts only while player controls the car |
| Optional camera tuning | Enter Bonnet/Bumper and adjust with the numpad; rebind under Cameras → Adjustment bindings if needed | Active-mount values, actual saved shortcuts, Bind/Clear/Cancel/defaults; same pose values as sliders, no duplicate camera-cycle binding |
| Optional telemetry | Enable a tested receiver preset in Simple; follow matching receiver instructions | Actual profile/destination and Sending status; custom connection edits and recording setup in Advanced; active capture/Stop and errors remain in Simple |

Do not make dashboard configuration, a shifter, handbrake ownership/binding, camera tuning or telemetry
recording prerequisites for steering and pedals. A keyboard/mouse remains a
usable escape route while the wheel is unbound. When a required step is missing,
offer the one next action, not a page of device dumps.

## Reusable quick-start text

The following text is a template, not a statement about every current release:

> **Install:** Close [Game], extract [release ZIP], and run **Install.bat**.
> Confirm the detected game folder. Launch the game normally.
>
> **Set up:** Press **F6** for Wheel settings. Start in **Simple → Setup**, then
> bind and calibrate Steering, Throttle and Brake. The wheel bar should move left when you turn left;
> each pedal should read 0% released and 100% fully pressed. Bind **Change camera**
> and **Pause**, then any other buttons you want.
>
> **Handbrake, if you use one:** Bind and calibrate **Handbrake (axis)** from
> released to fully pulled. Keep any handbrake button too. [If the game only
> supports On/Off, explain the activation/release points and show its game state.]
>
> **Drive:** In **FFB**, **FFB device** defaults to **Use steering wheel** and shows
> the wheel you bound. Use the dropdown if you want a different base. FFB defaults
> On; a saved Off choice is retained. Start with the suggested strength. Close the
> panel and enter a driving session. **F8 saves FFB Off**; choose On to resume.
> The game's normal camera button cycles its views, then Bonnet and
> Bumper when enabled.
>
> **Adjust the view:** In Bonnet or Bumper, use numpad 8/2 up/down, 9/7 forward/back,
> 4/6 left/right, 1/3 tilt down/up, +/− widen/narrow field of view, and 0 to reset
> that view. Change these in **F6 → Cameras → Adjustment bindings**. [Show actual
> saved mappings after a rebind and explain any unavailable adjustment.]
>
> **Customize later:** Choose **View: Advanced** for detailed feedback tuning,
> camera position sliders and telemetry options. Returning to Simple keeps your
> changes. Ordinary binding and camera shortcut rebinding are available in Simple.
>
> **Update:** Close the game and run Install.bat from the new release. Your
> bindings and settings are kept. **Uninstall:** run Uninstall.bat; personal data
> is kept unless you explicitly choose to remove it.

Link from this short guide to a symptom-based troubleshooting page. Keep source
build instructions, loader internals, compatibility evidence and force mathematics
in developer documents, while exposing actual gameplay restrictions beside the
affected option. Never make a player infer the tested game/version from git history.

The exact [handbrake capture and camera adjustment workflows](CONSUMER-CONTROLS-CAMERAS.md)
are part of this contract. A Bind button must not redirect to another Bind page;
a keyboard without a numpad must be able to rebind camera adjustments in F6.

## Release evidence

Use [the shared checklist](CONSUMER-UX-CHECKLIST.md) with actual packaged artifacts.
Exercise fresh install, update with personal bindings, unsupported/conflicting
install, failed update/recovery, and uninstall with a second mod present in a
disposable fixture. Verify first launch and setup in the game's actual runtime
separately. Fixture success does not prove text readability, live input or feel.
Check Simple on fresh installs and upgrades without a view preference, including
upgrades with an existing custom tune. Verify the explicit view choice persists
and that changing views preserves all effective settings.
