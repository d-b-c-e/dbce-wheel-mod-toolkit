# Shared settings presentation, first slice

`SettingsPresentation.cs` is MIT source intended to be vendored into managed consumers. It depends only on the
base library, and may be compiled by Unity Mono and ordinary .NET hosts. It neither opens devices nor writes files.

The first DRIVE/Wheelkit UX pilot shares player-facing axis/button/hat names and explicit numeric edit rules.
Hardware indices in stored profiles remain zero based; buttons and hats shown to players are one based. Numeric
values are finite, clamped and stepped from the minimum. Never normalize a saved value merely while displaying it.

Copy exact source with its license and record the source commit/hash in the consumer. This small slice is not the
full mod-published setting descriptor/edit-session contract; see `docs/ux-audits/2026-10-11-wheelkit-drive-review.md`.

## Recording status

`SessionStatusPresentation.cs` owns recording/playback states, American English
wording, elapsed time and center-display placement. `unity/SessionStatusHud.cs`
is the optional Unity IMGUI renderer; it must not be added to a portable managed
assembly. Vendor exact source plus this repo's MIT license and a commit/hash pin.
Native/WPF adapters use the same state contract with their own renderer.

For IL2CPP games whose generated interop lacks managed `GUIStyle` setters,
`unity/StrippedSessionStatusHud.cs` supplies the same presentation/layout with
a host-provided `Action<GUIStyle, int, bool>` style configurator. The host must
pass its verified font-size and wrap setter. This variant avoids `GUI.depth`,
`GUI.skin` and `GUIStyle.CalcHeight`; it preserves matrix, color and enabled
state around drawing. Its fixed minimum height is sized for two wrapped lines;
check the text visually at the host's actual screen resolution. The ordinary
Mono renderer and existing consumer pins remain separate.

Game adapters provide real lifecycle state, output mute state and elapsed time,
and show only a stop action that the host actually supports. **Saved means the full artifact seal
succeeded**, not a key press or signal footer. An error never displays Saved.
`Limited` is a distinct warning for a time/size/row-limited partial capture;
retained or sealed partial data does not turn that outcome into Saved.
Render through an always-visible hook, outside the F6/settings window. The
component does not start/stop a recorder, write config, enable forces or register
global input. Public builds that exclude internal recording omit the adapter.
The host controls OnGUI ordering; setting and restoring GUI.depth inside the
renderer does not establish ordering between MonoBehaviour callbacks.

Shared acceptance: an ordinary recording shows REC + timer + stop hint through
title, menus, loading, driving and pause; on triples it stays on the center
display; the host's shown stop action closes the recording without closing the game; Saving remains until
completion; Saved/Incomplete agree with the actual sealed files. No armed session
means no banner or recording hotkey behavior. Verify the installed consumer, not
just these source tests (`dotnet run --project tests/SessionStatus`).
