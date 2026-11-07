"""Disassemble around code that references a string (or at an RVA). Read-only.

Usage: py -3 ego_disasm.py <exe> str:<exact string> [before] [after]
       py -3 ego_disasm.py <exe> rva:<hex rva> [count]
Annotates RIP-relative operands that point at ASCII strings.
"""
import re
import struct
import sys

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

exe, what = sys.argv[1], sys.argv[2]
pe = pefile.PE(exe, fast_load=True)
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
tb, trva = text.get_data(), text.VirtualAddress
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = False


def ascii_at(rva):
    try:
        raw = pe.get_data(rva, 120)
    except Exception:
        return None
    m = re.match(rb"([\x20-\x7e]{3,120})\x00", raw)
    return m.group(1).decode() if m else None


def show(start, count):
    off = start - trva
    for ins in md.disasm(tb[off:off + count * 15], start):
        note = ""
        m = re.search(r"\[rip ([+-]) 0x([0-9a-f]+)\]", ins.op_str)
        if m:
            disp = int(m.group(2), 16) * (1 if m.group(1) == "+" else -1)
            target = ins.address + ins.size + disp
            s = ascii_at(target)
            note = f"   ; {target:#x}" + (f" {s!r}" if s else "")
        elif ins.mnemonic == "call" and ins.op_str.startswith("0x"):
            note = ""
        print(f"{ins.address:#010x}  {ins.mnemonic:6} {ins.op_str}{note}")
        count -= 1
        if count <= 0:
            break


if what.startswith("rva:"):
    show(int(what[4:], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 40)
else:
    target_text = what[4:].encode() + b"\x00"
    rdata = next(s for s in pe.sections if s.Name.startswith(b".rdata"))
    idx = rdata.get_data().find(b"\x00" + target_text)
    srva = rdata.VirtualAddress + idx + 1
    before = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    after = int(sys.argv[4]) if len(sys.argv) > 4 else 30
    for m in re.finditer(rb"[\x48\x4c][\x8d\x8b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", tb):
        o = m.start()
        if trva + o + 7 + struct.unpack_from("<i", tb, o + 3)[0] == srva:
            print(f"--- reference at {trva + o:#x} to {srva:#x}")
            show(trva + o - before, before // 3 + after)
