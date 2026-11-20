# Make Ghidra's decompile of the lite-C dump readable:
#  - DAT_<slot>   -> engine name (slot holds an acknex function/variable address: c_trace, me, you, ...)
#  - DAT_<const>  -> its value (lite-C keeps literals in memory; never written / never address-taken = a constant)
#  - DAT_<global> -> g_<addr>  (written somewhere or address taken)
#  - calls/refs to function starts -> function names
#   python -I scnames.py <dump dir> <decomp_raw.c> <decomp.c>
import struct, sys, os, re, glob, capstone
from capstone import x86

dd, src, dst = sys.argv[1:4]
reg = sorted(glob.glob(os.path.join(dd, 'region_*.bin')))[0]
B = int(re.search(r'region_([0-9a-f]+)', reg).group(1), 16)
data = open(reg, 'rb').read(); S = len(data)
fn = {}
for ln in open(os.path.join(dd, 'functions.txt')):
    a, n = ln.split()[:2]; fn.setdefault(int(a, 16), n)
ack = {}
for ln in open(os.path.join(dd, 'acknex_names.txt')):
    p = ln.split(); ack.setdefault(int(p[0], 16), p[1])
starts = sorted(fn)
EBX = None
o = starts[0] - B
if data[o + 9:o + 12] == b'\x58\x8b\xd8': EBX = (starts[0] + 9 - struct.unpack_from('<I', data, o + 14)[0]) & 0xffffffff
print('ebx = %08x' % EBX)

WR = {'mov', 'add', 'sub', 'and', 'or', 'xor', 'inc', 'dec', 'neg', 'not', 'shl', 'shr', 'sar', 'sal', 'fstp', 'fst',
      'fistp', 'fist', 'xchg', 'movsd', 'movss', 'movq', 'stosd', 'adc', 'sbb', 'rol', 'ror', 'cmpxchg', 'setne', 'sete'}
cache = os.path.join(dd, 'ebx_access.txt')
written, taken, read = set(), set(), set()
if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(reg):
    for ln in open(cache):
        k, a = ln.split(); {'w': written, 't': taken, 'r': read}[k].add(int(a, 16))
else:
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
    ends = starts[1:] + [B + S]
    for st, en in zip(starts, ends):
        for ins in md.disasm(data[st - B:en - B], st):
            for i, op in enumerate(ins.operands):
                if op.type != x86.X86_OP_MEM or op.mem.base != x86.X86_REG_EBX or op.mem.index != 0: continue
                a = (EBX + op.mem.disp) & 0xffffffff
                if ins.mnemonic == 'lea': taken.add(a)
                elif i == 0 and ins.mnemonic in WR: written.add(a)
                else: read.add(a)
            if ins.mnemonic in ('ret', 'retn') and ins.address + ins.size >= en - 16: break
    with open(cache, 'w') as f:
        for k, s in (('w', written), ('t', taken), ('r', read)):
            for a in sorted(s): f.write('%s %08x\n' % (k, a))
print('ebx refs: %d read, %d written, %d address-taken' % (len(read), len(written), len(taken)))

def dw(a):
    return struct.unpack_from('<I', data, a - B)[0] if 0 <= a - B <= S - 4 else None
def name_for(a):
    if a == B + 0x10: return '1024.0'
    if a in fn: return fn[a]
    v = dw(a)
    if v in ack: return 'E_' + ack[v]          # slot: *E_me = the me pointer, E_c_trace(...) = call
    if a in written or a in taken: return 'g_%08x' % a
    if a in read and v is not None:
        sv = v - (1 << 32) if v & 0x80000000 else v
        if -100000 <= sv <= 100000: return str(sv)
        if sv & 0x3ff == 0 and -100000 * 1024 <= sv <= 100000 * 1024: return '%d/*var %g*/' % (sv, sv / 1024.0)
        return '0x%x' % v
    return None

txt = open(src, encoding='utf-8').read()
pat = re.compile(r'\b(?:(?:FUN|LAB|DAT|PTR_[A-Za-z0-9]+|_DAT)_|[a-z]+Ram)([0-9a-f]{8})\b')
def sub(m):
    n = name_for(int(m.group(1), 16))
    return n if n else m.group(0)
txt = pat.sub(sub, txt)
# function pointers passed as literals (0x81c1995 style) -> names
txt = re.sub(r'\b0x([0-9a-f]{7,8})\b', lambda m: fn.get(int(m.group(1), 16), m.group(0)) if int(m.group(1), 16) in fn else m.group(0), txt)
open(dst, 'w', encoding='utf-8').write(txt)
print('wrote', dst)
