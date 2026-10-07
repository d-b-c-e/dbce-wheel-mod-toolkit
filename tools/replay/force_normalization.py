"""Duration-weighted software FFB comparisons. Reads files only; no device APIs.

Game adapters own signal selection, model evaluation, settings and validation.
This tool never rescales recorded outputs to guess a different nonlinear model.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
import sys

import replay_case

QUANTITIES = {"constant-force-request", "spring-coefficient", "damper-coefficient", "periodic-amplitude"}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def finite(value):
    require(not isinstance(value, bool), "boolean where a number is required")
    n = float(value)
    require(math.isfinite(n), "nonfinite number")
    return n


def load_json(path):
    return replay_case.decode(Path(path).read_bytes().removeprefix(b"\xef\xbb\xbf"))


def artifact(root, entry):
    path = (root / entry["path"]).resolve()
    require(path.is_file(), "missing artifact: " + str(path))
    require(replay_case.sha256(path) == entry["sha256"].lower(), "artifact hash mismatch: " + str(path))
    return path


def mapped(raw, mapping):
    value = finite(raw[mapping["value"]])
    require(abs(value) <= 1, "request outside [-1,1]")
    valid = finite(raw[mapping["valid"]]) if mapping.get("valid") else 1
    require(valid in (0, 1), "valid must be 0 or 1")
    epoch = finite(raw[mapping["epoch"]]) if mapping.get("epoch") else 0
    require(0 <= epoch <= 2**53 and epoch == int(epoch), "epoch must be a nonnegative exact integer")
    time = finite(raw[mapping["time"]])
    require(time >= 0, "negative time")
    speed = None
    if mapping.get("speed"):
        scale = finite(mapping.get("speedScale", 1))
        require(scale > 0, "invalid speed conversion")
        speed = finite(abs(finite(raw[mapping["speed"]])) * scale)
    return {"time": time, "value": value, "valid": valid == 1, "epoch": epoch, "speed": speed}


def read_rows(path, spec):
    require(path.stat().st_size <= replay_case.MAX_FILE, "input exceeds 256 MiB")
    mapping = spec["mapping"]
    rows = []
    if spec["format"] == "csv":
        with path.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            require(reader.fieldnames and len(set(reader.fieldnames)) == len(reader.fieldnames), "duplicate/absent CSV headers")
            for raw in reader:
                require(None not in raw, "extra CSV fields")
                rows.append(mapped(raw, mapping))
                require(len(rows) <= replay_case.MAX_RECORDS, "too many rows")
    elif spec["format"] == "session-jsonl":
        footer = None
        metadata = False
        with path.open("rb") as stream:
            for line in stream:
                require(footer is None, "data after session footer")
                record = replay_case.decode(line)
                if "metadata" in record:
                    require(not metadata and not rows, "duplicate/late metadata")
                    metadata = True
                elif "sample" in record:
                    require(metadata, "sample before metadata")
                    sample = record["sample"]
                    require(sample["sequence"] == len(rows), "noncontiguous sample sequence")
                    raw = dict(sample["channels"])
                    require("$elapsed" not in raw, "reserved channel")
                    raw["$elapsed"] = sample["elapsedSeconds"]
                    rows.append(mapped(raw, mapping))
                    require(len(rows) <= replay_case.MAX_RECORDS, "too many rows")
                elif "footer" in record:
                    footer = record["footer"]
                else:
                    require("marker" in record, "unknown session record")
        require(metadata and footer and footer["completed"] is True, "incomplete session")
        counts = footer["counts"]
        require(counts["writtenSamples"] == len(rows), "session count mismatch")
        for key in ("droppedSamples", "droppedMarkers", "invalidCount", "errorCount", "queueFullCount", "limitCount", "contentionCount"):
            require(counts[key] == 0, "session lost data: " + key)
    else:
        raise ValueError("unsupported input format")
    require(len(rows) >= 2, "at least two samples required")
    return rows


def metrics(intervals, cap):
    if not intervals:
        return {"seconds": 0, "rms": None, "absP50": None, "absP95": None, "absP99": None,
                "peak": None, "atCapFraction": None, "atUnitFraction": None}
    duration = sum(dt for value, dt in intervals)
    absolute = sorted((abs(value), dt) for value, dt in intervals)
    def percentile(q):
        elapsed = 0
        for value, dt in absolute:
            elapsed += dt
            if elapsed >= duration * q:
                return value
        return absolute[-1][0]
    return {"seconds": duration, "rms": math.sqrt(sum(v*v*dt for v, dt in intervals)/duration),
            "absP50": percentile(.5), "absP95": percentile(.95), "absP99": percentile(.99),
            "peak": absolute[-1][0],
            "atCapFraction": sum(dt for v, dt in intervals if abs(v) >= cap - .0001)/duration,
            "atUnitFraction": sum(dt for v, dt in intervals if abs(v) >= .9999)/duration}


def temporal_metrics(intervals):
    """Ordered holds; no continuity across gaps, epochs, duplicates or bands.

    Variation uses changes between consecutive interval starts, not a guessed
    ramp between samples. The last sample has no known hold duration. These
    metrics describe software requests, never torque or identified corners.
    """
    duration = sum(end - start for start, end, _, _ in intervals)
    thresholds = {str(x): {"fraction": None, "longestSameSignSeconds": None, "runs": 0}
                  for x in (.25, .5, .75)}
    if not intervals:
        return {"seconds": 0., "meanSigned": None, "meanAbsolute": None,
                "variationPerSecond": None, "signReversals": 0, "segments": 0,
                "thresholds": thresholds}
    sums = {x: 0. for x in thresholds}
    runs = {x: [0, 0.] for x in thresholds}
    for item in thresholds.values():
        item["longestSameSignSeconds"] = 0.
    previous = None
    direction = reversals = segments = 0
    variation = signed = absolute = 0.
    for start, end, value, segment in intervals:
        continuous = previous is not None and start == previous[1] and segment == previous[3]
        if not continuous:
            segments += 1
            direction = 0
            runs = {x: [0, 0.] for x in thresholds}
        else:
            variation += abs(value - previous[2])
        sign = 1 if value > .02 else -1 if value < -.02 else 0
        if sign:
            if direction and sign != direction:
                reversals += 1
            direction = sign
        dt = end - start
        signed += value * dt
        absolute += abs(value) * dt
        for key, item in thresholds.items():
            hold_sign = (1 if value > 0 else -1) if abs(value) >= float(key) else 0
            if not hold_sign:
                runs[key] = [0, 0.]
                continue
            sums[key] += dt
            if runs[key][0] != hold_sign:
                item["runs"] += 1
                runs[key] = [hold_sign, 0.]
            runs[key][1] += dt
            item["longestSameSignSeconds"] = max(item["longestSameSignSeconds"], runs[key][1])
        previous = (start, end, value, segment)
    for key, item in thresholds.items():
        item["fraction"] = sums[key] / duration
    return {"seconds": duration, "meanSigned": signed / duration,
            "meanAbsolute": absolute / duration, "variationPerSecond": variation / duration,
            "signReversals": reversals, "segments": segments, "thresholds": thresholds}


def summarize(rows, cap=1, max_gap=.1):
    require(0 < finite(cap) <= 1 and 0 < finite(max_gap) <= 1, "invalid cap/gap")
    bands = {name: [] for name in ("all", "0-5 km/h", "5-20 km/h", "20-60 km/h", "60+ km/h", "speed unavailable")}
    ordered = {name: [] for name in bands}
    excluded = {"invalid": 0., "epoch": 0., "gap": 0.}
    duplicates = 0
    segment = 0
    for left, right in zip(rows, rows[1:]):
        dt = right["time"] - left["time"]
        require(dt >= 0, "time reversed; adapters must provide a monotonic timeline")
        if dt == 0:
            duplicates += 1
            segment += 1  # An instantaneous reset/change cannot establish a continuous hold.
            continue
        reason = "epoch" if left["epoch"] != right["epoch"] else "invalid" if not left["valid"] else "gap" if dt > max_gap + 1e-9 else None
        if reason:
            excluded[reason] += dt
            segment += 1
            continue
        pair = (left["value"], dt)
        speed = left["speed"]
        band = "speed unavailable" if speed is None else "0-5 km/h" if speed < 5 else "5-20 km/h" if speed < 20 else "20-60 km/h" if speed < 60 else "60+ km/h"
        bands["all"].append(pair)
        bands[band].append(pair)
        interval = (left["time"], right["time"], left["value"], segment)
        ordered["all"].append(interval)
        ordered[band].append(interval)
    require(bands["all"], "no valid intervals")
    return {"samples": len(rows), "spanSeconds": rows[-1]["time"] - rows[0]["time"],
            "duplicateTimes": duplicates, "excludedSeconds": excluded,
            "validSamplePeak": max(abs(r["value"]) for r in rows if r["valid"]),
            "bands": {name: metrics(data, cap) for name, data in bands.items()},
            "temporal": {"version": 1, "hold": "left sample until next timestamp; final sample has no duration",
                         "neutralZone": .02, "thresholdBasis": "fraction of software unit range, not configured cap or torque",
                         "variationBasis": "sum of changes between adjacent eligible interval-start requests / eligible seconds",
                         "bands": {name: temporal_metrics(data) for name, data in ordered.items()}}}


def analyze(manifest_path):
    manifest_path = Path(manifest_path).resolve()
    manifest = load_json(manifest_path)
    require(manifest["schema"] == "dbce.ffb-normalization" and manifest["version"] == 1, "unknown manifest")
    reference = None
    if "reference" in manifest:
        reference_path = artifact(manifest_path.parent, manifest["reference"])
        profile = load_json(reference_path)
        require(profile["schema"] == "dbce.ffb-normalization-reference" and profile["version"] == 1,
                "unknown reference profile")
        for key in ("id", "game", "modelVersion", "sourceCommit"):
            require(isinstance(profile.get(key), str) and bool(profile[key]), "missing reference identity: " + key)
        require(0 < finite(profile["strengthPercent"]) <= 100, "invalid reference strength")
        reference = {"sha256": manifest["reference"]["sha256"], "profile": profile}
        artifact(manifest_path.parent, manifest["reference"])
    results = []
    seen = set()
    for spec in manifest["series"]:
        require(spec["id"] not in seen, "duplicate series id")
        seen.add(spec["id"])
        require(spec["quantity"] in QUANTITIES, "unknown effect quantity")
        require(spec["evidence"] in ("recorded-model-output", "counterfactual", "synthetic"), "unknown evidence level")
        for key in ("game", "model", "settings", "limitations", "validation"):
            require(bool(spec.get(key)), "missing provenance: " + key)
        # Identity checks do not replace the adapter's game/model validation.
        require(spec.get("artifacts"), "validation/model artifacts must be pinned")
        for entry in spec["artifacts"]:
            artifact(manifest_path.parent, entry)
        path = artifact(manifest_path.parent, spec["source"])
        stats = summarize(read_rows(path, spec), spec["cap"], spec["maxGapSeconds"])
        artifact(manifest_path.parent, spec["source"])  # Refuse a concurrently changed source.
        results.append({"id": spec["id"], "game": spec["game"], "quantity": spec["quantity"],
                        "evidence": spec["evidence"], "model": spec["model"], "settings": spec["settings"],
                        "sourceSha256": spec["source"]["sha256"], "validation": spec["validation"],
                        "limitations": spec["limitations"], "cap": spec["cap"], **stats})
    require(results, "empty comparison")
    if reference is not None:
        artifact(manifest_path.parent, manifest["reference"])
    return {"schema": "dbce.ffb-normalization-report", "version": 1,
            "manifestSha256": replay_case.sha256(manifest_path), "physicalAcceptance": False,
            "scope": "Duration-weighted software requests; different drives are not matched physical workloads. No automatic gain recommendation.",
            "reference": reference, "series": results}


def markdown(report):
    def percent(v):
        return "—" if v is None else f"{v*100:.2f}%"
    lines = ["# FFB software comparison", "", report["scope"], "",
             "Values are fractions of the software request range, not measured wheel torque. Spring/damper coefficients and periodic amplitudes must be compared separately from constant force.", ""]
    if report.get("reference"):
        ref = report["reference"]
        profile = ref["profile"]
        lines += [f"Reference: **{profile['game']} at Strength {profile['strengthPercent']}**, model `{profile['modelVersion']}`, profile `{profile['id']}`.",
                  f"Pinned source `{profile['sourceCommit']}`; reference SHA-256 `{ref['sha256']}`.",
                  "Per-effect settings remain part of the reference. Trial settings below may differ; this report does not certify a match or rescale any series.", ""]
    lines += [
             "| Series / speed | Seconds | RMS | Abs P95 | Peak | At configured cap |",
             "|---|---:|---:|---:|---:|---:|"]
    for s in report["series"]:
        for band, m in s["bands"].items():
            if m["seconds"]:
                lines.append(f"| {s['id']} / {band} | {m['seconds']:.2f} | {percent(m['rms'])} | {percent(m['absP95'])} | {percent(m['peak'])} | {percent(m['atCapFraction'])} |")
    for s in report["series"]:
        lines += ["", f"## Request timing: {s['id']}", "",
                  "Ordered software requests; sign reversals cross a +/-2% neutral zone. Holds break at exclusions, epochs, duplicate timestamps and speed-band boundaries. These are not detected corner phases or physical response times.", "",
                  "| Speed | Mean absolute | Mean signed | Time at >=50% | Longest same-sign >=50% hold (s) | Sign reversals | Variation / eligible s |",
                  "|---|---:|---:|---:|---:|---:|---:|"]
        for band, m in s['temporal']['bands'].items():
            if m['seconds']:
                half = m['thresholds']['0.5']
                lines.append(f"| {band} | {percent(m['meanAbsolute'])} | {percent(m['meanSigned'])} | {percent(half['fraction'])} | {half['longestSameSignSeconds']:.3f} | {m['signReversals']} | {m['variationPerSecond']:.3f} |")
    for s in report["series"]:
        lines += ["", f"## {s['id']}", "", f"{s['quantity']}; {s['evidence']}. Model: `{s['model']}`.", "",
                  "Settings: `" + json.dumps(s["settings"], sort_keys=True) + "`.", "",
                  s["limitations"], "", f"Validation: {s['validation']}", "",
                  f"Excluded seconds: `{json.dumps(s['excludedSeconds'])}`. Duplicate times: {s['duplicateTimes']}. Source SHA-256: `{s['sourceSha256']}`."]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path, help="New directory; existing results are never overwritten")
    args = parser.parse_args(argv)
    try:
        report = analyze(args.manifest)
        args.output.mkdir(parents=True, exist_ok=False)
        (args.output / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        (args.output / "report.md").write_text(markdown(report), encoding="utf-8")
        print(f"Compared {len(report['series'])} series: {args.output / 'report.md'}")
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("COMPARISON REFUSED: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
