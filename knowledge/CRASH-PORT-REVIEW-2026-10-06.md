# Shared crash port: differences from the accepted Art reference

## Follow-up on 9ddd8e6 / iRacing 19db938

The corrected shared detector passes the four previously failing actual-Art
comparisons (stale contact, 150 ms gap, duplicate tick and teleport). Independent
`CrashCue.Requests(.5)` parameters match Art's actual mixer fake sink. Eleven
independent oracle checks and all 92 toolkit tests pass. The sampled delivery is
now explicitly `crash-constant-fallback@2`; its 25 Hz DC-alias reproduction is
fixed, but it remains separate from native Art-equivalent effect delivery.

iRacing's consumer record/analyze contract still needs work: it trusts recorded
crash output rather than reconstructing it, ignores the enabled analysis-only
Grip stream, rejects the unchanged legacy Classic source fingerprint, lacks a
car-change Grip reset marker, and consumes raw contact inputs on the first recorder
read. Consumer suite 347 passes; nine additional frozen-source cases reproduce
these gaps. The 3,001-row legacy reference replays exactly but fails one identity
check. Runtime implementation remains with Claude; DRIVE adoption and physical
cue qualification are not approved by these offline results.

Detailed review: iRacing repository
`docs/reviews/2026-10-06-grip-contract-followup.md`. Frozen private evidence:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/iracing-grip-review-19db938-20261006`;
547 files hash-verified, manifest SHA-256
`4360F7B02D52C6918533CC96CCB284A7DBA991EB3CDB9D2A9F43039F43059831`.
This follows the original findings below; it does not overwrite their evidence.

## Original review of 8191840

Review of toolkit `8191840` and iRacing integration `1ba711f`, following the
owner's request to carry Art's crash/normalization behavior into the family.
Claude owns implementation; this review changes no runtime or installed tune.
All execution used frozen managed sources and a fake effect sink, with no game,
device load, acquisition or physical output.

## Fix before calling the port equivalent

1. **Detection continuity (high).** `CrashDetector.Track(double)` omits Art's
   motion continuity (position versus prior velocity, 5 m residual), does not
   reset on duplicate time, and allows 250 ms gaps where Art allows 100 ms.
   `Observe` omits the 50 ms maximum contact age. Four actual-source cases
   produce a full-intensity shared cue versus Art's zero: contact 800 ms after
   last tracking, a 150 ms physics gap, a duplicate tick, and a 50 m teleport.
   Supply the shared detector with the motion frame or an explicitly validated
   continuity result; retain contact age and clear the active cue when the
   epoch breaks. Merely reimplementing speed/angle thresholds is insufficient.

2. **Delivery and waveform (high for equivalence/normalization).** Art's actual
   `ImpactMixer`/`LandingFeedback` asks a fake sink for an independent +0.50
   constant lasting 120 ms and amplitude 0.25, 25 Hz sine lasting 250 ms with
   final 150 ms fade (phase zero in `ImpactController`). The new `CrashCue`
   produces +0.75 at onset by switching to cosine and adding both to the main
   steering command. At steering +0.8, the clamped onset changes the command by
   only +0.2; at -0.8, by +0.75. At a 25 Hz tick cadence, the first 100 ms samples
   are all +0.75: the rattle aliases to a constant. At 50 Hz they alternate
   +0.75/+0.25. Matching durations does not establish the accepted force shape.
   Preserve independent, finite effect requests and native scheduling using
   the consumer's single output lifecycle. If a constant-output fallback is
   retained, give it a distinct model/quality label and qualify it separately.
   Drivers can still limit the total physical output of independent effects;
   these tests establish software requests, never measured torque or headroom.

3. **Consumer observation (source review).** iRacing `CrashHook` passes road=false
   unconditionally, so its angle filter is not the shared detector's road
   exclusion. It must either classify the collider or explicitly document and
   record unavailable classification; do not claim the complete Art rule.
   `MixCrash` computes only while FFB enabled, and recordings contain only
   `crash.Cue`/`crash.Count`, without raw contact/eligibility/settings/epoch data.
   `RequestedForce` still describes Classic while the dispatched command can
   contain Grip and the crash mix. `DeliveredOutput` is set before crash mixing.
   This extends the existing selected-model recorder defect: a muted playback
   cannot reconstruct or validate the actual cue from those fields alone.

Keep the native compatibility finding from the inbox: iRacing's 1760aaa
watchdog/initialization-lock fix is missing from Art's old v0.15 native source.
Do not solve missing burst exports by replacing the native binary blindly.

## Reproduction

Immutable private evidence:
`%LOCALAPPDATA%/Dbce/StagePlayback/SessionEvidence/shared-crash-review-8191840-20261006`.
Contains copied Art `ddabada` and toolkit `8191840` sources, a standalone
`Review.csproj`, `Program.cs`, binaries, build log, JSON results and source
identities. All 34 files independently hash-verified; manifest SHA-256:
`BFD0E7F1C92585E9F2FFA5D5D57595B44D505B6D81481402AC831C6C411F0610`.
Copy to a scratch folder and run `dotnet run --project Review.csproj -c Release`.
Eleven checks reproduce the differences; they assert the old behavior and must
be converted to corrected expectations when fixing production. Copied Art
structs produce twelve expected unassigned-field compiler warnings in this
small fixture; production build quality is not inferred from that fixture.

The earlier full Art cue reference remains at
`SessionEvidence/art-gold-reference-20261006-01` (12 cases / 63 checks). Use that
actual effect-request contract as the migration oracle, then collect matched
event data and retain physical acceptance for the owner. The owner is away:
no physical R12 output is authorized during this work.
