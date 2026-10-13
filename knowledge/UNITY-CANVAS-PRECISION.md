# Unity HUD precision away from the world origin

[Super Woden Rally Edge, 2026-10-13]

Moving overlay canvases to a dedicated camera can center a HUD across triple
screens, but the camera's position and canvas depth affect numerical precision.
Do not copy a far-away camera with a one-unit canvas plane without considering
that scale. This can affect static text and gauge ticks even when the camera
itself does not move.

## Measured Woden case

The HUD camera stays at `(0, -10000, 0)`, vertical FOV 60 degrees, near 0.1,
and the center viewport. The old canvas plane was 1 and far plane 10. At
Y=-10000, adjacent float32 values are 1/1024 apart. Projected into a 1440-pixel
display at plane 1, that world-space step corresponds to about 1.218 pixels.
At plane 100 it corresponds to about 0.01218 pixels. This calculation identifies
a precision risk; it does not alone prove Unity's actual batching/render path.

Matched private builds changed only the selected canvas plane/far geometry to
100/110. During driving, native-pixel captures covered 4.928/4.939 seconds and
saved 178/190 frames. Twelve adjacent saved-frame pairs spread across each
interval measured static gauge ticks and the DeCastro, FUEL and TEMP labels.
Maximum measured vertical displacement in those four regions was
2.457/2.295/2.154/2.152 pixels with the old geometry, versus
0.033/0.040/0.012/0.022 pixels with the new geometry. Retained camera and canvas
state rows show unchanged pose, viewport and canvas ownership during each crop.
These are sampled game-window frames, not a continuous physical-scanout result.

Both larger controls runners failed later when the car respawned, so their
overall result remains failed and their post-stop screenshots are absent.
The crop intervals precede that failure and support the narrower HUD diagnosis.
The source change can use that evidence without rerunning unrelated controls.

Private evidence index: portfolio hub
`artifacts/woden-private-apply-20261013/hud-crop-roi-03`, with capture hashes;
`hud-crop-ab-live-03` retains both runner failures and exact owner restoration.
Woden `TripleHudDepth` / `HudDepthChecks` preserve the projection calculations.
Toolkit `ca2680c` adds bounded native-pixel WGC crops when full-window PNG
encoding cannot retain enough source-time coverage.

## Reuse in another game

- Keep camera pose/viewport, render pipeline, masks and original canvas state
  distinct from the depth choice. Moving the plane farther away retains ideal
  screen-space layout because Unity resizes a camera-space canvas with depth.
  Verify that assumption for the consumer's actual canvases.
- Keep the canvas within near/far bounds and restore each original plane,
  render mode and camera when returning to the game's rendering.
- A larger far plane sees a larger volume. Inspect what else that camera can
  render; a UI-only mask and an all-layer menu camera are different cases.
- Measure static marks/text, avoiding moving needles. Retain source timestamps,
  original and crop dimensions, frame hashes, drops and enough samples to cover
  the symptom. Do not interpret the requested capture duration as saved coverage.
- Avoid coupling a pure presentation correction to wheel-force tuning or input
  lifecycle changes. State which consumer was actually observed.

DRIVE `TripleView` and iRacing `TripleView` currently share the old
Y=-10000/plane1/far10 arrangement. DRIVE `MenuCentre` also uses it, with an
all-layer mask. These are source-identified follow-ups, not measured defects or
automatically qualified fixes in those games. Share any extracted geometry
policy while leaving Unity/game hooks in their adapters.
