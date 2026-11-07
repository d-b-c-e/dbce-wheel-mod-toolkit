# dbce-ui.exe: game-closed setup with the F6 panel core (UI-3)

The small setup tool of `docs/architecture/2026-11/04-cross-cutting.md` F.3 (`--setup`) and
`05-ux-simplification.md` 4.2, in the hub (`dbce-project-mgmt`). It is a native Win32 app (D3D11 + Dear ImGui) that
draws the shared panel core (`native/panel`, UI-1) in a normal window, so setup looks and works exactly like F6 in the
game. There is no overlay mode: in-game UI is drawn in-process (`native/ui-host`, `native/panel`).

```
dbce-ui.exe --setup <dbce-mod.json> [--config <ini>]
```

- `<dbce-mod.json>` holds the `settings` block (the descriptor, `native/panel/dbce_panel_descriptor.h`). The window
  title is "<product> setup".
- Values live in the mod's ini, section `[Settings]`, one key per row id: `ffb.enabled=1`, `ffb.strength=50`,
  `display.screens=Surround`. Toggles are `1`/`0`, choices their text, numbers use the row's decimals, bindings their
  saved text. The ini is the descriptor's optional `"config"` (relative to the descriptor file) or `--config`.
- Each change writes only its own key at once (STD-044) through `native/settings` `saveKeys`: other lines, comments,
  BOM and line endings are kept. Opening writes nothing; there is no Apply step. The panel's own `[Panel]` view and
  page are not written.
- Left out with the game closed: the Cameras page, status lines and the frame-rate readout. "Restore defaults"
  (`<page>.restore`) and "Restore all defaults" (`help.restore-all`) work; other actions say "Start the game to use
  this."
- Esc, the panel's Close or the window's X ends it.

| File | What it is |
|---|---|
| `dbce_ui_exe.cpp` | The exe: window, D3D11 device (hardware, else WARP), the panel's input hook on its own window, values in and out of the ini. |
| `build.ps1` | Builds `build\dbce-ui.exe` (x64, /O1, static CRT; fails at 5 MB or more; about 0.65 MB), then runs `test_setup.exe`. `-NoTest` builds only. |
| `test_setup.cpp` | Starts the exe hidden on `native/panel/samples/art-of-rally.settings.json` with a copy of `fixtures\before.ini`, posts keys to its window (Right, Down, Enter, Down, Left, Left, Esc), and checks the ini equals `fixtures\after.ini` byte for byte: only `ffb.enabled` and `ffb.strength` changed. |

## Limits (2026-11-07)

- Binding capture needs the wheel read, which the exe does not do yet: a binding row shows its saved input and can be
  cleared; Select shows "Set this in the game: press F6 while it runs."
- No "Close the game first" check yet: the descriptor does not name the game's exe.
- No game reads `[Settings]` yet; the first consumer (art of rally, UI-4) adopts the key format above, or a per-row
  key mapping gets added then. Not yet in a package or `Sync-Toolkit` part; Wheelkit's 80 MB standalone helper stays
  until a package ships this.
