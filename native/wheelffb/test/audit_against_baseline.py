"""Function-level audit of WheelFfb.cpp against a baseline revision (default c8ec2ee, native-0.9.1).

Prints every top-level function whose body differs, the added/removed lines in it, and whether
each force/init/send/hold/effect entry point is byte-identical. Source only: nothing is built or run.
Usage: python audit_against_baseline.py [baseline-revision]
"""
import difflib, re, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / 'WheelFfb.cpp'
BASE = sys.argv[1] if len(sys.argv) > 1 else 'c8ec2ee'

def functions(src):
    src = src.replace('\r\n', '\n')
    out = {}
    signature = re.compile(r'^(?:__declspec\(dllexport\)\s+)?(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w:<>\*\s&]*?\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(?:const\s*)?\n?\{', re.M)
    for m in signature.finditer(src):
        name = m.group(1)
        if name in ('if', 'for', 'while', 'switch', 'return', 'sizeof'): continue
        j, depth, quote, escaped = m.end() - 1, 0, None, False
        while j < len(src):
            c = src[j]
            if quote:
                if escaped: escaped = False
                elif c == '\\': escaped = True
                elif c == quote: quote = None
            elif src.startswith('//', j): j = src.find('\n', j); continue
            elif src.startswith('/*', j): j = src.find('*/', j) + 2; continue
            elif c in '"\'': quote = c
            elif c == '{': depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0: break
            j += 1
        key, n = name, 2
        while key in out: key, n = f'{name}#{n}', n + 1
        out[key] = src[m.start():j + 1]
    return out

root = subprocess.run(['git', '-C', str(HERE), 'rev-parse', '--show-toplevel'], capture_output=True, text=True, check=True).stdout.strip()
relative = SOURCE.relative_to(Path(root)).as_posix()
base = functions(subprocess.run(['git', '-C', root, 'show', f'{BASE}:{relative}'], capture_output=True, text=True, encoding='utf-8', check=True).stdout)
port = functions(SOURCE.read_text(encoding='utf-8'))
changed = [k for k in base if k in port and base[k] != port[k]]
print(f'{BASE}: {len(base)} functions; working copy: {len(port)}; identical: {len(base) - len(changed)}')
print('removed:', [k for k in base if k not in port], 'added:', [k for k in port if k not in base])
for k in changed:
    lines = [l for l in difflib.unified_diff(base[k].split('\n'), port[k].split('\n'), lineterm='', n=0) if l[:1] in '+-' and not l.startswith(('+++', '---'))]
    print(f'changed {k}: {lines}')
force = ['InitDirectInput', 'Aquire', 'SetDeviceForcesXY', 'StartEffect', 'StopEffect', 'SetAutoCenter', 'FreeDirectInput', 'PanicStop',
         'ZeroForces', 'InstallExitGuards', 'SetHoldTimeoutMs', 'CreatePeriodicEffect', 'UpdatePeriodicEffect', 'CreatePeriodicBurst',
         'PlayPeriodicBurst', 'PlayShapedPeriodicBurst', 'CreateConstantBurst', 'PlayConstantBurst', 'StopConstantBurst',
         'ReleaseConstantBursts', 'StopPeriodicBurst', 'ReleasePeriodicEffects', 'CreateConditionEffect', 'UpdateConditionEffect',
         'ReleaseConditionEffects', 'SetStrictDeviceSelection', 'SetPreferredDevice', 'SetPreferredDeviceGuid', 'SetPreferredDeviceIndex',
         'GetWheelFfbVersion', 'AcquireFfbDevice', 'CloseCandidate']
different = [f for f in force if f not in base or f not in port or base[f] != port[f]]
print(f'force/init/send/hold/effect entry points: {len(force) - len(different)} of {len(force)} identical; differing: {different}')
sys.exit(1 if different else 0)
