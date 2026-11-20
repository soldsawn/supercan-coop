"""Annotate a lite-C code dump: resolve [ebx+off] engine-call slots to acknex export names and show constants.
usage: python -I annotate_litec.py <code.bin> <code_base_hex> <data.bin> <ebx_hex> <acknex.dll> <start_hex> <len_hex>
"""
import re
import struct
import sys

import capstone

code_p, code_base, data_p, ebx, ack_p, start, length = sys.argv[1:8]
code_base, ebx, start, length = (int(x, 16) for x in (code_base, ebx, start, length))
code, data = open(code_p, "rb").read(), open(data_p, "rb").read()
DATA_OFF = 0x288000  # data dump starts at ebx+DATA_OFF

d = open(ack_p, "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
n = struct.unpack_from("<H", d, pe + 6)[0]
opt = pe + 24
secs = [struct.unpack_from("<IIII", d, opt + optsz + i * 40 + 8) for i in range(n)]


def r2o(rva):
    for vs, va, rs, rp in secs:
        if va <= rva < va + max(vs, rs):
            return rva - va + rp


e = r2o(struct.unpack_from("<I", d, opt + 96)[0])
nn = struct.unpack_from("<I", d, e + 24)[0]
af, an, ao = (r2o(struct.unpack_from("<I", d, e + k)[0]) for k in (28, 32, 36))
names = {}
for i in range(nn):
    no = r2o(struct.unpack_from("<I", d, an + i * 4)[0])
    o = struct.unpack_from("<H", d, ao + i * 2)[0]
    names[0x10000000 + struct.unpack_from("<I", d, af + o * 4)[0]] = d[no:d.index(b"\0", no)].decode()


def dword(off):
    k = off - DATA_OFF
    return struct.unpack_from("<I", data, k)[0] if 0 <= k <= len(data) - 4 else None


def cstr(off):
    k = off - DATA_OFF
    if not 0 <= k < len(data):
        return None
    s = data[k:k + 48].split(b"\0")[0]
    return s.decode("latin-1") if s and all(32 <= c < 127 or c >= 0xC0 for c in s) else None


def describe(off):
    v = dword(off)
    if v is None:
        return ""
    if v in names:
        return f"-> {names[v]}()"
    parts = [f"=0x{v:08x} ({v if v < 0x80000000 else v - 2**32})"]
    if 0x10000000 <= v < 0x10400000:
        parts.append("acknex data")
    s = cstr(off)
    if s:
        parts.append(f"str'{s}'")
    return " ".join(parts)


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
off0 = start - code_base
for ins in md.disasm(code[off0:off0 + length], start):
    note = ""
    for m in re.finditer(r"\[ebx \+ 0x([0-9a-f]+)\]", ins.op_str):
        note += " ; " + describe(int(m.group(1), 16))
    m = re.match(r"eax, 0x([0-9a-f]+)$", ins.op_str)
    if ins.mnemonic == "add" and m and int(m.group(1), 16) > 0x200000:
        o = int(m.group(1), 16)
        s = cstr(o)
        note += f" ; &ebx+0x{o:x}" + (f" str'{s}'" if s else f" ({describe(o)})")
    print(f"{ins.address:08x}  {ins.mnemonic:6s} {ins.op_str}{note}")
