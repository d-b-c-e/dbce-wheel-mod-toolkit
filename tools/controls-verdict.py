# STD-033 "input observed": did the game's own car input answer each injected control? (toolkit tools/; the
# synthetic check is controls-verdict-fixture.py: py -3 controls-verdict-fixture.py f.npz f.txt, then this on them) Reads a Forza Horizon capture
# (udp-capture.py .npz: t, p) and the event marks ("<unix-seconds> inject <action> <n>" lines), and judges each
# injection window against the packet fields the game's telemetry provides. Fields a game does not send are reported as
# not observable, never as a pass.
#   py -3 controls-verdict.py CAPTURE.npz EVENTS.txt [--unobservable clutch,handbrake] [--json out.json]
import argparse, json, struct, numpy as np

ap = argparse.ArgumentParser()
ap.add_argument('capture'); ap.add_argument('events')
ap.add_argument('--unobservable', default='', help='actions this game\'s telemetry cannot show (comma list)')
ap.add_argument('--json')
a = ap.parse_args()
z = np.load(a.capture); t = z['t']; P = z['p']
ev = []
for line in open(a.events, encoding='utf-8-sig'):
    parts = line.strip().split(' ', 1)
    if len(parts) == 2: ev.append((float(parts[0]), parts[1]))
unobs = set(x for x in a.unobservable.split(',') if x)

def rows(t0, t1):
    k = (t >= t0) & (t < t1)
    return P[k]
def race(p): return struct.unpack_from('<i', p.tobytes(), 0)[0]
def steer(p): return struct.unpack_from('<b', p.tobytes(), 320)[0]

results = []
for n, (ts, label) in enumerate(ev):
    w = label.split()
    if len(w) < 3 or w[0] != 'inject': continue
    action, value = w[1], float(w[2])
    end = ev[n + 1][0] if n + 1 < len(ev) else ts + 3
    before, during = rows(ts - 1.0, ts), rows(ts, min(end, ts + 4.0))
    after = rows(ts, min(end, ts + 4.0) + 1.5)
    r = {'action': action, 'value': value, 'packets': int(len(during))}
    if not len(during):
        r['result'] = 'no packets'; results.append(r); continue
    raceOn = bool((during[:, 0] != 0).any())
    if action in unobs:
        r['result'] = 'not observable (this game\'s telemetry has no field for it)'
    elif action == 'steer':
        s = [steer(p) for p in during]; best = max(s) if value > 0 else min(s)
        r['seen'] = best; need = 0.25 * value * 127
        r['result'] = 'input observed' if (best >= need if value > 0 else best <= need) and abs(best) >= 10 else ('not observed (race-on 0)' if not raceOn else 'not observed')
    elif action in ('throttle', 'brake', 'clutch', 'handbrake'):
        off = {'throttle': 315, 'brake': 316, 'clutch': 317, 'handbrake': 318}[action]
        best = int(during[:, off].max()); base = int(before[:, off].max()) if len(before) else 0
        r['seen'] = best; r['before'] = base
        ok = best >= 0.5 * value * 255 and best > base
        # Automatic gearboxes swap the pedals in reverse (gear 0): the brake pedal drives backwards, the throttle brakes.
        if not ok and action in ('throttle', 'brake') and (during[:, 319] == 0).any():
            other = 316 if action == 'throttle' else 315
            rev = during[during[:, 319] == 0]
            alt = int(rev[:, other].max()); alt_base = int(before[:, other].max()) if len(before) else 0
            if alt >= 0.5 * value * 255 and alt > alt_base:
                ok = True
                r['seen'] = '%d as %s (reverse)' % (alt, 'brake' if action == 'throttle' else 'throttle')
        r['result'] = 'input observed' if ok else ('not observed (race-on 0)' if not raceOn else 'not observed')
    elif action in ('shiftUp', 'shiftDown'):
        g0 = int(before[-1, 319]) if len(before) else int(during[0, 319])
        g = [int(x) for x in after[:, 319]]
        moved = (max(g) > g0) if action == 'shiftUp' else (min(g) < g0)
        r['seen'] = f'{g0} -> {g[-1]}'
        r['result'] = 'input observed' if moved else 'not observed (an automatic gearbox ignores shift buttons)'
    elif action == 'start':
        r0 = race(before[-1]) if len(before) else race(during[0])
        flips = [race(p) for p in after]
        changed = [x for x in flips if x != r0]
        r['seen'] = f'race-on {r0} -> {changed[0] if changed else r0}'
        r['result'] = 'input observed' if changed else 'not observed'
    else:
        r['result'] = 'not observable by telemetry (menu or camera action)'
    results.append(r)

print(f'{"action":10} {"value":>6} {"packets":>7}  seen            result')
for r in results:
    print(f'{r["action"]:10} {r["value"]:6.2f} {r["packets"]:7d}  {str(r.get("seen", "")):15} {r["result"]}')
observed = sum(1 for r in results if r['result'] == 'input observed')
print(f'{observed} of {len(results)} injections observed in the game\'s car input')
if a.json: json.dump(results, open(a.json, 'w'), indent=1)
