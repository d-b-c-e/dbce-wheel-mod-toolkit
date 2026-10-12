# Shared settings presentation, first slice

`SettingsPresentation.cs` is MIT source intended to be vendored into managed consumers. It depends only on the
base library, and may be compiled by Unity Mono and ordinary .NET hosts. It neither opens devices nor writes files.

The first DRIVE/Wheelkit UX pilot shares player-facing axis/button/hat names and explicit numeric edit rules.
Hardware indices in stored profiles remain zero based; buttons and hats shown to players are one based. Numeric
values are finite, clamped and stepped from the minimum. Never normalize a saved value merely while displaying it.

Copy exact source with its license and record the source commit/hash in the consumer. This small slice is not the
full mod-published setting descriptor/edit-session contract; see `docs/ux-audits/2026-10-11-wheelkit-drive-review.md`.
