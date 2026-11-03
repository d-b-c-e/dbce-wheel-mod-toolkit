# Development-only panel message-delivery diagnostic

November 2: merged and qualified with fake Win32 CPU checks. See
[the reconciliation checkpoint](2026-11-02-raw-snapshot-reconciliation.md).
UNRUN below describes the original increment, not the current offline checkpoint.

Source/fixture successor of exact toolkit `fd8391f`; **native build/tests UNRUN**.
Native panel input base SHA256 `D5FB2BF1A810F871F9422E89694E9107D2E48FAA7EDB68134ACD91394993DCB3`
equals Revo CDD17C41's vendored input. That candidate, installed AB94 baseline,
force logic and neutralizer are unchanged. This is no interaction bug fix.

The October27 run rendered Controls/Help through the dev channel, but provider
F6/Help click/Escape requests produced no corresponding visible effect. The
attached HWND matched native foreground; delivery to the actual message hook
has no witness. No polling, WndProc or provider substitution is justified.

Use only the existing fresh `panel-dev.txt` channel:

- `input-trace on`: reset fixed counters and enable, capture current hook/window/
  thread and open state. Does not change panel/queue. Repeated on resets counters;
  it never resets the process-wide reporting budget.
- `input-trace snapshot`: copy a consistent snapshot under the input lock, then
  format/log outside it at the existing render/dev boundary.
- `input-trace off`: disable and report; panel/queue stay unchanged.

Default is off, with no INI option. Existing command-mtime/process-start checks
still reject stale files. Invalid modes do nothing. At most8 reports per process,
3 fixed lines per report; counters saturate at1000000. There is no allocation or
IO in the hook. Enabling/disabling remains possible after the log budget ends.

Counters: current and last attachment HWND/thread/hook identity, attach attempts/
failures/detach; nonnegative callback/HC_ACTION counts; REMOVE/NOREMOVE; root/child/
foreign/null-window/null-message categories on removed messages only. Negative
codes chain immediately; NOREMOVE never reads MSG, so its window/key categories
are intentionally unknown. Keys are only F6/Escape removed-message down/up/repeat
counts (including foreign windows); no characters or general keystroke values.
Mouse counts distinguish observed button messages and actual EV_BUTTON enqueue.
Consumed counts and open/close transitions follow the real handler. DrainInput
also observes dev-channel state transitions at its existing locked queue drain;
these are observed transitions, not a recorder of every inter-frame state write.

Actual-hook fixture: `native/panel/test_input_custody.cpp` includes production
input.cpp, substitutes hook/window/cursor APIs and dispatches fabricated MSGs.
It preserves all16 cursor-custody checks and runs the same input sequence with
tracing off/on: negative/NOREMOVE invalid pointers, null/root/child/foreign,
F6 repeat/edges, Escape open-only, Alt+Tab/Alt+F4/Windows keys, unrelated chaining,
mouse enqueue/drain, detach/late callbacks, counter reset/saturation/report cap,
logging outside the lock and failed/retried hook attachment. This proves no real
queue/provider delivery and opens no OS window/hook/device.

Source review corrected one diagnostic-only race: DrainInput's render `open`
snapshot may predate a game-thread F6 callback. TraceOpen now reads IsOpen inside
the existing queue lock, preserving the render snapshot. A fake GetClientRect
interleaves an actual F6 handler call after the snapshot and before that lock;
the assertion requires one open and zero phantom closes, and unchanged render
state. The old traceOpen(open) would record a false close. Native execution of
this regression is still pending scheduled qualification.

Smallest registered command, after source review and scheduled shared lease:
`./native/panel/build_test.ps1 -InputCustodyOnly` (x64+x86, CPU fake APIs).
The normal panel runner includes these checks too. No build has run yet.

After qualification, adopt only exact diagnostic panel files with explicit
VENDORED provenance in a separate Revo successor; preserve CDD17C41. A separately
admitted short run can distinguish absent callbacks, non-removing pump traffic,
wrong ownership, recognized key/transition, or an external render-state change.
No current source observation selects a speculative product correction.
