# ffbprobe

A standalone DirectInput probe that drives the first force-feedback device it
finds with scripted constant-force parameter encodings and prints, per call,
whether the driver accepted them. It settled in ten minutes what a day of
in-game testing had not (art-of-sim-rally, 2026-09-02): every encoding was
accepted by a MOZA R12, `Start()` on a fresh effect returned
`0x80040205` (`DIERR_NOTEXCLUSIVEACQUIRED`), and a non-exclusive acquisition
answered `DI_DOWNLOADSKIPPED`.

Build: `build.bat` (MSVC x64). Run with the game closed and the base powered
on; keep your hands off the rim — it applies small forces briefly.

Use it before changing an encoding, and to compare backends (ADR-0001): the
same sequence through the DirectInput and SDL2 paths, recorded against the
wheel and date in `knowledge/`.
