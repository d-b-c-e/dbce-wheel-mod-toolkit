"""Static recon for 32-bit EGO exes (DiRT 3): strings matching patterns and the instructions that hold their absolute
address (push imm32, mov r32, imm32, operands with a disp32). Read-only.

Usage: py -3 ego_strings32.py <exe> <regex> [<regex> ...]
"""
import re
import struct
import sys

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe, patterns = sys.argv[1], [re.compile(p.encode(), re.I) for p in sys.argv[2:]]
pe = pefile.PE(exe, fast_load=True)
image = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
tdata, tbase = text.get_data(), text.VirtualAddress
md = Cs(CS_ARCH_X86, CS_MODE_32)

strings = []
for s in pe.sections:
    if s.Name.startswith(b".text"): continue
    data = s.get_data()
    for m in re.finditer(rb"[\x20-\x7e]{4,200}\x00", data):
        raw = m.group(0)[:-1]
        if any(p.search(raw) for p in patterns): strings.append((s.VirtualAddress + m.start(), raw.decode()))


def insn_covering(offset):
    for back in range(1, 8):
        start = offset - back
        if start < 0: continue
        for insn in md.disasm(tdata[start:start + 16], image + tbase + start, count=1):
            if insn.address <= image + tbase + offset and insn.address + insn.size >= image + tbase + offset + 4:
                return insn
    return None


for rva, text_value in strings:
    needle = struct.pack("<I", image + rva)
    refs, at = [], tdata.find(needle)
    while at >= 0 and len(refs) < 12:
        insn = insn_covering(at)
        refs.append(f"{insn.address - image:#x} {insn.mnemonic} {insn.op_str}" if insn else f"{tbase + at:#x} ?")
        at = tdata.find(needle, at + 1)
    print(f"{rva:#x} {text_value!r}")
    for r in refs: print("    ", r)
