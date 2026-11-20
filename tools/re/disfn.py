# Disassemble a function by name from local/dump/code with full symbol/global/string/constant annotations.
#   python -I local/tools/disfn.py <func_name> [max_ins]
import struct, sys, os, re, glob, capstone

dd = os.path.join(os.path.dirname(__file__), '..', 'dump', 'code')
reg = sorted(glob.glob(os.path.join(dd, 'region_*.bin')))[0]
B = int(re.search(r'region_([0-9a-f]+)', reg).group(1), 16)
data = open(reg, 'rb').read()
EBX = 0x07d40045
ack = {int(p[0], 16): p[1] for p in (ln.split() for ln in open(os.path.join(dd, 'acknex_names.txt')))}
fn = {int(p[0], 16): p[1] for p in (ln.split()[:2] for ln in open(os.path.join(dd, 'functions.txt')))}
starts = sorted(fn)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

name = sys.argv[1]
max_ins = int(sys.argv[2]) if len(sys.argv) > 2 else 400
matches = [a for a, n in fn.items() if n == name or name.lower() in n.lower()]
for st in matches[:5]:
    idx = starts.index(st)
    en = starts[idx + 1] if idx + 1 < len(starts) else B + len(data)
    print(f'=== {fn[st]} @ {st:08x}..{en:08x} ({en-st} bytes) ===')
    buf = bytearray(data[st - B:en - B])
    # Replace inline 8-byte double constants (e8 08 00 00 00 <8 bytes> 58) with NOPs so capstone doesn't desync
    for o in range(len(buf) - 14):
        if buf[o:o+5] == b'\xe8\x08\x00\x00\x00' and buf[o+13] == 0x58:
            buf[o+5:o+13] = b'\x90' * 8
    for k, ins in enumerate(md.disasm(bytes(buf), st)):
        if k >= max_ins:
            print(f'... ({k} instructions shown)')
            break
        note = []
        if ins.mnemonic == 'call' and ins.bytes == b'\xe8\x08\x00\x00\x00':
            dval = struct.unpack_from('<d', data, ins.address + 5 - B)[0]
            note.append(f'inline_double={dval:g}')
        for m in re.finditer(r'\[ebx \+ 0x([0-9a-f]+)\]', ins.op_str):
            a = EBX + int(m.group(1), 16)
            if 0 <= a - B <= len(data) - 4:
                v = struct.unpack_from('<I', data, a - B)[0]
                sv = v - (1 << 32) if v & 0x80000000 else v
                s = data[a - B:a - B + 40].split(b'\0')[0]
                stxt = f' "{s.decode("latin-1")}"' if len(s) > 2 and all(32 <= c < 127 for c in s) else ''
                if v in ack:
                    note.append(f'E_{ack[v]}')
                else:
                    note.append(f'g_{a:08x}={sv}({sv/1024.0:g}v){stxt}')
        for m in re.finditer(r'0x([0-9a-f]{6,8})', ins.op_str):
            ca = int(m.group(1), 16)
            if ca in fn:
                note.append(fn[ca])
            elif 0 <= ca < 0x100000 and ins.mnemonic == 'add':
                pass
            elif 0x200000 <= ca < len(data) and ins.mnemonic == 'add':
                a = EBX + ca
                s = data[a - B:a - B + 40].split(b'\0')[0]
                if len(s) > 1 and all(32 <= c < 127 for c in s):
                    note.append(f'&g_{a:08x} "{s.decode("latin-1")}"')
                else:
                    note.append(f'&g_{a:08x}')
        print(f'{ins.address:08x}: {ins.mnemonic:6s} {ins.op_str:32s}' + (' ; ' + ', '.join(note) if note else ''))
