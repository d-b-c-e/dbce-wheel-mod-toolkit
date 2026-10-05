# Woden Rally Edge: FFB window initialization failure

[woden-rally-edge-wheel, 2026-09-16]

Observed on Woden consumer 0.2.6, pinned toolkit v0.12.0 / native 0.5.0, MOZA R12. Native log at 23:14:11 selected owned HWND 0x00992384, then replaced it with foreground HWND 0x002610E4 after InitDirectInput(hwnd=0). Wheel initialization succeeded; InstallExitGuards failed SetWindowLongPtr with Win32 error 5. Woden correctly zeroed/stopped/closed and latched the error: zero output writes for the entire drive. The managed error reported HRESULT 0, which did not describe the Win32 failure. Raw owner logs remain private in the consumer's ignored artifacts/backups.

The v0.12.0 tag and current native source both assign g_hwnd = ResolveGameWindow(...), then overwrite it with ResolveHwnd(hwnd). For zero/invalid input the latter returns the foreground window without process ownership validation. ResolveGameWindow also falls back to the passed handle when it finds no owned window. Preserve owned-window selection through initialization and reject foreign/unavailable windows before DirectInput or subclassing; propagate the actual Win32 failure when guard installation fails. Shared native correction and native ABI/window regression remain outstanding. No toolkit binary/source or release pin is changed by this report.

Woden 0.2.7 captures a visible owned UnityWndClass HWND before closing readers/enumerating, revalidates ownership and passes that explicit nonzero handle to the existing toolkit API. Missing windows wait without reader churn or latching a failure. Exit guards remain mandatory; this is not a guard bypass or retry loop. Offline tests compile the actual adapter with a fake toolkit API and verify HWND/GUID forwarding across a foreground change, watchdog/guard/initial-zero ordering, missing/lost-window rejection and cleanup. Physical recovery awaits an attended Woden run.

## Follow-up: owned HWND confirmed in Woden 0.2.7

[woden-rally-edge-wheel, 2026-09-16]

The subsequent owner run initialized the exact R12 with explicit HWND 0x001820E4 and successfully installed exit guards at 23:32:20.715. This confirms the consumer window-selection workaround at runtime. Before racing, PanicStop at 23:32:49.644 saved the consumer preference Off; it stayed Off for the entire drive (zero writes and zero failures). This run therefore does not establish physical force delivery. No panic button is bound; keyboard F8 and the panel Stop FFB button are available, but logs do not identify the trigger. The consumer preserves saved Off until explicit On. Shared native source correction remains outstanding.
