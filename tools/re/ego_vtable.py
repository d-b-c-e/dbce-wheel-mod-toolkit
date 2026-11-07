"""Find vtables holding a function RVA, list their entries, and the code that loads each vtable address (constructors).

Usage: py -3 ego_vtable.py <exe> <function rva hex>
"""
import re
import struct
import sys

import pefile

exe, frva = sys.argv[1], int(sys.argv[2], 16)
pe = pefile.PE(exe, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
rdata = next(s for s in pe.sections if s.Name.startswith(b".rdata"))
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
rb, rrva = rdata.get_data(), rdata.VirtualAddress
tb, trva = text.get_data(), text.VirtualAddress
needle = struct.pack("<Q", base + frva)
for m in re.finditer(re.escape(needle), rb):
    slot = rrva + m.start()
    # Walk back to the start of the vtable: entries are pointers into .text.
    start = m.start()
    while start >= 8:
        v = struct.unpack_from("<Q", rb, start - 8)[0] - base
        if trva <= v < trva + len(tb):
            start -= 8
        else:
            break
    entries = []
    i = start
    while i + 8 <= len(rb):
        v = struct.unpack_from("<Q", rb, i)[0] - base
        if not (trva <= v < trva + len(tb)):
            break
        entries.append(v)
        i += 8
    vt = rrva + start
    print(f"vtable {vt:#x}: slot of {frva:#x} = {(slot - vt) // 8}; {len(entries)} entries: {[hex(e) for e in entries[:12]]}")
    for mm in re.finditer(rb"[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", tb):
        o = mm.start()
        if trva + o + 7 + struct.unpack_from("<i", tb, o + 3)[0] == vt:
            print(f"  vtable loaded at {trva + o:#x}")
