# Provenance: shared input shim

Moved from `d-b-c-e/outrun2-decomp`, commit `24dc0f8a` (branch `codex/outrun-wheel-parity`, the OutRun integration
line; parent `9c31d66`). The copy is exact as git stores it: each file's blob id below equals the source repository's,
and it is checked after staging. Line endings follow both repositories' `core.autocrlf` normalization. The directory
layout mirrors the source `windows/` folders, so the decomp can vendor this tree back as a plain copy.

## Authorship and terms

- Every file here is DBCE-authored. `git log` on the source folders shows commits by `d-b-c-e` only; the upstream decomp
  author (`r4dius`, remote `upstream`) has 2 commits in that repository and none under `windows/`.
- No file carries a third-party license header. `shim/sha256.hpp` is written from FIPS 180-4: only the standard's
  constants are shared.
- The files are published under this toolkit's existing MIT license (repository `LICENSE`, (c) 2026 d-b-c-e), by the
  owner's direction relayed in sana 50288.
- No decomp, game or runtime source, header or asset is included. The DLL's include closure stays inside these folders
  plus the toolkit's own `native/controls` and `native/session`.
- `native/controls` and `native/session` are the files the decomp had vendored from this toolkit: they are
  byte-identical today, so they are not copied again.

## Source variants in use (consumers unchanged by this move)

| variant | source | DLL SHA-256 | consumers | notes |
|---|---|---|---|---|
| direct provider | `codex/shim-device-path` `f49cfc55` | `9B8A036F664CF8F45CCCF82B98E5E4376F93215B60EAC50B6DB965BF498015D8` | DRIVE (rc.12 and later), the Woden private pilot | The consumers pin this hash, ABI 2 and 31 actions. |
| copied-provider bridge | `codex/art-shim-bridge` `574bb882` | `7DDC962132819EDAE76EF84CE6509F1936B8C38177A36D498B8ABBCFDCFFBCAF` | Art (installed) | Shares Art's existing native reader. Its additions from base `a9e16010`: `bridge_provider.{hpp,cpp}`, `shim_dll/dbce_shim_bridge.h`, bridge exports in `dbce_shim.cpp`, and core/provider changes (`d46096c`, `1e88feb`, `574bb88`; tests `test_shim_bridge.cpp`). **Not included here**: merging them is a reviewed follow-up. |
| integration line (this copy) | `24dc0f8a` | built by `build_test.ps1` (see its receipt) | none yet | A superset of the direct provider inside the DLL closure: an optional `front` provider in `provider_host.hpp` (default off; used by OutRun's WheelFfb bridge), read accessors in `frame_recorder.hpp`, and the absent-device rescan in `core.cpp`/`shim.hpp`. `dinput_provider.*` and `shim_dll/*` match `f49cfc55`. |

No consumer is repinned by this move. Adoption, a repin or the bridge merge each need their own review.

## Not moved

- OutRun's adapter and game-side shim code (`windows/source/shim_input.cpp`, `outrun_adapter.*`, `shim_steer.hpp`), and
  the test that needs it (`test_shim_steer.cpp`).
- `shim/wheelffb_bridge.*`, which only OutRun uses today.
- The P21 and control-host fixtures.

## Files (git blob id, source path under `windows/`)

| blob | file |
|---|---|
| `0df2a76e5bed64d070abfcc3f7a28123667ae139` | `docs/CONTROL-JOURNAL.md` |
| `b30c079104a086f0aaf6326591dcf698d5a899c5` | `docs/INPUT-SHIM.md` |
| `cbf22b36675719aae0b4b0f25c7baf6861e45bb9` | `host/control_journal.hpp` |
| `e282931da5e389f1b4ff70ca05cd63cd92e36139` | `host/control_snapshot.hpp` |
| `09856c8e8209cf68f11bc28ae9f6b95c1bf352ff` | `host/host_lifecycle.hpp` |
| `ce0d32040a0181d79ce3088800ea9282444dee05` | `host/journal_identity.hpp` |
| `38e281829141e470627384e422bf8d4b7d04fcb1` | `host/provider_host.hpp` |
| `d6336d5373949618b417768e2dcec0bf9e4bc0a3` | `host/recorder/capture.hpp` |
| `9d6b20ce5fe2d265fcf81f306827fbfc38992d8c` | `host/recorder/frame_channels.hpp` |
| `e0a046dfe77c54d882bfba89cfe2487462562cfe` | `host/recorder/frame_recorder.hpp` |
| `2d3fdab65698399c3bef1597bf98287ff42ed72b` | `host/recorder/worker.hpp` |
| `a78afe704d331569b4f62dc5ff3543de2db16bb9` | `shim/control_pump.hpp` |
| `881496aed55911f874a99f22c2451f0c4d165f82` | `shim/core.cpp` |
| `312584e5f0940516dbc530b60c866aac6d24dc74` | `shim/dinput_provider.cpp` |
| `3e039e4074fefd85ce24818e59b0420ea8e03738` | `shim/dinput_provider.hpp` |
| `c33de82687e7e8833c912afeff0ff16252ce8b57` | `shim/observer_domain.hpp` |
| `94cc00c2838042436153b954bb7be641dfd20b92` | `shim/observer_transport.hpp` |
| `caefe9468ea25eff8d4bda3d69c160e3cb243cc0` | `shim/parse.cpp` |
| `a01d8fb502895e15c138a9dfcdae10fdc2dc1a3d` | `shim/sha256.hpp` |
| `277e90c1dad867086f6f4bf743a2159e70733942` | `shim/shim.hpp` |
| `e2f2302b207260128945deaaa02c92d682166556` | `shim_dll/README.md` |
| `8df5f102acb4539d5e61395d9afaf369fbe03ba2` | `shim_dll/dbce_shim.cpp` |
| `47e264627289370ed486789e423fc76b7d6fcbdd` | `shim_dll/dbce_shim.h` |
| `ab3a662046c4ea3f86205ce15db84c06d4615443` | `shim_dll/dbce_shim_internal.hpp` |
| `d764a3dda797dee03ec90fa8ad19d0e633097c1d` | `tests/shim_fake_com.hpp` |
| `6447f2913ba5c085fda0c64dace0beeb0ca31a1b` | `tests/shim_fake_provider.hpp` |
| `12a030a2e3766943174e17eabd556ed9c97d4ce0` | `tests/test_dinput_provider.cpp` |
| `2197412a5476be86fdfd12d0071b03e161cdb7ba` | `tests/test_shim.cpp` |
| `4cbf4fe5469f905f881fc452aa4da6fbb45060d4` | `tests/test_shim_abi.cpp` |
