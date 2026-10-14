# Shared input shim (native/shim)

This is the input shim behind DBCE's shared controls (STD-033/036). It has three parts:
- **Core** (`shim/`): the profile reader, device epochs and rearm, and the resolver.
- **DirectInput provider** (`shim/dinput_provider.*`): nonexclusive and background. It never creates force effects.
- **Host** (`host/`): bounded worker ownership, the gated control journal, and the shared frame recorder.

`shim_dll/` puts all three behind a C ABI (`dbce_shim.h`, ABI 2, 31 actions) for managed game adapters. Game adapters
stay in their own repositories. Where this came from, the variants in use, and what was left out are in
[PROVENANCE.md](PROVENANCE.md). The full design is in [docs/INPUT-SHIM.md](docs/INPUT-SHIM.md); the journal is in
[docs/CONTROL-JOURNAL.md](docs/CONTROL-JOURNAL.md).

## Build and check

```powershell
pwsh native/shim/build_test.ps1            # x64, clang-cl, static C runtime (/MT)
pwsh native/shim/build_test.ps1 -Crt MD    # the earlier dynamic-runtime build, for comparison
pwsh native/shim/build_test.ps1 -Arch x86  # the ordinary-only DLL for 32-bit games (shim_dll/README.md)
```

The script builds and runs the targets the decomp's CMake defines:
- `dbce_shim.dll`;
- `test_shim`: Core against the scripted provider;
- `test_dinput_provider`: the provider against a fake DirectInput;
- `test_provider_host`: the in-process host (`host/provider_host.hpp`) as a consumer constructs it, with a fake
  DirectInput: a Schema 2 profile at `controls_reads` 2 (ordinary and latched values Core applies) and its refusal at
  the default;
- `test_shim_abi`: the C ABI, including the DLL's exports, with a fake DirectInput;
- `test_shim_bridge`: the copied-input bridge (x64 only).

Each build also checks the DLL's machine, its imports, and that its export names are exactly the `DBCE_SHIM_API`
declarations in `dbce_shim.h` and `dbce_shim_bridge.h`.

No game or real device is used. Output goes to `build/<arch>-<crt>/` (ignored), with `build-receipt.json`: the
architecture, the DLL's SHA-256, its runtime imports and exports, the compiler and the toolkit commit.

Requirements: Visual Studio 2022 with the C++ Clang tools (`VC\Tools\Llvm\x64\bin\clang-cl.exe`). Flags follow the
decomp's Release configuration:
- `/std:c++17 /W4 /EHsc /O2`, with `/WX` on the DLL, the host test and the ABI test;
- `NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS`;
- links `dinput8 dxguid bcrypt`.

## Static C runtime: why /MT is safe here

The consumers' current DLL imports `MSVCP140.dll`, `VCRUNTIME140.dll` and the UCRT, so a player machine needs the
VC++ 2015-2022 redistributable. A static runtime removes that requirement. It is only safe if no allocation or CRT
object crosses the boundary, and `dbce_shim.h` guarantees that:
- It is a C ABI with no callbacks, and nothing throws across it.
- Every buffer is caller-owned and bounded: text is truncated, always NUL-terminated, and `needed` reports the full
  length.
- `dbce_shim_action_name` returns static strings.
- The handle is opaque. It is created and freed only inside the DLL (`dbce_shim_stop`), and an unresolved stop leaves
  it allocated until process exit.
- `dbce_shim_open2`'s string and path arguments are read during the call.
- No `FILE*`, `std::` type, CRT handle or heap block is exchanged.

The script refuses a /MT build that still imports a C runtime DLL.

A managed adapter must not declare a returned `const char*` as a marshalled `string`. The marshaller would free the
static pointer, which is wrong under /MD and /MT alike. Read it with `Marshal.PtrToStringAnsi`.

## Developer capabilities

`dbce_shim_open2`'s latched test session (the control worker and journal) and its capture stay compiled in on x64, as
before. The x86 build leaves them out, and its `dbce_shim_open2` refuses.
They are refused unless the caller verified its run fences, physical force is latched off, and both session files are
named. Public packages expose no new entry point or UI for them (sana 50288).
