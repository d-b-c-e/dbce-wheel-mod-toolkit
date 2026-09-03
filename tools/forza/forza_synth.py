"""Synthetic Forza telemetry: proves the READ side (SimHub) with a known-good
pattern, independent of any game.

Emits a scripted drive at 60 Hz: speed triangles 0 -> 200 mph -> 0 every ~24 s,
RPM sweeps idle -> redline within each of 4 gears, gear steps with the speed.
If the dash mirrors this pattern, the packet layout and the SimHub profile are
right; anything left is the values the game feeds.

    python tools/forza/forza_synth.py                 # Horizon 324 B to 127.0.0.1:8000
    python tools/forza/forza_synth.py 5300            # other port
    python tools/forza/forza_synth.py 8000 fm7        # 311 B FM7 dash layout
    python tools/forza/forza_synth.py 8000 sled       # 232 B sled only

Lineage: cruisn-collection harness/forza_synth.py and art-of-sim-rally
tools/ArtOfSimRally.Synth, unified.
"""
import socket
import struct
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
LAYOUT = (sys.argv[2] if len(sys.argv) > 2 else "horizon").lower()
SIZE, DASH = {"sled": (232, None), "fm7": (311, 232), "horizon": (324, 244)}[LAYOUT]
ADDR = ("127.0.0.1", PORT)

MAXRPM, IDLERPM = 7500.0, 700.0
GEAR_TOP_MPH = [50.0, 95.0, 145.0, 200.0]


def build(ms, mph, rpm, gear):
    pkt = bytearray(SIZE)
    struct.pack_into("<i", pkt, 0, 1)            # IsRaceOn
    struct.pack_into("<I", pkt, 4, ms)           # TimestampMS
    struct.pack_into("<f", pkt, 8, MAXRPM)
    struct.pack_into("<f", pkt, 12, IDLERPM)
    struct.pack_into("<f", pkt, 16, rpm)
    spd = mph * 0.44704
    struct.pack_into("<f", pkt, 40, spd)         # VelocityZ
    if DASH is not None:
        struct.pack_into("<f", pkt, DASH + 12, spd)   # Speed (m/s)
        pkt[DASH + 71] = int(255 * min(1.0, max(0.0, rpm / MAXRPM)))   # Accel follows rpm
        pkt[DASH + 75] = gear
    if SIZE == 324:
        pkt[323] = 0x53                          # 'S' - synthetic
    return bytes(pkt)


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"emitting synthetic {LAYOUT} ({SIZE} B) packets to udp://{ADDR[0]}:{PORT} at 60 Hz")
    print("pattern: speed 0->200->0 mph triangle (~24 s), 4 gears, RPM sawtooth. Ctrl+C to stop\n")
    t0 = time.time()
    ms = 0
    try:
        while True:
            t = time.time() - t0
            ph = (t % 24.0) / 12.0
            tri = ph if ph <= 1.0 else 2.0 - ph
            mph = 200.0 * tri
            gear, lo, hi = 1, 0.0, GEAR_TOP_MPH[-1]
            for i, top in enumerate(GEAR_TOP_MPH):
                if mph <= top or i == len(GEAR_TOP_MPH) - 1:
                    gear, hi = i + 1, top
                    break
                lo = top
            frac = 0.0 if hi <= lo else max(0.0, min(1.0, (mph - lo) / (hi - lo)))
            rpm = IDLERPM + (MAXRPM - 500.0 - IDLERPM) * frac
            ms = (ms + 17) & 0xFFFFFFFF
            sock.sendto(build(ms, mph, rpm, gear), ADDR)
            sys.stdout.write(f"\r  sending: {mph:6.1f} mph  rpm {rpm:5.0f}  gear {gear}   ")
            sys.stdout.flush()
            time.sleep(1.0 / 60.0)
    except KeyboardInterrupt:
        print("\nstopped")


if __name__ == "__main__":
    main()
