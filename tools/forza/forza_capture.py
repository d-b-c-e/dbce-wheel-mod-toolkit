"""Bounded, receive-only Forza packet capture for any toolkit consumer.

python forza_capture.py --port 8001 --seconds 120 --output curb-run.jsonl
Use a diagnostic/mirror port when SimHub already owns the normal port.
Every accepted datagram retains its original bytes and all decoded fields.
No game injection, replay transmission, or device access. Python stdlib only.
"""
import argparse
import base64
import json
import math
from pathlib import Path
import socket
import struct
import time

CORNERS = ("FrontLeft", "FrontRight", "RearLeft", "RearRight")


def decode(data):
    """Strict length/finite-value decode; 232 sled, 311 dash, 324 Horizon."""
    if len(data) not in (232, 311, 324):
        raise ValueError(f"Unsupported Forza datagram length: {len(data)}")
    fields = {}

    def read(name, offset, kind="f"):
        value = struct.unpack_from("<" + kind, data, offset)[0]
        if isinstance(value, float) and not math.isfinite(value):
            raise ValueError(f"Non-finite {name} at byte {offset}")
        fields[name] = value

    read("IsRaceOn", 0, "i")
    if fields["IsRaceOn"] not in (0, 1):
        raise ValueError("IsRaceOn must be 0 or 1")
    read("TimestampMs", 4, "I")
    for offset, name in enumerate(("EngineMaxRpm", "EngineIdleRpm", "CurrentEngineRpm")):
        read(name, 8 + 4 * offset)
    for offset, name in ((20, "Acceleration"), (32, "Velocity"), (44, "AngularVelocity")):
        for i, axis in enumerate("XYZ"):
            read(name + axis, offset + 4 * i)
    for i, name in enumerate(("Yaw", "Pitch", "Roll")):
        read(name, 56 + 4 * i)
    arrays = ("NormalizedSuspensionTravel", "TireSlipRatio", "WheelRotationSpeed", "WheelOnRumbleStrip",
              "WheelInPuddleDepth", "SurfaceRumble", "TireSlipAngle", "TireCombinedSlip", "SuspensionTravelMeters")
    for a, name in enumerate(arrays):
        for i, corner in enumerate(CORNERS):
            read(name + "." + corner, 68 + 16 * a + 4 * i, "i" if name == "WheelOnRumbleStrip" else "f")
    for i, name in enumerate(("CarOrdinal", "CarClass", "CarPerformanceIndex", "DrivetrainType", "NumCylinders")):
        read(name, 212 + 4 * i, "i")
    if len(data) > 232:
        dash = 244 if len(data) == 324 else 232
        names = ("PositionX", "PositionY", "PositionZ", "Speed", "Power", "Torque",
                 *("TireTemp." + c for c in CORNERS), "Boost", "Fuel", "DistanceTraveled", "BestLap",
                 "LastLap", "CurrentLap", "CurrentRaceTime")
        for i, name in enumerate(names):
            read(name, dash + 4 * i)
        read("LapNumber", dash + 68, "H")
        for i, name in enumerate(("RacePosition", "Accel", "Brake", "Clutch", "HandBrake", "Gear")):
            read(name, dash + 70 + i, "B")
        for i, name in enumerate(("Steer", "NormalizedDrivingLine", "NormalizedAIBrakeDifference")):
            read(name, dash + 76 + i, "b")
    return fields


def packet_record(data, elapsed):
    result = {"type": "packet", "seconds": elapsed, "length": len(data),
              "rawBase64": base64.b64encode(data).decode("ascii")}
    try:
        result["fields"] = decode(data)
    except ValueError as error:
        result["error"] = str(error)
    if len(data) == 324:
        result["source"] = {0x52: "mod", 0x4B: "keeper", 0x53: "synthetic"}.get(data[323], "unknown")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8001)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--seconds", type=float, default=120)
    parser.add_argument("--max-mb", type=int, default=256)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not 0 < args.seconds <= 3600 or not 1 <= args.max_mb <= 1024:
        parser.error("Port 1..65535, seconds >0..3600, max-mb 1..1024 required")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    summary = {"type": "footer", "packets": 0, "invalid": 0, "driving": 0, "curb": 0, "rumble": 0,
               "ranges": {}, "stopReason": "duration"}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        # Exclusive ownership: do not silently compete with SimHub or another test.
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        sock.bind((args.bind, args.port))
        sock.settimeout(.25)
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            output.write(json.dumps({"type": "header", "schema": "dbce/forza-wire-v1", "bind": args.bind,
                                     "port": args.port, "durationLimit": args.seconds, "byteLimit": args.max_mb * 1024**2}) + "\n")
            output.flush()  # lets unattended runners observe that the receiver is ready
            start = time.monotonic()
            try:
                while time.monotonic() - start < args.seconds:
                    try:
                        data, _ = sock.recvfrom(65535)
                    except socket.timeout:
                        continue
                    row = packet_record(data, time.monotonic() - start)
                    text = json.dumps(row, allow_nan=False, separators=(",", ":")) + "\n"
                    if output.tell() + len(text) > args.max_mb * 1024**2 - 16384:
                        summary["stopReason"] = "size-limit"
                        break
                    output.write(text)
                    summary["packets"] += 1
                    if "error" in row:
                        summary["invalid"] += 1
                        continue
                    fields = row["fields"]
                    if fields["IsRaceOn"] == 1:
                        summary["driving"] += 1
                        summary["curb"] += any(fields["WheelOnRumbleStrip." + c] > 0 for c in CORNERS)
                        summary["rumble"] += any(fields["SurfaceRumble." + c] > 0 for c in CORNERS)
                        for key, value in fields.items():
                            limits = summary["ranges"].setdefault(key, [value, value])
                            limits[0] = min(limits[0], value)
                            limits[1] = max(limits[1], value)
            except KeyboardInterrupt:
                summary["stopReason"] = "user"
            summary["elapsedSeconds"] = time.monotonic() - start
            output.write(json.dumps(summary, allow_nan=False) + "\n")
    print(json.dumps({k: v for k, v in summary.items() if k != "ranges"}))
    return 0 if summary["packets"] > 0 and summary["invalid"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
