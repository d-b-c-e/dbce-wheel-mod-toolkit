"""Disassemble the x86 function that contains an RVA (DiRT 3 and other 32-bit EGO exes). Read-only.

Usage: py -3 ego_disasm32.py <exe> <hex rva> [max instructions] [--mark rva,rva,...]
Finds the function start by scanning back (up to 64 KB) for the push ebp / mov ebp,esp prologue that follows padding
(CC/90) or a ret, then prints from there. Absolute operands that point at ASCII strings are annotated; call/jump
targets are also shown as RVAs. Lines whose address is a marked RVA (for example a logged return address) get "<==".
"""
import re
import sys

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe, target = sys.argv[1], int(sys.argv[2], 16)
limit = int(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith('--') else 400
marks = set()
if '--mark' in sys.argv:
    marks = {int(x, 16) for x in sys.argv[sys.argv.index('--mark') + 1].split(',')}
pe = pefile.PE(exe, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
tb, trva = text.get_data(), text.VirtualAddress
md = Cs(CS_ARCH_X86, CS_MODE_32)


def ascii_at(va):
    rva = va - base
    try:
        raw = pe.get_data(rva, 100)
    except Exception:
        return None
    m = re.match(rb'[\x20-\x7e]{4,100}', raw)
    return m.group(0).decode() if m and (raw[len(m.group(0)):len(m.group(0)) + 1] == b'\x00') else None


off = target - trva
start = None
for back in range(0, 65536):
    o = off - back
    if o < 1:
        break
    if tb[o:o + 3] == b'\x55\x8b\xec' and tb[o - 1] in (0xCC, 0x90, 0xC3, 0xC2):
        start = o
        break
if start is None:
    start = max(0, off - 0x80)
print(f'function start rva 0x{start + trva:x} (va 0x{start + trva + base:x}), target rva 0x{target:x}')
n = 0
for insn in md.disasm(tb[start:start + 0x4000], start + trva + base):
    rva = insn.address - base
    note = ''
    for m in re.finditer(r'0x([0-9a-f]{6,8})', insn.op_str):
        v = int(m.group(1), 16)
        if insn.mnemonic.startswith(('call', 'j')) and base <= v < base + 0x2000000:
            note += f'  ; -> rva 0x{v - base:x}'
        else:
            s = ascii_at(v)
            if s:
                note += f'  ; "{s}"'
    flag = '  <==' if rva in marks else ''
    print(f'{rva:08x}: {insn.mnemonic:7} {insn.op_str}{note}{flag}')
    n += 1
    if n >= limit or (insn.mnemonic.startswith('ret') and rva > target):
        break
