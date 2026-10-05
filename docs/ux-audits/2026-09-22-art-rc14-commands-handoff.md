# Art RC14: compact commands and native-menu handoff

Date: 2026-09-22 (owner's local date). Shared guidance baseline: `be55195`.
Art's deployment record identifies RC14 as
`0.2.6-rc.14+8d37db001aea2ff21ca30455723f604523d78ac4.clean`.
Task `01a0736d-c9ec-7c52-8357-eded394685f2` forwarded the latest owner-test
findings. This toolkit task read the deployment identity, but did not reopen
the game or independently reproduce the reported appearance/handoff failure.

## Findings incorporated into UX-1

1. **Full-width raised commands resemble section dividers.** Apply compact
   content-sized or bounded fixed widths to ordinary commands on every page,
   usually grouped right beside explanatory text. Full-row surfaces belong to
   real groups/navigation. Use compact segments for short mutually exclusive
   settings and flat links/disclosures for Advanced/Show. Narrow layouts may
   stack and expand only when reachability requires it. This extends the earlier
   binding-card rule to reset, preset, support and native-menu commands too.
2. **Global mapped-input neutrality can trap a programmatic native menu.** A
   resting wheel/pedal axis may be non-zero; an H-pattern gear can remain held
   indefinitely. For a command that transfers from the mod overlay to a native
   screen, consume the actual initiating mouse/key/control, wait for its release
   plus a short neutral-frame barrier, then explicitly release overlay ownership.
   Preserve focus, queued-pointer cancellation and cancellation guards. Unrelated
   mapped axes/buttons must not be prerequisites for releasing the destination.

The [main standard](../CONSUMER-UX.md) replaces the overly broad universal
release wording with actual-gesture protection on ordinary close and a separate
[programmatic handoff contract](../CONSUMER-UX.md#programmatic-native-menu-handoff--ux-03-h).
The [view contract](../CONSUMER-SETTINGS-VIEWS.md) and
[checklist](../CONSUMER-UX-CHECKLIST.md) carry the same requirements (UX-03-C/H).
This does not permit an initiating click/submit to activate the new screen or
remove the game's native input backend. Rendering and input policy remain
consumer-owned; the toolkit runtime binaries and HTML reference are unchanged.

## Required consumer evidence

- Inspect command sizing on all five core pages at the actual wide/default host
  and a narrow/custom-scale host. Advanced links/disclosures must read as navigation.
- Hold the initiating mouse button, key or control. The destination cannot receive
  that activation; release plus the documented neutral frames precedes handoff.
- Repeat with an unrelated pedal resting non-zero and an H-pattern gear held.
  After handoff, fresh native-menu input and return navigation must work.
- Cancel while pending and lose focus during the transition. No unexpected later
  opening, stale overlay ownership or replayed pointer gesture may remain.

These checks are consumer acceptance requirements, not tests performed by this
documentation update. Art's implementation and final package/interaction evidence
belong in its task. Earlier RC13/RC14 build or deployment checks do not establish
that these new cases pass.

Documentation validation: 72 local links and six Markdown section fragments
across eight changed files resolve; diff/whitespace checks pass. The conflicting
universal release-wait wording is removed from the current standard. No toolkit
runtime build, native smoke, game interaction or force test is needed or claimed
for this documentation-only change.
