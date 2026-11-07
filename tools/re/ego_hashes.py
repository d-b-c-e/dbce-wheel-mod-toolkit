"""Search an EGO exe for compile-time hashes of names (render-thread message ids etc.). Read-only.

Usage: py -3 ego_hashes.py <exe> <name> [<name> ...]
For each name (as given and lowercased) computes FNV-1/FNV-1a 32/64, CRC32, djb2, sdbm, Jenkins one-at-a-time and
murmur3_32 (seed 0) and reports every place the value appears in .text as a little-endian immediate, plus the
instruction there. Several related names hitting with the same algorithm is the evidence worth following.
"""
import struct
import sys
import zlib

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

M32, M64 = 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF


def fnv1_32(b):
    h = 0x811C9DC5
    for c in b: h = ((h * 0x01000193) & M32) ^ c
    return h


def fnv1a_32(b):
    h = 0x811C9DC5
    for c in b: h = ((h ^ c) * 0x01000193) & M32
    return h


def fnv1_64(b):
    h = 0xCBF29CE484222325
    for c in b: h = ((h * 0x100000001B3) & M64) ^ c
    return h


def fnv1a_64(b):
    h = 0xCBF29CE484222325
    for c in b: h = ((h ^ c) * 0x100000001B3) & M64
    return h


def djb2(b):
    h = 5381
    for c in b: h = (h * 33 + c) & M32
    return h


def sdbm(b):
    h = 0
    for c in b: h = (c + (h << 6) + (h << 16) - h) & M32
    return h


def oaat(b):
    h = 0
    for c in b:
        h = (h + c) & M32; h = (h + (h << 10)) & M32; h ^= h >> 6
    h = (h + (h << 3)) & M32; h ^= h >> 11; h = (h + (h << 15)) & M32
    return h


def murmur3_32(b, seed=0):
    c1, c2, h = 0xCC9E2D51, 0x1B873593, seed
    n = len(b) // 4
    for i in range(n):
        k = struct.unpack_from("<I", b, i * 4)[0]
        k = (k * c1) & M32; k = ((k << 15) | (k >> 17)) & M32; k = (k * c2) & M32
        h ^= k; h = ((h << 13) | (h >> 19)) & M32; h = (h * 5 + 0xE6546B64) & M32
    tail, k = b[n * 4:], 0
    if len(tail) >= 3: k ^= tail[2] << 16
    if len(tail) >= 2: k ^= tail[1] << 8
    if len(tail) >= 1:
        k ^= tail[0]; k = (k * c1) & M32; k = ((k << 15) | (k >> 17)) & M32; k = (k * c2) & M32; h ^= k
    h ^= len(b); h ^= h >> 16; h = (h * 0x85EBCA6B) & M32; h ^= h >> 13; h = (h * 0xC2B2AE35) & M32; h ^= h >> 16
    return h


ALGORITHMS = {
    "fnv1_32": (fnv1_32, 4), "fnv1a_32": (fnv1a_32, 4), "fnv1_64": (fnv1_64, 8), "fnv1a_64": (fnv1a_64, 8),
    "crc32": (lambda b: zlib.crc32(b) & M32, 4), "crc32_inv": (lambda b: (~zlib.crc32(b)) & M32, 4),
    "djb2": (djb2, 4), "sdbm": (sdbm, 4), "oaat": (oaat, 4), "murmur3_32": (murmur3_32, 4),
}

exe, names = sys.argv[1], sys.argv[2:]
pe = pefile.PE(exe, fast_load=True)
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
data, base = text.get_data(), text.VirtualAddress
md = Cs(CS_ARCH_X86, CS_MODE_64)


def instruction_at(offset):
    # Try instruction starts up to 12 bytes before the immediate and keep the one that covers it.
    for back in range(1, 13):
        start = offset - back
        if start < 0: continue
        for insn in md.disasm(data[start:start + 16], base + start, count=1):
            if insn.address + insn.size >= base + offset + 4 and insn.address <= base + offset:
                return f"{insn.address:#x}: {insn.mnemonic} {insn.op_str}"
    return "?"


hits_by_algorithm = {}
for name in names:
    for variant in {name, name.lower()}:
        raw = variant.encode()
        for algo, (fn, size) in ALGORITHMS.items():
            value = fn(raw)
            needle = struct.pack("<Q" if size == 8 else "<I", value)
            found, at = [], data.find(needle)
            while at >= 0 and len(found) < 8:
                found.append(at); at = data.find(needle, at + 1)
            if found:
                hits_by_algorithm.setdefault(algo, set()).add(name)
                for f in found:
                    print(f"{variant!r} {algo} {value:#x} at rva {base + f:#x}  {instruction_at(f)}")
print("names hit per algorithm:", {a: sorted(n) for a, n in hits_by_algorithm.items()})
