# Native INI settings saves

`dbce_ini_settings_win.hpp` supplies `dbce::settings::save(path, section, changes)` for existing native INI files. Changes are an ordered list of ASCII key/value pairs. One atomic replacement saves a whole camera draft; untouched bytes, whitespace, comments, BOM and line endings survive. UTF-8/ANSI and UTF-16LE are supported. UTF-16BE, embedded NULs, oversized files, missing/duplicate target sections, duplicate owned keys and malformed lines are refused. An absent key is added inside the existing section. The function returns false on failure; callers show a save failure and keep the live draft.

The caller serializes writers and excludes external editors while running. The file handle denies content writes during read/replace; an identity check refuses an externally replaced target. This does not provide a transaction across independently renaming writers. Intended first consumer: Gravel, whose external editor refuses a running game. No device, game, force or display APIs are used.

Offline fixture (use a disposable output folder):

```powershell
& E:\msys64\mingw64\bin\g++.exe -std=c++17 -Wall -Wextra -Werror -static -o build/ini-settings-test.exe native/settings/test.cpp
& build/ini-settings-test.exe build
```

The fixture checks exact UTF-8/BOM/UTF-16LE output, unknown/comment retention, absent-key placement, batched updates, duplicate/malformed refusal and disk replacement/locked-file preservation. It does not establish game UI behavior.
