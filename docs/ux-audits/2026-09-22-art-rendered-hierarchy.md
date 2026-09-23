# Rendered Art RC13 feedback: hierarchy clarification

Date: 2026-09-22. Guidance baseline: toolkit `8d522ca`, including UX-03-G
`673d451`. Consumer baseline: installed RC13 source `470e398`.

Provenance: Art of Sim Rally task `01a0736d-c9ec-7c52-8357-eded394685f2`
forwarded the owner's findings from its rendered RC13 screenshot to the toolkit
coordinator. This is reported consumer screenshot feedback, not a fresh toolkit
game/desktop inspection. The earlier [source review](2026-09-21-art-visual-hierarchy.md)
and deployment remain historical evidence; they do not establish visual acceptance.
Art is implementing the follow-up on `codex/ui-contrast-spacing` in its own task.

| Reported finding | Shared requirement | Acceptance evidence |
|---|---|---|
| Bounded cards still read as one stack when fill contrast is weak and no border is visible | Perceptibly separate surface or visible border/separator, plus inter-group spacing; add a boundary when a subtle fill is insufficient | Actual host at normal viewing size, adjacent/partly scrolled cards, disabled controls and long names |
| Navigation resembles command buttons and consumes too much header space | Flat compact tabs, explicit selection such as an accent underline, separate focus indication; one row at the default host when labels/hit targets permit | Default host and custom-scale screenshots; no text shrinking or clipping to force one row |
| Setup repeats Controls/FFB rather than adding a useful workflow | Remove duplicate Setup/Overview; retain Setup only for distinct guided first use; keep five core pages in their established order | Fresh-use walk plus every old page ID, both views, unknown ID and repeated load/save migration checks |
| Full-width repeated commands add button noise | Short instruction on the left, compact command group on the right; stack within the same card when needed and repeat target names | Wide/narrow, partial-scroll and active-edit views with preconditions, Save/Cancel and errors visible |
| Navigation, disclosures and status use the same visual weight as actions | Raised/button styling for commands; flat selection/disclosure controls and plain status text | Selected, focused, disabled and expanded states remain distinguishable in the real renderer |

The main [standard](../CONSUMER-UX.md), [view contract](../CONSUMER-SETTINGS-VIEWS.md),
[first-use guide](../CONSUMER-SETUP.md) and [checklist](../CONSUMER-UX-CHECKLIST.md)
are reconciled together. Setup is no longer mandatory, but complete basic setup
and recovery in Simple remain mandatory. Numeric page positions must not be
reinterpreted after removing Setup: preserve stable IDs or use an explicit,
versioned, idempotent migration. No binding, tune, device or output preference
changes as a consequence of the navigation change.

The [HTML reference notes](../reference/README.md) explicitly identify its older
six-page/button-style presentation as superseded. The HTML itself is unchanged;
its previous browser checks are not new acceptance evidence. This documentation
change neither modifies toolkit binaries nor certifies another consumer's UI.
Art's final source/package/deployment/rendered results belong in its own task.

Validation: all 80 local links and five Markdown section fragments across the
10 changed documents resolve. Diff/whitespace checks pass. A consistency search
finds no remaining mandatory six-page or unconditional Simple Setup default in
current normative guidance; conditional Setup and historical reference statements
remain intentional. Consumer rendering and migration tests remain consumer work.
