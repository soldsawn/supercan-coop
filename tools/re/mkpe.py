# Wrap a dumpall dump (region_*.bin + functions.txt + acknex_exports.txt) into a PE32 DLL whose export table names every
# lite-C function and every engine-function slot (dwords holding an acknex export address), so Ghidra/rizin load it with names.
#   python -I mkpe.py <dump dir> <out.dll>
import struct, sys, os, glob, re

dd, out = sys.argv[1], sys.argv[2]
reg = sorted(glob.glob(os.path.join(dd, 'region_*.bin')))
B = int(re.search(r'region_([0-9a-f]+)', reg[0]).group(1), 16)
data = open(reg[0], 'rb').read()
S = len(data)

names = {}
for ln in open(os.path.join(dd, 'functions.txt')):
    a, n = ln.split()[:2]; names.setdefault(int(a, 16), n)
# lite-C prologue: push ebp/ebx/esi/edi; call $+5; pop eax; mov ebx,eax; sub ebx,imm  -> ebx = one global base.
# Restore our own inline hooks (E9 rel32 + 4 NOPs) and turn the position trick into "mov ebx, base" so decompilers see
# [ebx+off] as absolute addresses.
data = bytearray(data); PRO = bytes([0x55, 0x53, 0x56, 0x57, 0xE8, 0, 0, 0, 0]); bases = {}
for a in names:
    o = a - B
    if data[o] == 0xE9 and data[o + 5:o + 9] == b'\x90' * 4: data[o:o + 9] = PRO
    if data[o:o + 9] == PRO and data[o + 9:o + 12] == b'\x58\x8b\xd8' and data[o + 12:o + 14] == b'\x81\xeb':
        base = (a + 9 - struct.unpack_from('<I', data, o + 14)[0]) & 0xffffffff; bases[base] = bases.get(base, 0) + 1
        data[o + 4:o + 18] = b'\xbb' + struct.pack('<I', base) + b'\x90' * 9
# lite-C inline double 1024.0: "call $+13; <8-byte double 1024.0>; pop eax" (14 bytes).
# Place 1024.0 at B+0x10 and turn the 14-byte sequence into "mov eax, B+0x10; nop*9" so Ghidra doesn't see a PIC call over data.
D1024 = b'\x00\x00\x00\x00\x00\x00\x90\x40'
data[0x10:0x18] = D1024
PAT_D1024 = b'\xe8\x08\x00\x00\x00' + D1024 + b'\x58'
REP_D1024 = b'\xb8' + struct.pack('<I', B + 0x10) + b'\x90' * 9
ndbl = 0; pos = min(names) - B
while True:
    idx = data.find(PAT_D1024, pos)
    if idx < 0: break
    data[idx:idx + 14] = REP_D1024; ndbl += 1; pos = idx + 14
print('ebx bases:', {hex(k): v for k, v in bases.items()}, 'patched inline doubles:', ndbl)
data = bytes(data)
ack = {}
for ln in open(os.path.join(dd, 'acknex_exports.txt')):
    p = ln.split()
    if p[0] != 'base': ack[int(p[0], 16)] = p[1]
nslot = 0
for off in range(0, S - 3, 4):
    v = struct.unpack_from('<I', data, off)[0]
    if v in ack and B + off not in names:
        names[B + off] = 'p_' + ack[v]; nslot += 1

used = set(); ex = []
for a in sorted(names):
    n = re.sub(r'[^A-Za-z0-9_]', '_', names[a]); m = n; k = 2
    while m in used: m = '%s_%d' % (n, k); k += 1
    used.add(m); ex.append((m, a))
ex.sort()

SA, FA = 0x1000, 0x200
al = lambda x, a: (x + a - 1) // a * a
IB = B - 0x10000          # section 1 at RVA 0x10000 -> lands exactly at B
t_rva, t_raw = 0x10000, 0x400
e_rva = t_rva + al(S, SA)
n = len(ex)
# export section layout
dirsz = 40; eat = dirsz; npt = eat + 4 * n; ordt = npt + 4 * n; strs = ordt + 2 * n
dllname = b'scscript.dll\0'
blob = bytearray(strs); pos = strs
blob += dllname; name_rva = e_rva + pos; pos += len(dllname)
nrvas = []
for m, a in ex:
    nrvas.append(e_rva + pos); s = m.encode() + b'\0'; blob += s; pos += len(s)
struct.pack_into('<IIHHIIIIIII', blob, 0, 0, 0, 0, 0, name_rva, 1, n, n, e_rva + eat, e_rva + npt, e_rva + ordt)
for i, (m, a) in enumerate(ex):
    struct.pack_into('<I', blob, eat + 4 * i, a - IB)
    struct.pack_into('<I', blob, npt + 4 * i, nrvas[i])
    struct.pack_into('<H', blob, ordt + 2 * i, i)
E = len(blob)
e_raw = t_raw + al(S, FA)
img = e_rva + al(E, SA)

hdr = bytearray(0x400)
hdr[0:2] = b'MZ'; struct.pack_into('<I', hdr, 0x3c, 0x80)
o = 0x80; hdr[o:o + 4] = b'PE\0\0'
struct.pack_into('<HHIIIHH', hdr, o + 4, 0x14c, 2, 0, 0, 0, 0xe0, 0x2102)   # i386, 2 sections, DLL|32BIT|EXEC
op = o + 24
struct.pack_into('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII', hdr, op,
    0x10b, 14, 0, al(S, FA), al(E, FA), 0, 0, t_rva, t_rva, IB, SA, FA, 6, 0, 0, 0, 6, 0, 0, img, 0x400, 0, 2, 0,
    0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
struct.pack_into('<II', hdr, op + 96, e_rva, E)          # export directory
sh = op + 0xe0
def sec(off, nm, vs, va, rs, rp, ch):
    hdr[off:off + 8] = nm.ljust(8, b'\0'); struct.pack_into('<IIIIIIHHI', hdr, off + 8, vs, va, rs, rp, 0, 0, 0, 0, ch)
sec(sh, b'.text', S, t_rva, al(S, FA), t_raw, 0xE0000020)
sec(sh + 40, b'.edata', E, e_rva, al(E, FA), e_raw, 0x40000040)
with open(out, 'wb') as f:
    f.write(hdr); f.write(data); f.write(b'\0' * (al(S, FA) - S)); f.write(blob); f.write(b'\0' * (al(E, FA) - E))
print('base %08x size %x: %d functions, %d engine slots -> %s' % (B, S, len(names) - nslot, nslot, out))
