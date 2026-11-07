# dinput8-ffb-log (muted research proxy, x64 and x86)

`dinput8.dll` placed next to a game exe; every export is forwarded to the system DLL. It logs what the game does with
DirectInput:
- devices it enumerates and opens (calls made by DirectInput or the Steam overlay on the game's behalf are not
  re-logged)
- cooperative level, data format and properties it sets
- every force-feedback effect it creates (type, gain, direction, magnitude or condition values, creating code address)
- parameter updates: the first 20 in full, then one summary per effect per second (count, magnitude range, last
  value)

**Muted:** `IDirectInputEffect::Start` and `DIEP_START` are swallowed and `Escape` is refused, so nothing ever plays
on the wheel. Log: `%LOCALAPPDATA%\dbce\dinput8log\dinput8log-<exe>-<pid>.txt`.

**Retype switch:** when `dinput8log-retype.txt` sits next to the game exe, devices that report themselves as
DI8DEVTYPE_1STPERSON (the MOZA R12 base does) are shown to the game as DI8DEVTYPE_DRIVING in enumeration,
GetDeviceInfo and GetCapabilities, the same fix Gravel's mod uses. This tests whether a game then accepts the wheel.

- Build: `./build.ps1` (MSVC; `build\x64` and `build\x86`, six exports checked).
- Smoke test (no force on the hardware: the device is never acquired, the effect has zero magnitude and Start is
  swallowed): `build\x64\smoke.exe build\x64\dinput8.dll`.
- Use: one run at a time, never alongside another `dinput8.dll`; delete it afterwards if it is still the exact file
  you copied. First used on Rush Rally 3 and Rush Rally Origins, 2026-10-07 (profiles 21, 22).
