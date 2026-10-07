# State observations used to qualify replay comparisons

iRacing's d70e4a2 read-only effect observer illustrates three reusable recording
boundaries. Its normal profileRef/no-clone behavior, writes, transitions and
bounded row overflow pass64 production-fixture assertions and five negative
controls, but nine independent reproduction/control assertions expose gaps.
Consumer detail: docs/reviews/2026-10-07-effect-observer-review.md.

1. **Change events are not coverage evidence.** Stable values generate no new
   rows, so a consumer cannot distinguish ongoing observation from an observer
   that stopped after the first scan. Record successful scan boundaries with
   clock/frame/sample/scene, validity/unknown counts and stop/disable markers.
   Enumeration/read errors and dropped records must invalidate affected windows.
   Preserve the actual cadence: one-Hz sampling cannot prove every render frame.
2. **Addresses are not object lifetimes.** A destroyed volume/profile/component
   can be replaced at reused native addresses. Raw-pointer keys and cached text
   may merge them, especially across scene changes. Include engine identity or
   generation and scene epoch; expire dead objects. Test address reuse as well
   as the easier fresh-pointer replacement case.
3. **Read validity and metadata must participate in deduplication.** Comparing
   only value/origin can swallow an identity-read failure and its recovery.
   Record unknown type/identity reads explicitly, and refresh cached names and
   identity metadata when recovered. Missing evidence is not measured stability.

Before drawing a causal conclusion from matched replay frames, pin requested
condition settings and qualify observed state over the actual capture window.
Baseline must run the same observer. Failed/absent/reenabled targets or missing
coverage produce an inconclusive comparison, not a negative result for an effect.
Keep observation separate from mutation; do not repeatedly force a flag off to
hide game re-enablement. This does not require game-specific renderer hooks in
the shared toolkit.

The prepared private harness enables the observer identically in both conditions
under owner backup/restoration, preserving the one intended A/B difference.
54 actual config/file-restoration fixtures pass, including malformed Dev sections
refused before any partial Triple write. Observer collection/coverage parsing
still awaits corrections; no game, install, physical output or new seam result
is claimed. Earlier validated trajectory/force evidence remains unchanged.

Test hygiene also matters: use a uniquely owned temporary directory or explicit
created-file inventory. A temp-name prefix alone does not authorize deleting
another concurrent fixture's logs.
