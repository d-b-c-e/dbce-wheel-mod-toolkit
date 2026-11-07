"""Static recon for EGO-engine split-screen: find strings and the code that references them (RIP-relative LEA/MOV).

Usage: py -3 ego_strings.py <exe> <pattern> [<pattern> ...]
Prints each matching string (ASCII, NUL-terminated) with its RVA, then every instruction in .text that addresses it
via RIP-relative disp32 (48/4C 8D xx / 48/4C 8B xx), with the start of its enclosing function guessed from the .pdata
table. Read-only; nothing is written next to the game.
"""
import re
import struct
import sys
import bisect

import pefile

exe, patterns = sys.argv[1], [p.encode() for p in sys.argv[2:]]
pe = pefile.PE(exe, fast_load=False)
base = pe.OPTIONAL_HEADER.ImageBase
data = pe.__data__
sections = {s.Name.rstrip(b"\0").decode(): s for s in pe.sections}
text = sections[".text"]
tbytes = text.get_data()
trva = text.VirtualAddress

# Function starts from .pdata (x64 RUNTIME_FUNCTION entries).
starts = []
if ".pdata" in sections:
    p = sections[".pdata"].get_data()
    for i in range(0, len(p) - 11, 12):
        b, e, _ = struct.unpack_from("<III", p, i)
        if b and e > b:
            starts.append((b, e))
starts.sort()
begins = [s for s, _ in starts]


def func_of(rva):
    i = bisect.bisect_right(begins, rva) - 1
    if i >= 0 and starts[i][0] <= rva < starts[i][1]:
        return starts[i][0]
    return None


def strings_matching(pat):
    found = []
    for s in pe.sections:
        name = s.Name.rstrip(b"\0").decode()
        if name not in (".rdata", ".data"):
            continue
        blob = s.get_data()
        for m in re.finditer(rb"[\x20-\x7e]{4,200}\x00", blob):
            if pat.lower() in m.group().lower():
                found.append((s.VirtualAddress + m.start(), m.group()[:-1].decode("ascii", "replace")))
    return found


# One pass over .text for RIP-relative LEA/MOV with REX.W (48/4C) 8D/8B modrm with mod=00 rm=101.
refs = {}
for m in re.finditer(rb"[\x48\x4c][\x8d\x8b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", tbytes):
    off = m.start()
    if off + 7 > len(tbytes):
        continue
    disp = struct.unpack_from("<i", tbytes, off + 3)[0]
    target = trva + off + 7 + disp
    refs.setdefault(target, []).append(trva + off)

for pat in patterns:
    hits = strings_matching(pat)
    print(f"== {pat.decode()}: {len(hits)} strings")
    for rva, s in hits[:40]:
        users = refs.get(rva, [])
        funcs = sorted({func_of(u) for u in users if func_of(u) is not None})
        print(f"  {rva:#010x} {s!r:70} refs {len(users)} funcs {[hex(f) for f in funcs[:6]]}")
