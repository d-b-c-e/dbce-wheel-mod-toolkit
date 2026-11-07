# d3d11-viewport-log (read-only research proxy, x86)

`d3d11.dll` placed next to a 32-bit D3D11 game exe. Every export is passed through to the system DLL. For sampled
frames it logs:
- viewports and render targets
- constant-buffer writes (first 48 floats) and VS/PS constant-buffer bindings
- the swap chain
- for each event, the exe return address plus an EBP-chain backtrace (marked `?`)

Nothing is changed or drawn. Log: `%LOCALAPPDATA%\dbce\d3d11log\d3d11log-<exe>-<pid>.txt`.

Sampled frames:
- each distinct viewport pattern that holds for two frames (max 60)
- every 1800th frame
- two frames whenever `capture.txt` appears in the log folder (the file is then deleted)

The log stops at 64 MB.

The D3D11 runtime rewrites the context's Draw* vtable slots itself, so draws are not counted. `seq` numbers the
logged events of a frame instead.

- Build: `./build.ps1` (MSVC vcvars32; checks the 51 exports). Smoke test: `build\smoke.exe build\d3d11.dll` (WARP
  device on a hidden window).
- Use: install for one run only, never alongside another `d3d11.dll`, and delete it afterwards if it is still the
  exact file you copied. First used on DiRT 3 split-screen (profile 30).
