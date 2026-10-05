# Art of Sim Rally: Simple visual hierarchy review

Reviewed 2026-09-21 with an independent UX reviewer. Baseline Art source
`1a72fd9`, installed RC12 runtime `cbcf76c`, toolkit guidance `1726a14`.
Evidence: source and retained actual RC12 3840×2160 screenshots in
`E:/Source/art-of-sim-rally-ux/results/ux-rc12-smoke-d24e3e71d58f498699f4eba1ea8c28f7/`.
No new game launch, desktop input or physical-force check was performed.

## Findings

| Finding | Evidence and effect | Correction |
|---|---|---|
| P2: ambiguous binding ownership | User report and `SettingsPanel.Axis`: loose label, device bar, calibration paragraph, generic Bind/Calibrate/Clear, then only six unscaled pixels before the next axis. Retained screenshots do not show these complete rows. | One compact bounded group per action; stronger heading; target-specific stacked actions; editor inside the same group. |
| P2: little usable content in Simple | Actual Controls screenshot shows header, status and instructions consuming most of the default 960×720 UMM host; only the first axis heading reaches the body bottom. Policy allows 190 px of page height at Auto2x. | Compact routine status and local instructions; reclaim header space in the page budget; show a complete actionable first group. |
| P2: optional navigation resembles a setting | Source uses checkboxes to reveal clutch, shifter and camera binding lists. | Show/Hide disclosure wording; prevent hiding an active edit. |
| P2: camera action split across sections | Source draws keyboard controls followed by a separate USB binding block for every adjustment. | One action group containing both assignments. |

These findings add [UX-03-G](../CONSUMER-UX.md#binding-ownership-and-useful-space--ux-03-g)
and its [acceptance row](../CONSUMER-UX-CHECKLIST.md). This is a reusable rule for
all consumers, not evidence that they have adopted it. No shared renderer is added.

Art implementation, source identity, package and deployment are tracked in
`E:/Source/art-of-sim-rally/docs/reviews/2026-09-21-visual-grouping.md` and
its `docs/LOCAL-DEPLOYMENT.md`. A source review and offline checks can establish
implementation, but **rendered acceptance remains pending** until the actual
host shows the complete initial Controls group and Setup next action, with
long names, active edits, errors, partial scrolling and custom scales checked.

## Implemented and deployed follow-up

Art source `470e398` implements the groups, local preconditions, compact header
and explicit disclosures. Independent source review closed its findings. RC13
passed all 16 local release gates and was installed with the game closed at
2026-09-22 04:15:29 UTC (September 21 local). Its six mod files and second native
copy match; six current owner/game files retain their pre-install hashes.
Current settings, UMM preferences, toolkit `358add5` and probe0.2.5.4 are retained.
The exact [consumer review](https://github.com/d-b-c-e/art-of-sim-rally/blob/main/docs/reviews/2026-09-21-visual-grouping.md)
and [deployment record](https://github.com/d-b-c-e/art-of-sim-rally/blob/main/docs/LOCAL-DEPLOYMENT.md)
separate source, package and installation evidence from the still-pending
rendered checks (KI-41). This turn did not launch the game or apply force.
