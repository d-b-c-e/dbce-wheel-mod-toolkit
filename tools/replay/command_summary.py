"""Offline command-summary diagnostics; no device, game or native-library APIs."""
import argparse
import hashlib
import re
import sys
from pathlib import Path

import replay_case as replay

MAX_SUMMARY = 1024 * 1024
METRICS = ("peak", "rms", "capOccupancy", "duration", "duty", "slew", "shutdownZeros")
UNITS = "normalized signed command [-1,1]; no Nm or delivered torque"
IDENTIFIER = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.:@/-]{0,127}\Z")


def read_summary(path):
    with Path(path).open("rb") as source:
        data = source.read(MAX_SUMMARY + 1)
    replay.require(len(data) <= MAX_SUMMARY, "summary exceeds bound")
    return replay.decode(data), hashlib.sha256(data).hexdigest()


def metric(value, unit, definition):
    return {"status": "supported", "value": value, "unit": unit, "definition": definition}


def woden(summary, artifact_sha256):
    """Adapt the verified model-3 synthetic controller summary, not recordings."""
    replay.digest(artifact_sha256)
    replay.require(type(summary) is dict, "summary must be an object")
    replay.require(summary.get("Schema") == "dbce.woden.ffb-regression-summary@1" and
                   summary.get("Kind") == "synthetic-production-controller-requests" and
                   type(summary.get("ModelVersion")) is int and summary["ModelVersion"] == 3,
                   "unsupported producer/schema/model")
    replay.require(summary.get("Units") == UNITS, "unsupported command units")
    for field in ("HardwareOpened", "PhysicalTorqueMeasured", "CrossGameFeelEquivalence"):
        replay.require(summary.get(field) is False, "unsupported evidence boundary")
    replay.digest(summary.get("SharedFfbSha256"))
    replay.number(summary.get("SampleSeconds"), .02, .02)
    expected_options = {"Strength": 50, "PeakPercent": 25, "LoadReference": 6000,
                        "SlipScale": .35, "SmoothingMs": 35, "Damping": .05, "Invert": False}
    replay.require(summary.get("FixedOptionsExceptStrength") == expected_options,
                   "unsupported model options")
    for key, expected in expected_options.items():
        actual = summary["FixedOptionsExceptStrength"][key]
        replay.require(type(actual) is bool if type(expected) is bool else type(actual) in (int, float),
                       "invalid option type")
        if type(expected) is not bool:
            replay.number(actual, expected, expected)
    replay.require(type(summary.get("Rows")) is list and len(summary["Rows"]) == 20,
                   "expected complete 20-row synthetic reference")
    for field in ("Collision", "NativePeriodicEffectCadence", "RigAssumptions"):
        replay.require(type(summary.get(field)) is str and 0 < len(summary[field]) <= 2048,
                       "missing model assumptions")
    cases, identities = [], set()
    for row in summary["Rows"]:
        replay.require(type(row) is dict, "invalid row")
        replay.require(row.get("Input") in ("steady-load", "sine-1hz", "sine-4hz", "sign-step", "lifecycle-gates"),
                       "unsupported synthetic input")
        replay.integer(row.get("StrengthPercent"), 0, 100)
        replay.require(row["StrengthPercent"] in (0, 25, 50, 100), "unsupported strength")
        identity = (row["Input"], row["StrengthPercent"])
        replay.require(identity not in identities, "duplicate case")
        identities.add(identity)
        replay.integer(row.get("Frames"), 300, 300)
        replay.integer(row.get("PeakPercent"), 25, 25)
        replay.number(row.get("DurationSeconds"), 6, 6)
        for field in ("Peak", "Rms"):
            replay.number(row.get(field), 0, .250001)
        replay.require(row["Rms"] <= row["Peak"] + 1e-12, "RMS exceeds peak")
        for field in ("AtOutputCapFrames", "NonzeroCommandFrames"):
            replay.integer(row.get(field), 0, 300)
        replay.number(row.get("NonzeroCommandDuty"), 0, 1)
        replay.number(row.get("AtOutputCapSeconds"), 0, 6)
        replay.require(abs(row["NonzeroCommandDuty"] - row["NonzeroCommandFrames"] / 300) < 1e-12 and
                       abs(row["AtOutputCapSeconds"] - row["AtOutputCapFrames"] * .02) < 1e-12,
                       "inconsistent sample occupancy/duty")
        replay.require(row["AtOutputCapFrames"] <= row["NonzeroCommandFrames"],
                       "cap occupancy exceeds active samples")
        replay.require((row["AtOutputCapFrames"] > 0) == (row["Peak"] >= .25 - .000001),
                       "cap occupancy contradicts declared peak/threshold")
        replay.require((row["NonzeroCommandFrames"] > 0) == (row["Peak"] > .0015),
                       "activity contradicts declared peak/threshold")
        replay.number(row.get("MaximumObservedSlew"), 0, 25.0001)
        replay.integer(row.get("ShutdownZeroCalls"), 0, 100)
        replay.require(type(row.get("ShutdownZeroBeforeClose")) is bool, "invalid shutdown order")
        replay.digest(row.get("InputSha256"))
        replay.digest(row.get("CommandsSha256"))
        cases.append({"case": row["Input"], "strengthPercent": row["StrengthPercent"],
            "producerRow": row, "metrics": {
                "peak": metric(row["Peak"], "normalized-command", "constant.signed.peak-absolute@1"),
                "rms": metric(row["Rms"], "normalized-command", "constant.uniform-sample.rms@1"),
                "capOccupancy": metric(row["AtOutputCapFrames"] / 300, "fraction",
                                       "constant.uniform-sample.abs-ge-0.25-minus-0.000001@1"),
                "duration": metric(6, "seconds", "injected-envelope.frames-times-dt@1"),
                "duty": metric(row["NonzeroCommandDuty"], "fraction",
                               "constant.uniform-sample.abs-gt-0.0015@1"),
                "slew": metric(row["MaximumObservedSlew"], "normalized-command/second",
                               "constant.uniform-0.02s.initial-zero.gates-in.shutdown-out@1"),
                "shutdownZeros": metric(row["ShutdownZeroCalls"], "calls",
                                        "fake-device.ZeroAndStop-after-envelope@1")}})
    return {"game": "Woden", "status": "supported", "adapter": "woden-model3-summary@1",
            "artifactSha256": artifact_sha256,
            "provenance": {key: value for key, value in summary.items() if key != "Rows"},
            "nativeVersion": {"status": "unsupported", "reason": "fake device; summary records no native version"},
            "sourceRevision": {"status": "unsupported", "reason": "producer summary records no source revision"},
            "hashScope": "artifact bytes verified; input/command/dependency hashes retained as producer claims",
            "cases": cases}


def unavailable(game, reason):
    replay.name(game)
    replay.require(type(reason) is str and 0 < len(reason) <= 2048, "missing unsupported reason")
    return {"game": game, "status": "unsupported", "reason": reason, "cases": []}


def compare_metrics(left, right):
    """Compare only matching explicit quantity definitions, never torque/feel."""
    result = {}
    for key in METRICS:
        a, b = left.get(key), right.get(key)
        if not a or not b or a.get("status") != "supported" or b.get("status") != "supported":
            result[key] = {"status": "unsupported", "reason": "metric missing or unsupported"}
        elif not all(type(item.get(field)) is str and IDENTIFIER.fullmatch(item[field])
                     for item in (a, b) for field in ("unit", "definition")):
            result[key] = {"status": "unsupported", "reason": "invalid unit or definition identifier"}
        elif a["unit"] != b["unit"] or a["definition"] != b["definition"]:
            result[key] = {"status": "unsupported", "reason": "units or effect/clock/threshold semantics differ"}
        else:
            for value in (a.get("value"), b.get("value")):
                replay.number(value, 0, 1e12)
            result[key] = {"status": "supported", "baseline": a["value"], "candidate": b["value"],
                           "delta": b["value"] - a["value"], "unit": a["unit"],
                           "definition": a["definition"]}
    return result


def report(woden_input, outrun_input=None):
    other = [unavailable(game, "No verified command-summary supplied to this report; no conversion inferred")
             for game in ("OutRun2006", "F-Zero")]
    if outrun_input is not None:
        other[0] = outrun_input
    comparisons = [{"baselineGame": a, "candidateGame": b,
                    "metrics": compare_metrics({}, {})}
                   for a, b in (("Woden", "OutRun2006"), ("Woden", "F-Zero"), ("OutRun2006", "F-Zero"))]
    return {"schema": "dbce.wheel.command-summary-report", "version": 1,
            "scope": "offline commanded-value diagnostics; synthetic inputs are not matched cross-game stimuli",
            "physicalAcceptance": False, "crossGameFeelEquivalence": False,
            "inputs": [woden_input] + other, "crossGameComparisons": comparisons}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("woden_summary", type=Path)
    parser.add_argument("--outrun-summary", type=Path,
                        help="exact reviewed synthetic raw-request artifact; no normalization conversion")
    args = parser.parse_args()
    try:
        summary, digest = read_summary(args.woden_summary)
        outrun_input = None
        if args.outrun_summary is not None:
            from outrun_summary import adapt
            outrun_value, outrun_digest = read_summary(args.outrun_summary)
            outrun_input = adapt(outrun_value, outrun_digest)
        sys.stdout.buffer.write(replay.encode(report(woden(summary, digest), outrun_input)))
        return 0
    except (replay.InvalidCase, OSError, KeyError, TypeError) as error:
        print("invalid summary: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
