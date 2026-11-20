# Extract acknex.dll's lite-C symbol table (16-byte entries: name ptr, address, n, type) -> "<addr> <name> <n> <type>" lines.
#   python -I acknames.py <acknex.dll> <out.txt>
import struct, sys
f = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', f, 0x3c)[0]; ib = struct.unpack_from('<I', f, pe + 52)[0]
ns = struct.unpack_from('<H', f, pe + 6)[0]; so = pe + 24 + struct.unpack_from('<H', f, pe + 20)[0]
secs = [struct.unpack_from('<IIII', f, so + 40 * i + 8) for i in range(ns)]   # vs, va, rs, rp
def v2f(v):
    for vs, va, rs, rp in secs:
        if ib + va <= v < ib + va + min(vs, rs): return v - ib - va + rp
def f2v(o):
    for vs, va, rs, rp in secs:
        if rp <= o < rp + rs: return ib + va + o - rp
def cstr(v):
    o = v2f(v)
    if o is None: return None
    e = f.find(b'\0', o, o + 64)
    if e <= o: return None
    s = f[o:e]
    return s.decode() if all(48 <= c < 123 and (chr(c).isalnum() or c == 95) for c in s) else None
def entry(o):
    n, a, k, t = struct.unpack_from('<IIII', f, o)
    s = cstr(n)
    return (s, a, k, t) if s and ib <= a < ib + 0x800000 else None
ref = struct.pack('<I', f2v(f.find(b'\0c_trace\0') + 1))
start = f.find(ref)
lo = start
while entry(lo - 16): lo -= 16
hi = start
while entry(hi + 16): hi += 16
rows = []
for o in range(lo, hi + 16, 16):
    s, a, k, t = entry(o); rows.append('%08x %s %d %d' % (a, s, k, t))
# engine variables: 24-byte entries {type, name ptr, address, ...}; anchored at "total_frames"
def ventry(o):
    t, n, a = struct.unpack_from('<III', f, o)
    s = cstr(n)
    return (s, a, t) if s and ib <= a < ib + 0x800000 and t < 256 else None
ref = struct.pack('<I', f2v(f.find(b'\0total_frames\0') + 1))
vs0 = f.find(ref) - 4
lo = vs0
while ventry(lo - 24): lo -= 24
hi = vs0
while ventry(hi + 24): hi += 24
nv = 0
for o in range(lo, hi + 24, 24):
    s, a, t = ventry(o); rows.append('%08x %s var %d' % (a, s, t)); nv += 1
# variables registered by code: push <addr>; push <name>; call register
import re
seen = set(r.split()[1] for r in rows)
for m in re.finditer(rb'\x68(....)\x68(....)[\x50-\x57]?\xe8', f, re.S):
    a, n = struct.unpack('<II', m.group(1) + m.group(2)); s = cstr(n)
    if s and s not in seen and 0x10100000 <= a < ib + 0x800000: seen.add(s); rows.append('%08x %s var -1' % (a, s)); nv += 1
with open(sys.argv[2], 'w') as out: out.write('\n'.join(rows) + '\n')
print('%d entries, %d variables' % (len(rows) - nv, nv))
