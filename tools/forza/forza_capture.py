"""Bounded, receive-only Forza packet capture for any toolkit consumer.

python forza_capture.py --port 8001 --seconds 120 --output curb-run.jsonl
Use a diagnostic/mirror port when SimHub already owns the normal port.
Every accepted datagram retains its original bytes and all decoded fields.
No game injection, replay transmission, or device access. Python stdlib only.
Use --port 0 for an exclusively bound OS-selected port (reported in the flushed
header). Only the owning supervisor creates the fresh --stop-file, after the
sender has closed. Stop drains queued datagrams until the socket is empty, with
a one-second / 4096-datagram bound; hitting either bound is an incomplete stop.
This proves an empty receive queue at stop, not that UDP never lost a packet.
The duration and size limits still apply independently.
"""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import socket
import struct
import time

CORNERS = ("FrontLeft", "FrontRight", "RearLeft", "RearRight")
PROFILES = ("toolkit", "fh4", "fh5", "fh6")


def decode(data, profile="toolkit"):
    """Strict length/finite-value decode; 232 sled, 311 dash, 324 Horizon."""
    if profile not in PROFILES:
        raise ValueError(f"Unknown producer profile: {profile}")
    if profile != "toolkit" and len(data) != 324:
        raise ValueError(f"{profile} requires the 324-byte Horizon datagram, got {len(data)}")
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
        # FH6's published contract calls these S32 presence flags. The toolkit's
        # older float depth fields retain their exact names and interpretation.
        if profile == "fh6" and name == "WheelInPuddleDepth":
            name = "WheelInPuddle"
        for i, corner in enumerate(CORNERS):
            read(name + "." + corner, 68 + 16 * a + 4 * i,
                 "i" if name in ("WheelOnRumbleStrip", "WheelInPuddle") else "f")
            if profile == "fh6" and name == "WheelInPuddle" and fields[name + "." + corner] not in (0, 1):
                raise ValueError("FH6 published WheelInPuddle flag must be 0 or 1; retain raw bytes for contract review")
    for i, name in enumerate(("CarOrdinal", "CarClass", "CarPerformanceIndex", "DrivetrainType", "NumCylinders")):
        read(name, 212 + 4 * i, "i")
    if profile == "fh6":
        read("CarGroup", 232, "I")
        read("SmashableVelDiff", 236)
        read("SmashableMass", 240)
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


def packet_record(data, elapsed, profile="toolkit"):
    result = {"type": "packet", "seconds": elapsed, "length": len(data),
              "rawBase64": base64.b64encode(data).decode("ascii")}
    try:
        result["fields"] = decode(data, profile)
    except ValueError as error:
        result["error"] = str(error)
    if profile != "toolkit":
        # Caller-supplied provenance, not authentication inferred from packet bytes.
        result["source"] = "native-declared"
        result["profile"] = profile
        if len(data) == 324:
            result["horizonExtraBase64"] = base64.b64encode(data[232:244]).decode("ascii")
            result["trailingByte"] = data[323]
    elif len(data) == 324:
        result["source"] = {0x52: "mod", 0x4B: "keeper", 0x53: "synthetic"}.get(data[323], "unknown")
    return result


def drain_pending(sock, record, deadline, max_packets=4096, clock=time.monotonic):
    """Retain queued datagrams with the normal writer; never wait for a sender."""
    sock.setblocking(False)
    drained = 0
    while drained < max_packets and clock() < deadline:
        try:
            data, peer = sock.recvfrom(65535)
        except BlockingIOError:
            return "supervisor-stop", drained
        if not record(data, peer):
            return "size-limit", drained
        drained += 1
    return "supervisor-stop-incomplete", drained


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8001)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--seconds", type=float, default=120)
    parser.add_argument("--max-mb", type=int, default=256)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", choices=PROFILES, default="toolkit",
                        help="Explicit producer contract; native names do not authenticate the UDP sender")
    parser.add_argument("--source-exe", type=Path, help="Read-only executable hash for capture provenance")
    parser.add_argument("--source-build", help="Declared game build ID (not inferred from telemetry)")
    parser.add_argument("--stop-file", type=Path,
                        help="Fresh supervisor-owned path; its appearance ends capture with a footer")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535 or not 0 < args.seconds <= 3600 or not 1 <= args.max_mb <= 1024:
        parser.error("Port 0..65535 (0 chooses an exclusive free port), seconds >0..3600, max-mb 1..1024 required")
    if args.stop_file and (args.stop_file.exists() or args.stop_file.resolve() == args.output.resolve()):
        parser.error("The stop file must be a fresh path distinct from the capture output")
    if args.profile == "fh6" and 5200 <= args.port <= 5300:
        parser.error("FH6 reserves ports 5200..5300 for its own outgoing socket; use another receiver port")
    identity = {"profile": args.profile, "declaredBuild": args.source_build}
    if args.source_exe:
        with args.source_exe.open("rb") as executable:
            identity["executableSha256"] = hashlib.file_digest(executable, "sha256").hexdigest().upper()
        identity["executableName"] = args.source_exe.name
    args.output.parent.mkdir(parents=True, exist_ok=True)
    summary = {"type": "footer", "packets": 0, "invalid": 0, "driving": 0, "curb": 0, "rumble": 0,
               "ranges": {}, "stopReason": "duration", "drainedAfterStop": 0}
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        # Exclusive ownership: do not silently compete with SimHub or another test.
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        sock.bind((args.bind, args.port))
        bound_port = sock.getsockname()[1]
        if args.profile == "fh6" and 5200 <= bound_port <= 5300:
            parser.error("The OS-selected port is reserved by FH6; choose another receiver port")
        sock.settimeout(.25)
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            output.write(json.dumps({"type": "header", "schema": "dbce/forza-wire-v1", "bind": args.bind,
                                     "port": bound_port, "requestedPort": args.port,
                                     "durationLimit": args.seconds, "byteLimit": args.max_mb * 1024**2,
                                     "producer": identity}) + "\n")
            output.flush()  # lets unattended runners observe that the receiver is ready
            start = time.monotonic()
            def record_datagram(data, peer):
                row = packet_record(data, time.monotonic() - start, args.profile)
                row["sender"] = {"address": peer[0], "port": peer[1]}
                text = json.dumps(row, allow_nan=False, separators=(",", ":")) + "\n"
                if output.tell() + len(text) > args.max_mb * 1024**2 - 16384:
                    return False
                output.write(text)
                summary["packets"] += 1
                if "error" in row:
                    summary["invalid"] += 1
                    return True
                fields = row["fields"]
                if fields["IsRaceOn"] == 1:
                    summary["driving"] += 1
                    summary["curb"] += any(fields["WheelOnRumbleStrip." + c] > 0 for c in CORNERS)
                    summary["rumble"] += any(fields["SurfaceRumble." + c] > 0 for c in CORNERS)
                    for key, value in fields.items():
                        limits = summary["ranges"].setdefault(key, [value, value])
                        limits[0] = min(limits[0], value)
                        limits[1] = max(limits[1], value)
                return True

            try:
                while time.monotonic() - start < args.seconds:
                    if args.stop_file and args.stop_file.exists():
                        summary["stopReason"], summary["drainedAfterStop"] = drain_pending(
                            sock, record_datagram, min(start + args.seconds, time.monotonic() + 1))
                        break
                    try:
                        data, peer = sock.recvfrom(65535)
                    except socket.timeout:
                        continue
                    if not record_datagram(data, peer):
                        summary["stopReason"] = "size-limit"
                        break
            except KeyboardInterrupt:
                summary["stopReason"] = "user"
            summary["elapsedSeconds"] = time.monotonic() - start
            output.write(json.dumps(summary, allow_nan=False) + "\n")
    print(json.dumps({k: v for k, v in summary.items() if k != "ranges"}))
    return 0 if (summary["packets"] > 0 and summary["invalid"] == 0
                 and summary["stopReason"] != "supervisor-stop-incomplete") else 1


if __name__ == "__main__":
    raise SystemExit(main())
