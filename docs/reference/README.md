# Wheel settings interaction reference

Open [wheel-settings.html](wheel-settings.html) in a browser. It is a standalone,
offline visual reference for [UX-1](../CONSUMER-UX.md), especially the
[Simple/Advanced contract](../CONSUMER-SETTINGS-VIEWS.md) and
[binding/camera workflows](../CONSUMER-CONTROLS-CAMERAS.md). No build, server,
download or dependency is needed. The HTML is a design reference, not a shared
renderer or an implementation to inject into a game.

It uses example devices and values. **It does not connect to hardware, send input,
apply force, transmit telemetry, launch a game or create support/recording files.**
Its content security policy blocks network connections. Browser local storage
remembers the demo's explicit view choice and example settings. Preview scenarios
and Reset browser demo affect only that storage.

## Reference decisions

- Simple opens on Setup by default. The stable View selector, six core pages,
  Close and Stop FFB remain visible; Driving appears only in Advanced.
- The same data backs both views. Advanced keeps the basic controls first and
  adds tuning below. A custom tune has a quiet summary in Simple; switching view
  never resets the tune or changes FFB On/Off.
- Four axis rows stay visible in Controls, including Handbrake (axis). Button
  groups start collapsed. Bind starts the relevant operation directly; cancelling
  keeps the saved assignment. A sample-axis button stands in for real hardware
  capture and is explicitly marked as a demo control.
- Axis/button handbrake bindings coexist. Full basic calibration stays in Simple.
  This reference does not simulate a game's analog or digital input route.
- Camera adjustment bindings are on demand in Simple. They use the family numpad
  defaults and support keyboard rebinding. Pose sliders are Advanced and select
  one mount at a time. No camera image or game movement is simulated.
- FFB uses Off/On, a steering-following dropdown and Strength in Simple. Stop FFB
  saves Off. Example force numbers are **not universal recommended defaults**.
- Telemetry exposes the actual example destination, with an Advanced shortcut
  for atomic connection edits. Active recording and its stop action appear in
  both views. The reference's Sending label is explicitly an example state.
- Capture, calibration and connection edits lock view changes. Keyboard focus,
  Esc cancellation, timeout and conflicting bindings are represented. Browser
  keyboard behavior is not evidence for any Unity/native consumer's input path.
- Dark opaque surfaces, 16 px body / 14 px help / 20 px headings at normal scale,
  8 px spacing increments, fixed navigation and internal content scrolling.
  Help provides UI scale; high-resolution viewports use a larger base scale.

## Preview walks

The Preview scenarios control sits outside the product panel. Use it to inspect
the next incomplete setup step, a disconnected saved wheel, hidden custom FFB
tuning, a save failure, or an active recording.

1. Reset the browser demo. At 1280×720, inspect Setup without scrolling.
2. In Simple Controls, bind Handbrake (axis), use the sample axis, inspect the
   provisional calibration, then Cancel. The old Handbrake (button) is unchanged.
   Repeat and Save calibration. Expand Driving buttons to inspect both bindings.
3. In Simple Cameras, expand Adjustment bindings. Bind a movement key, cancel a
   second capture, and try assigning an already-used camera key. Restore numpad
   defaults changes shortcut bindings only.
4. Use the custom-tune scenario. Review in Advanced, change Smoothing, return to
   Simple and reload. The value, explicit view preference and FFB Off remain.
5. In Advanced Telemetry, edit host/port. View changes remain disabled until Apply
   connection or Cancel. Cancelling keeps the previous destination.
6. Inspect all pages, Close and Stop FFB at 720p and high resolution. F6 toggles
   the panel; F8 saves FFB Off. No force is produced in any scenario.

## Verification record

Checked overnight 2026-09-16–17 with headless Chromium 149.0.7827.55:

- All six Simple pages and seven Advanced pages fit horizontally at 1280×720;
  navigation, view selector and footer stay inside the viewport. Simple Setup's
  introduction, bars and next action fit without content scrolling.
- Setup fits at 3840×2160 with a 24 px base size. The footer remains visible at
  1280×720 with the explicit 150% UI scale. Screenshots were visually inspected
  for Setup, Controls, FFB, camera layout and handbrake calibration. A long device
  name wraps without covering Bind, Calibrate or Clear.
- Headless interaction checks passed for direct handbrake capture, Save/Cancel
  preserving the independent button binding; camera rebind/conflict/cancel/defaults;
  Simple/Advanced persistence, custom tuning summaries, saved FFB Off, unknown-view
  fallback, and returning from Advanced-only Driving to Setup.
- Atomic connection Apply/Cancel, edit-time view locking, missing Steering target,
  keyboard activation/focus retention, a rebound Settings key, reserved-key
  rejection, and F6/F8 behavior passed. No JavaScript page errors were reported.

The accompanying coordinator report records the exact source revision and hashes.
These checks exercise browser state and layout, not any consuming mod. The 15-second
capture timeout is implemented but was not waited out in the automated walks.
Consumer screenshot/readability, controller navigation, real capture, camera
movement and force behavior remain **Not tested** by this HTML reference. Each
consumer still needs its own placement inventory and packaged interaction evidence.
