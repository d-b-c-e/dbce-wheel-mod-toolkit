"""Forza "Data Out" probe: proves the EMIT side independently of SimHub.

Listens on a UDP port and live-prints what a mod is actually sending, so the
emitter and the consumer can be tested separately:

  1. CLOSE SimHub (it holds the port), or point the mod at a mirror port.
  2. python tools/forza/forza_probe.py [port]        (default 8000)
  3. Drive. Speed should rise AND fall; "drops" counts every decrease, so a
     non-zero count proves the emitter is not stuck ramping.
  4. Ctrl+C for a summary.

The layout is detected from the packet length: 232 (sled), 311 (FM7 dash),
324 (Horizon). Anything else is counted, not parsed - a wrong length is the
most common reason a dash stays dead, and the receiver rejects it silently.

Byte 323 of a Horizon packet is unused by every real Forza title. Mods built
on dbce-wheel-mod-toolkit stamp 'R' (0x52) there; the idle keeper stamps 'K'
(0x4B). That is how the "src" column tells packets apart.

Lineage: art-of-sim-rally harness/forza_probe.py and cruisn-collection
harness/forza_probe.py, both validated live against SimHub's Forza profiles.
"""
import os
import socket
import struct
import sys
import time

# Dash offsets differ only by the 12-byte insert Horizon adds at 232.
LAYOUTS = {
    232: ("sled", None),
    311: ("fm7", 232),
    324: ("horizon", 244),
}
OFF_IS_RACE_ON = 0
OFF_ENGINE = 8          # maxRpm, idleRpm, currentRpm
SENTINELS = {0x52: "mod", 0x4B: "keeper"}

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
MPS_TO_MPH = 1.0 / 0.44704


def decode(data):
    name, dash = LAYOUTS[len(data)]
    race_on, = struct.unpack_from("<i", data, OFF_IS_RACE_ON)
    maxr, idler, cur = struct.unpack_from("<fff", data, OFF_ENGINE)
    speed = gear = steer = accel = brake = None
    if dash is not None:
        speed, = struct.unpack_from("<f", data, dash + 12)
        accel = data[dash + 71]
        brake = data[dash + 72]
        gear = data[dash + 75]
        steer = struct.unpack_from("<b", data, dash + 76)[0]
    src = "other"
    if len(data) == 324:
        src = SENTINELS.get(data[323], "other")
    return name, src, race_on, maxr, cur, speed, gear, steer, accel, brake


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))
    sock.settimeout(0.5)

    out_dir = os.path.join(os.getcwd(), "results")
    os.makedirs(out_dir, exist_ok=True)
    log_path = os.path.join(out_dir, "forza_probe_log.csv")
    logf = open(log_path, "w")
    logf.write("t,layout,src,race_on,mph,rpm,maxrpm,gear,steer,accel,brake\n")

    print(f"listening on UDP {PORT} (Forza Data Out, any layout)... Ctrl+C to stop")
    print(f"logging every packet to {log_path}")

    n = 0
    last_mph = 0.0
    max_mph = 0.0
    max_rpm = 0.0
    drops = 0
    sizes = {}
    t0 = time.time()
    try:
        while True:
            try:
                data, _ = sock.recvfrom(2048)
            except socket.timeout:
                continue
            n += 1
            sizes[len(data)] = sizes.get(len(data), 0) + 1
            if len(data) not in LAYOUTS:
                continue
            name, src, race_on, maxr, cur, speed, gear, steer, accel, brake = decode(data)
            mph = (speed or 0.0) * MPS_TO_MPH
            logf.write(f"{time.time() - t0:.2f},{name},{src},{race_on},{mph:.1f},{cur:.0f},"
                       f"{maxr:.0f},{gear},{steer},{accel},{brake}\n")
            if mph < last_mph - 0.5:
                drops += 1
            last_mph = mph
            max_mph = max(max_mph, mph)
            max_rpm = max(max_rpm, cur)
            sys.stdout.write(
                f"\r  [{name}/{src}] race={race_on} {mph:6.1f} mph  rpm {cur:5.0f}/{maxr:5.0f}"
                f"  gear {gear}  steer {steer}  thr {accel}  brk {brake}   ")
            sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        logf.close()
    dt = max(1e-6, time.time() - t0)
    print(f"\n\n{n} packets in {dt:.0f}s ({n / dt:.1f}/s); sizes seen: {sizes}")
    print(f"max {max_mph:.1f} mph, max rpm {max_rpm:.0f}, speed drops seen: {drops}")
    bad = {k: v for k, v in sizes.items() if k not in LAYOUTS}
    if bad:
        print(f"WARNING: {sum(bad.values())} packet(s) with a non-Forza length {sorted(bad)} - "
              f"a receiver would reject these silently.")


if __name__ == "__main__":
    main()
