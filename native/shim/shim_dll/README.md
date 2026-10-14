# dbce_shim.dll: the shared input shim for managed game adapters

The reviewed Core and DInputProvider (`../shim`) and ProviderHost (`../host`), behind the
experimental C ABI in `dbce_shim.h` (ABI 2, not a stable-ABI promise; Astra 16982/17050). The first consumer is the
#DRIVE Rally mod (C#, loaded like the toolkit's `WheelFfb.dll`). OutRunDesktop links the same sources directly.

- The game's input thread opens a handle and makes every call on it. Each tick returns one Core frame: the normalized
  action values, statuses and edges, the frame a game adapter consumes. Binding resolution, normalization, rearm and
  availability stay in Core; a managed adapter never re-maps devices.
- Fixed layout: `dbce_shim_frame` is 296 bytes, naturally aligned and asserted at build time. `dbce_shim_frame_size()`
  lets the caller check its own declaration. Results are bounded codes; text goes into caller buffers (truncated,
  terminated, with the needed length).
- Lifetime: `dbce_shim_stop` runs the host's bounded P21 stop on the input thread. A resolved stop frees the host. An
  unresolved one leaves it alive until process exit, never torn down under a running worker. The module pins itself
  at the first open, so no FreeLibrary can unload code a worker may still run.
- Version 1 is ordinary (not latched): no control worker, no capture, never a force effect, non-exclusive reads.
  The next seams are latched test sessions (the existing control worker and journal) and the shared recorder (the
  frame producer with game channels from the adapter).

Build and test from the toolkit root with `pwsh native/shim/build_test.ps1`.
`test_shim_abi` runs the ABI over fake COM and checks the built DLL's exports. `test_shim_bridge` runs the copied-input
bridge: its provider, Core over it, and its exports in memory.

## Declared extras (extension 2, additive)

A game can declare destinations beyond the 31 common actions, such as Truckful's camera look stick or a nitro button.
`dbce_shim_extras.h` declares six exports. The design and its reviews are in the hub's
`artifacts/extra-actions-design-20261014`.

- **Unchanged:** ABI 2, the 296-byte frame, the 31 actions and every existing export. A handle opened without a
  declaration behaves exactly as before.
- **Declaration.** ASCII text of at most 4096 bytes, passed with its length to `dbce_shim_open_extras`:
  - a `game <id>` line;
  - an optional `contexts observed|advisory <names>` line;
  - one `<game>.<name> digital|pedal|centred|stick circle|square [contexts=...]` line per extra.
  At most 32 scalar slots, a stick taking two.
- **Profile.** The same `dbce-controls.ini` carries an `[Extras]` section after `[Controls]`, holding `Schema`, `Game`,
  `Declaration` (the declaration's SHA-256) and an optional `Revision`. Pinned DLLs ignore the section.
  - Each centred component is either one axis, or a `.neg`/`.pos` pair of buttons or hat directions.
  - A conflicting or malformed extra is invalid on its own and never changes the 31.
  - Extras-only devices come after the common device table.
- **One tick.** `dbce_shim_tick_extras(handle, contexts, frame, extras)` reads the devices once for both frames.
  `dbce_shim_tick` refuses an extras handle, and `dbce_shim_extras` only reads the cached frame by seq.
- **Contexts.** An observed context that is not active gives `ContextInactive` (14), and the extra rearms when the
  context returns. Advisory contexts never gate.
- **Status.** `dbce_shim_extras_info` reports each extra's plan, slots and inputs, for F6 and diagnostics.
- **`acceptedSchema`** in `extras_info` is the highest `[Extras]` Schema the open reads. It is 1 through the ABI, so a
  writer emits Schema 1 and no `cal=`; a Schema 2 section is refused as a whole, never half-applied.
- **Scope.** Ordinary input only, on x86 and x64. No latched control, injection or capture of extras yet, so no
  recording claims to hold them.

`test_extras` covers the grammar, the plan and Core's extras tick. `test_shim_abi` covers the exports on both
architectures.

## x86 build: ordinary only

`pwsh native/shim/build_test.ps1 -Arch x86` builds `build/x86-mt/dbce_shim.dll` for 32-bit games, such as Horizon
Chase Turbo on Unity's Mono 2.0. It uses the same clang-cl for `i686-pc-windows-msvc` under vcvars32.

It serves the ordinary path only: Core, the DirectInput provider and ProviderHost, opened with `dbce_shim_open`. The
control journal, its observer transport (`observer_transport.hpp`, qualified for x64 only, assert unchanged) and the
frame recorder are not compiled in (`shim/ordinary_only.hpp`, `DBCE_SHIM_ORDINARY_ONLY`). The x64 build is unchanged.

| | x86 |
| --- | --- |
| Exports | The same 20 names, undecorated `__cdecl`. An x86 caller must declare every function as cdecl; in C# that is `[UnmanagedFunctionPointer(CallingConvention.Cdecl)]`, because a delegate defaults to StdCall. |
| `dbce_shim_open`, `tick`, `start_report`, `device_reports`, `profile_info`, `profile_action_info`, `stop` | As on x64. |
| `dbce_shim_open2` | Always `DBCE_SHIM_REFUSED`, with the reason in `why`, before any DirectInput is created. |
| `dbce_shim_bridge_open` | Always `DBCE_SHIM_REFUSED`, with the reason. With no bridge handle, the other bridge calls refuse every handle (`DBCE_SHIM_BAD_ARGUMENT`), as a direct handle does on x64. |
| `dbce_shim_record` | `DBCE_SHIM_REFUSED` for every live handle. |
| `dbce_shim_open_args` | 80 bytes, which `dbce_shim_open_args_size()` reports. The 128-byte struct is x64's; nothing on x86 accepts it. |
| `dbce_shim_frame`, `dbce_shim_action`, bridge structs | Identical to x64 (296, 8, 144 and 232 bytes, with the same offsets). |

`test_shim`, `test_dinput_provider` and `test_provider_host` run their ordinary cases on x86. `test_shim_abi` runs
the ordinary cases plus the x86 refusals, and checks the built DLL's exports, its `open_args` size and its open2
refusal. The receipt (`build/x86-mt/build-receipt.json`) records `arch` and `ordinaryOnly`, for a reviewed pin.

## Pure profile inspection (optional export)

`dbce_shim_profile_action_info(profile, len, action, text, len, needed)` (since `5b96ba8`) reads one action's
configured binding from exact profile bytes, using Core's parser and binding admission (`plan_profile`). It has no
handle, host, provider, file or device. The handle-based `dbce_shim_profile_info` is unchanged. Profiles are read at
controls schema 1, as by the DLL's hosts, so a Schema 2 profile is reported invalid. The identity it returns is
configured, not evidence of connected hardware (`../docs/INPUT-SHIM-PROFILE-INSPECTION.md`).

## Copied-input bridge (extension 1, additive)

The same DLL also serves a consumer that already owns its device handle and copies each read in. Art's native reader
(the toolkit's `WheelFfb.dll`) is the first such consumer. `dbce_shim_bridge.h` declares seven exports:

- `dbce_shim_bridge_abi_version`, `dbce_shim_bridge_device_size` and `dbce_shim_bridge_sample_size`;
- `dbce_shim_bridge_open`;
- `dbce_shim_bridge_devices` and `dbce_shim_bridge_devices_with_paths`;
- `dbce_shim_bridge_tick`.

The design and its checks are in `../docs/COPIED-INPUT-BRIDGE.md` (copied from the OutRun branch; see `../PROVENANCE.md`).

- **Unchanged:** base ABI 2, the frame, the open arguments and every existing export. A bridge handle refuses
  `dbce_shim_tick`, and a direct handle refuses the bridge calls.
- **Flags:**
  - `DBCE_SHIM_LATCHED` (since `f38b855`) takes the base ABI's verified fences and process force mute. It writes the
    same control journal with `inputSource: "external-physical-input@1"` on every row. Copied input stays physical
    evidence; only accepted control batches are injected.
  - `DBCE_SHIM_CAPTURE` is refused: the capture format has no external sample provenance.
  - Any other flag is a bad argument.
- **Journal reader:** `../tests/control_journal` builds the production journal sink and reads it back with the strict
  reader, which accepts a header `inputSource` only with that exact value on every row.
- **No device calls:** `shim/bridge_provider` holds memory leases only. It makes no DirectInput or COM call and never
  reacquires; the external owner recovers its device.
- **Device adoption:** the bridge goes through the same Core as DirectInput, including the absent-device rescan.
  - A device batch published after an empty scan is adopted at the next scheduled scan
    (`CoreConfig::missing_rescan_us`, 250 ms by default). That is a scheduling delay, not a wall-clock guarantee.
  - A lost handle is closed and looked for again at once.
  - Freshness uses the source's original read time, not the copy time.

## Opening is asynchronous: tick before admitting input

Successful `dbce_shim_open` or `dbce_shim_open2` creates the host; it does **not** mean a
device is open or that its identity has been adopted. The worker opens devices, then a
`dbce_shim_tick` on the owning input thread adopts those handles into Core. Reading
`dbce_shim_device_reports` only reads the current snapshot. Waiting and polling reports
without ticking cannot complete this handoff.

A game adapter's startup sequence is:

1. Open on its input thread, verify Core's profile result, and enter an opening state.
2. On each input update, call `dbce_shim_tick` on that same thread and check its result.
   Keep game-facing shim output neutral while opening, including any virtual-pad
   output; do not block the game thread waiting for the device worker.
3. Read device reports **after** the tick. Validate the required owned devices and their
   current identities, complete the consumer's isolation checks, then admit input.
   An opened device still does not make every action Live: honor Core's action statuses
   and neutral/rearm rules. An empty interface path is still unavailable identity.
4. Bound opening with a consumer-defined deadline. Stop on the owning thread on timeout,
   native failure or a changed game/input context; report the specific reason. Keep the
   window, profile and game-state checks active throughout opening, not just at Open.

An unopened initial report is expected, not proof that a connected wheel is absent.
This affected iRacing's immediate post-Open ownership check (2026-10-13); the correction
belongs in ordinary startup as well as diagnostics. The contract is the same for both
Open calls; test injection must remain a separate, explicitly gated capability.

Existing source fixtures demonstrate the sequence without hardware:
`native/shim/tests/test_dinput_provider.cpp::test_path_epochs` publishes a worker-opened handle and
checks that only the next Core tick adopts its path;
`native/shim/tests/test_shim_abi.cpp::test_lifecycle` observes an empty initial report, ticks until
ordinary input is Live, then observes the owned device. These fixtures establish the
native contract, not a consumer's in-game qualification.

## Device report interface identity

`dbce_shim_device_reports` adds `interfacePath` to each JSON device row. It is the exact UTF-8
`DIPROP_GUIDANDPATH` value read from the provider's existing owned DirectInput handle; no extra device is opened or
acquired to obtain it. The fixed C ABI structs, frame layout, action list and ABI version stay unchanged.

- Before the first input tick adopts an open, and whenever that epoch is missing or read-failed, the string is empty.
  Call after `dbce_shim_tick` to observe adopted device state; the report immediately after open is only a startup snapshot.
- An absent, failed, unterminated or invalid UTF-16 property query produces an empty string. Legacy input still works.
  Empty means identity unavailable, never permission to match by name, product alone, enumeration index or a GUID
  belonging to another input API.
- Successful reacquisition of the same owned handle carries its path into the new epoch. A closed handle's path is
  discarded; a new open replaces it even when the new property query returns no path.
- Rewired's Windows RawInput instance GUID differs from DirectInput's. Consumers can join the live path and product to
  the exact RawInput extension path/product, using their existing `\\?\` / `\??\` path-prefix normalization. Require a
  unique match and do not merge native input until isolation is established. Do not retain a path from an older epoch.

`test_shim`, `test_dinput_provider` and `test_shim_abi` cover worker publication, loss/reopen, same-handle reacquisition,
failed metadata reads, UTF-8 preservation and JSON escaping. COM counters prove one device open and no effect calls.
