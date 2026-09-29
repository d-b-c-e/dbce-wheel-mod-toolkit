"""Versioned playback identity and observation only. No device/game/IPC APIs."""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import sys

MAX_LINE = 16384
MAX_FILE = 256 * 1024 * 1024
MAX_RECORDS = 2_000_000
HASH = re.compile(r"[0-9a-f]{64}\Z")
NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.:@-]{0,127}\Z")


class InvalidCase(ValueError):
    pass


def require(ok, message):
    if not ok:
        raise InvalidCase(message)


def shape(value, keys):
    require(type(value) is dict and set(value) == set(keys.split()),
            "unexpected or missing object fields: " + keys)


def integer(value, low=0, high=2**63-1):
    require(type(value) is int and low <= value <= high, "integer out of range")


def number(value, low, high):
    require(type(value) in (int, float) and math.isfinite(value) and low <= value <= high,
            "nonfinite or out-of-range number")


def name(value):
    require(type(value) is str and NAME.fullmatch(value), "invalid identifier")


def digest(value):
    require(type(value) is str and HASH.fullmatch(value), "invalid SHA-256")


def clock(value):
    shape(value, "domain ticksPerSecond")
    require(value["domain"] in ("emulated", "monotonic"), "unknown clock domain")
    integer(value["ticksPerSecond"], 1, 10**12)


def _pairs(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, "duplicate JSON key: " + key)
        out[key] = value
    return out


def decode(data):
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=_pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(InvalidCase("nonfinite JSON")))
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise InvalidCase("invalid bounded UTF-8 JSON") from error


def encode(value):
    try:
        return (json.dumps(value, ensure_ascii=True, allow_nan=False,
                           separators=(",", ":")) + "\n").encode("utf-8")
    except (ValueError, TypeError, RecursionError) as error:
        raise InvalidCase("unserializable observation") from error


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def artifact(root, item):
    shape(item, "path sha256 format")
    digest(item["sha256"])
    name(item["format"])
    rel = item["path"]
    require(type(rel) is str and 0 < len(rel) <= 512 and
            not any(c in rel for c in ("\\", ":", "\x00")) and
            all(part not in ("", ".", "..") for part in rel.split("/")), "unsafe artifact path")
    target = (root / rel).resolve()
    require(target.is_relative_to(root) and target.is_file(), "artifact absent or escapes case root")
    require(sha256(target) == item["sha256"], "artifact hash mismatch: " + rel)


def validate_case(path):
    path = Path(path)
    with path.open("rb") as source:
        data = source.read(65537)
    require(len(data) <= 65536, "case manifest too large")
    case = decode(data)
    shape(case, "schema version caseId game adapter capability clock source initialState artifacts provenance")
    require(case["schema"] == "dbce.wheel.replay-case" and type(case["version"]) is int
            and case["version"] == 1, "unsupported case schema")
    for field in ("caseId", "game", "adapter"):
        name(case[field])
    require(case["capability"] in ("signal-reprocess", "game-input-replay"), "unsupported capability")
    clock(case["clock"])
    root = path.resolve().parent
    artifact(root, case["source"])
    if case["initialState"] is not None:
        artifact(root, case["initialState"])
    require(case["capability"] != "game-input-replay" or case["initialState"] is not None,
            "game replay requires initial state identity")
    require(type(case["artifacts"]) is dict and len(case["artifacts"]) <= 128, "artifact count")
    for key, value in case["artifacts"].items():
        name(key)
        artifact(root, value)
    provenance = case["provenance"]
    shape(provenance, "sourceRevision dirty executableSha256")
    name(provenance["sourceRevision"])
    require(type(provenance["dirty"]) is bool, "dirty flag must be boolean")
    digest(provenance["executableSha256"])
    return {"valid": True, "caseId": case["caseId"], "caseSha256": hashlib.sha256(data).hexdigest(),
            "capability": case["capability"], "clock": case["clock"],
            "scope": "artifact identity only; game replay and physical force not established"}


def validate_header(header):
    shape(header, "kind schema version caseId caseSha256 clock model configSha256 profileSha256 output physicalOutput")
    require(header["kind"] == "header" and header["schema"] == "dbce.wheel.force-observation"
            and type(header["version"]) is int and header["version"] == 1, "unsupported observation schema")
    for field in ("caseId", "model"):
        name(header[field])
    for field in ("caseSha256", "configSha256", "profileSha256"):
        digest(header[field])
    clock(header["clock"])
    require(header["output"] == "observe" and header["physicalOutput"] is False,
            "only device-free observation is supported")


def validate_force(row, sequence, previous_tick):
    shape(row, "kind sequence tick frame effect family operation magnitude frequencyHz durationMs")
    require(row["kind"] == "force", "unexpected record kind")
    integer(row["sequence"])
    require(row["sequence"] == sequence, "noncontiguous force sequence")
    integer(row["tick"])
    require(row["tick"] >= previous_tick, "clock reversed")
    if row["frame"] is not None:
        integer(row["frame"])
    name(row["effect"])
    require(row["family"] in ("constant", "spring", "damper", "friction", "sine", "rumble", "all"),
            "unsupported effect family")
    require(row["operation"] in ("set", "start", "stop", "stop_all"), "unknown effect operation")
    require((row["family"] == "all") == (row["operation"] == "stop_all"), "invalid stop_all family")
    number(row["magnitude"], -1, 1)
    number(row["frequencyHz"], 0, 10000)
    if row["durationMs"] is not None:
        number(row["durationMs"], 0, 60000)
        require(row["durationMs"] > 0, "duration must be positive")
    if row["operation"] in ("stop", "stop_all"):
        require(row["magnitude"] == 0 and row["frequencyHz"] == 0 and row["durationMs"] is None,
                "stop must contain a zero request")


class ObservationSink:
    """Offline file sink only; no hardware backend or native module loading."""
    def __init__(self, path, header):
        validate_header(header)
        data = encode(header)
        require(len(data) <= MAX_LINE, "header too large")
        self._file = Path(path).open("xb")
        self._count = 0
        self._tick = 0
        self._bytes = len(data)
        self._failed = False
        try:
            self._file.write(data)
        except BaseException:
            self._file.close()
            raise

    def emit(self, request):
        require(not self._file.closed and not self._failed, "sink is closed or faulted")
        try:
            data = encode(request)
            require(len(data) <= MAX_LINE and self._bytes + len(data) + 256 <= MAX_FILE
                    and self._count < MAX_RECORDS, "observation bound exceeded")
            snapshot = decode(data)
            validate_force(snapshot, self._count, self._tick)
            self._file.write(data)
            self._bytes += len(data)
            self._tick = snapshot["tick"]
            self._count += 1
        except BaseException:
            self._failed = True
            self._file.close()
            raise

    def close(self):
        if self._file.closed:
            return
        try:
            if not self._failed:
                self._file.write(encode({"kind": "footer", "complete": True, "count": self._count}))
        finally:
            self._file.close()

    def __enter__(self):
        return self

    def __exit__(self, kind, value, traceback):
        if kind is not None:
            self._failed = True
        self.close()


def read_observations(path):
    """Stream with bounded memory; fully exhaust to establish validity."""
    with Path(path).open("rb") as source:
        count = total = previous_tick = 0
        header_seen = False
        while True:
            line = source.readline(MAX_LINE + 1)
            require(line, "missing observation footer")
            total += len(line)
            require(len(line) <= MAX_LINE and total <= MAX_FILE, "observation byte bound exceeded")
            require(line.endswith(b"\n"), "truncated observation line")
            row = decode(line)
            if not header_seen:
                validate_header(row)
                header_seen = True
                yield row
            elif type(row) is dict and row.get("kind") == "footer":
                shape(row, "kind complete count")
                integer(row["count"])
                require(row["complete"] is True and row["count"] == count, "incomplete/mismatched footer")
                require(source.read(1) == b"", "data after footer")
                yield row
                return
            else:
                require(count < MAX_RECORDS, "observation record bound exceeded")
                validate_force(row, count, previous_tick)
                previous_tick = row["tick"]
                count += 1
                yield row


def compare(baseline, candidate, tolerance=0.0):
    number(tolerance, 0, 1)
    left, right = read_observations(baseline), read_observations(candidate)
    a, b = next(left), next(right)
    require(all(a[key] == b[key] for key in ("caseId", "caseSha256", "clock")),
            "different case identity or clock; comparison unavailable")
    differences = compared = 0
    squared = maximum = 0.0
    first = []
    for index, (a_row, b_row) in enumerate(itertools.zip_longest(left, right)):
        mismatch = a_row is None or b_row is None or a_row["kind"] != b_row["kind"]
        if not mismatch and a_row["kind"] == "force":
            structural = ("sequence", "tick", "frame", "effect", "family", "operation")
            mismatch = any(a_row[k] != b_row[k] for k in structural)
            delta = abs(a_row["magnitude"] - b_row["magnitude"])
            if not mismatch:
                compared += 1
                squared += delta * delta
                maximum = max(maximum, delta)
            for key in ("magnitude", "frequencyHz", "durationMs"):
                x, y = a_row[key], b_row[key]
                mismatch |= ((x != y) if x is None or y is None else abs(x-y) > tolerance)
        elif not mismatch:
            mismatch = a_row != b_row
        if mismatch:
            differences += 1
            if len(first) < 20:
                first.append(index)
    return {"equal": differences == 0, "differences": differences, "comparedRequests": compared,
            "firstDifferentRecords": first, "maxMagnitudeDelta": maximum,
            "rmsMagnitudeDelta": math.sqrt(squared/compared) if compared else 0.0,
            "baseline": a, "candidate": b, "physicalAcceptance": False,
            "scope": "ordered software force requests; not game determinism or measured torque"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("validate")
    check.add_argument("case")
    diff = commands.add_parser("compare")
    diff.add_argument("baseline")
    diff.add_argument("candidate")
    diff.add_argument("--tolerance", type=float, default=0.0)
    args = parser.parse_args(argv)
    try:
        result = validate_case(args.case) if args.command == "validate" else compare(
            args.baseline, args.candidate, args.tolerance)
        print(json.dumps(result, indent=2, allow_nan=False))
        return 1 if result.get("equal") is False else 0
    except (OSError, ValueError) as error:
        print(json.dumps({"valid": False, "error": str(error)}))
        return 2


if __name__ == "__main__":
    sys.exit(main())
