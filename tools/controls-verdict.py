# STD-033 "input observed": did the game's own car input answer each injected control? (toolkit tools/; the
# synthetic check is controls-verdict-fixture.py: py -3 controls-verdict-fixture.py f.npz f.txt, then this on them) Reads a Forza Horizon capture
# The default mode is a HEURISTIC (one peak past a threshold, race-on checked only on failure); --strict judges each sample
# by eligibility (every packet race-on), a change from the pre-sample baseline and a sustained plateau, optionally inside
# a per-game band (--expect bands.json: {"throttle 0.5": [min, max], "steer 0.5": [min, max], ...}).
# (udp-capture.py .npz: t, p) and the event marks ("<unix-seconds> inject <action> <n>" lines), and judges each
# injection window against the packet fields the game's telemetry provides. Fields a game does not send are reported as
# not observable, never as a pass.
#   py -3 controls-verdict.py CAPTURE.npz EVENTS.txt [--unobservable clutch,handbrake] [--json out.json]
import argparse, json, struct, numpy as np

ap = argparse.ArgumentParser()
ap.add_argument('capture'); ap.add_argument('events')
ap.add_argument('--unobservable', default='', help='actions this game\'s telemetry cannot show (comma list)')
ap.add_argument('--json')
ap.add_argument('--strict', action='store_true', help='eligibility, baseline change and sustained plateau per sample')
ap.add_argument('--expect', help='strict: JSON of per-sample bands for this game (\"action value": [min, max])')
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
    if len(w) < 3 or w[0] != 'inject' or w[1] == 'raw': continue   # raw lines are menu steps (inject-raw.ps1), not the action sequence
    action, value = w[1], float(w[2])
    end = ev[n + 1][0] if n + 1 < len(ev) else ts + 3
    before, during = rows(ts - 1.0, ts), rows(ts, min(end, ts + 4.0))
    after = rows(ts, min(end, ts + 4.0) + 1.5)
    r = {'action': action, 'value': value, 'packets': int(len(during)), '_ts': ts, '_end': end}
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


FIELD = {'throttle': 315, 'brake': 316, 'clutch': 317, 'handbrake': 318}
bands = json.load(open(a.expect, encoding='utf-8-sig')) if a.expect else {}
def word(p, action):
    return struct.unpack_from('<b', p.tobytes(), 320)[0] if action == 'steer' else int(p[FIELD[action]])
def strict_case(action, value, ts, end):
    # Skip 0.15 s for delivery; the sample's own window ends at the next mark (or 4 s).
    t0, t1 = ts + 0.15, min(end, ts + 4.0)
    k = (t >= t0) & (t < t1); win, wt = P[k], t[k]
    if action in unobs: return {'result': 'unknown', 'reason': 'not observable in this game''s telemetry'}
    if action not in FIELD and action != 'steer': return {'result': 'unknown', 'reason': 'no strict rule for this action'}
    if len(win) < 5: return {'result': 'unknown', 'reason': 'fewer than 5 packets in the window'}
    if (win[:, 0] == 0).any(): return {'result': 'unknown', 'reason': 'race-on 0 in the window (not an eligible driving window)'}
    # Continuous sampling: no gap longer than 5 packet intervals (at least 0.25 s), and the window covered end to end.
    dts = np.diff(wt)
    gap = max(0.25, 5 * float(np.median(dts))) if len(dts) else 0.25
    if len(dts) and dts.max() > gap: return {'result': 'unknown', 'reason': f'a {dts.max():.2f} s gap in the packets (not continuous sampling)'}
    if (wt[0] - t0) > gap or (t1 - wt[-1]) > gap: return {'result': 'unknown', 'reason': 'the packets do not cover the window'}
    # An eligible baseline: at least 5 race-on packets in the second before the sample.
    b = (t >= ts - 1.0) & (t < ts); base = P[b]
    if len(base) < 5 or (base[:, 0] == 0).any(): return {'result': 'unknown', 'reason': 'no eligible baseline (fewer than 5 race-on packets in the second before)'}
    # An automatic gearbox in reverse (gear 0) swaps the pedals: throttle drives the brake word and the reverse.
    # Decided per packet, since a brake that stops the car crosses into reverse within its own window.
    swap = {'throttle': 'brake', 'brake': 'throttle'}
    def fld(p): return swap[action] if action in swap and p[319] == 0 else action
    vals = np.array([word(p, fld(p)) for p in win])
    field = action if not (action in swap and (win[:, 319] == 0).any()) else action + '/' + swap[action]
    full = 127 if action == 'steer' else 255
    lo, hi = bands.get(f'{action} {value:g}', (None, None))
    if lo is None:
        # No game band: the sign and at least 40 % of the sample's magnitude (pedals also at most full scale).
        need = 0.4 * abs(value) * full
        inside = (vals >= need) if value > 0 else (vals <= -need)
    else:
        inside = (vals >= lo) & (vals <= hi)
    # Longest contiguous run inside the band, in seconds.
    best = run = 0.0; start = None
    for i in range(len(vals)):
        if inside[i]:
            start = wt[i] if start is None else start; run = wt[i] - start
            best = max(best, run)
        else: start = None
    frac = float(inside.mean())
    bv = np.array([word(p, fld(p)) for p in base]); bmed = float(np.median(bv))
    if (lo is None and (bmed >= need if value > 0 else bmed <= -need)) or (lo is not None and lo <= bmed <= hi):
        return {'result': 'unchanged', 'reason': f'the baseline {bmed:g} was already inside the target', 'held_s': round(best, 2), 'fraction': round(frac, 2)}
    ok = best >= 0.3 and frac >= 0.5
    # Only a per-game band calibrates the value; without one, a held move in the right direction is a response.
    hit = 'observed' if lo is not None else 'response (directional; no per-game band)'
    return {'result': (hit if ok else 'mismatch') + (' (in reverse in part: ' + field + ')' if field != action else ''), 'held_s': round(best, 2), 'fraction': round(frac, 2), 'baseline': bmed,
            'range': [int(vals.min()), int(vals.max())], 'band': [lo, hi] if lo is not None else f'sign and >= {need:.0f}'}
if a.strict:
    for n, r in enumerate(results):
        ts = r.pop('_ts'); end = r.pop('_end')
        r['strict'] = strict_case(r['action'], r['value'], ts, end) if r['packets'] else {'result': 'unknown', 'reason': 'no packets'}
else:
    for r in results: r.pop('_ts', None); r.pop('_end', None)
print(('STRICT ' if a.strict else 'HEURISTIC (one peak; see --strict) ') + 'verdict')
print(f'{"action":10} {"value":>6} {"packets":>7}  seen            result')
for r in results:
    print(f'{r["action"]:10} {r["value"]:6.2f} {r["packets"]:7d}  {str(r.get("seen", "")):15} {r["result"]}' + (f'  | strict: {r["strict"]["result"]}' + (f' ({r["strict"].get("reason")})' if r["strict"].get('reason') else f' held {r["strict"].get("held_s")} s') if 'strict' in r else ''))
observed = sum(1 for r in results if r['result'] == 'input observed')
if a.strict: print(f'strict: {sum(1 for r in results if r["strict"]["result"].startswith("observed"))} observed, ' + ', '.join(f'{sum(1 for r in results if r["strict"]["result"].startswith(k))} {k}' for k in ('response', 'unchanged', 'mismatch', 'unknown')))
print(f'{observed} of {len(results)} injections observed in the game\'s car input')
if a.json: json.dump(results, open(a.json, 'w'), indent=1)
