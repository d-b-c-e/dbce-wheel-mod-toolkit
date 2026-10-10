import numpy as np, struct, sys
t0 = 1000.0; T = []; P = []
def pkt(race, steer, thr, brk, gear):
    b = bytearray(324); struct.pack_into('<i', b, 0, race); struct.pack_into('<b', b, 320, steer); b[315] = thr; b[316] = brk; b[319] = gear; b[323] = 82; return bytes(b)
ev = [(1003, 'inject steer 0.6'), (1006, 'inject steer -0.6'), (1009, 'inject throttle 1'), (1013, 'inject brake 1'), (1016, 'inject clutch 1'), (1018, 'inject shiftUp 1'), (1020, 'inject start 1'), (1024, 'inject camera 1'), (1026, 'end')]
for k in range(0, 1600):
    t = t0 + k * 0.016; race, steer, thr, brk, gear = 1, 0, 0, 0, 2
    if 1003 <= t < 1005: steer = 60
    if 1006 <= t < 1008: steer = -58
    if 1009 <= t < 1013: thr = 255
    if 1013 <= t < 1016: brk = 250
    if t >= 1018.3: gear = 3
    if t >= 1020.4: race = 0
    T.append(t); P.append(pkt(race, steer, thr, brk, gear))
np.savez_compressed(sys.argv[1], t=np.array(T), p=np.frombuffer(b''.join(P), dtype=np.uint8).reshape(-1, 324))
open(sys.argv[2], 'w').write(''.join(f'{a:.3f} {b}\n' for a, b in ev))
