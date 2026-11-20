"""WRS4 reader for 3D GameStudio A8 resource archives (as used by SuperCan .gen files).

Layout (derived from SuperCan 1 harita_v0.gen):
  "WRS4" | u32be dir_offset-8 | entries' data (each LZSS-compressed separately) | directory
  directory: LZSS-compressed, then XOR 0xAA; 40-byte records:
      name[32] (NUL-padded) | u32be compressed_size | u32be uncompressed_size
LZSS: Okumura variant, N=4096, F=18, threshold 2, r starts at N-F, flag bit 1 = literal.
"""
import os
import struct
import sys


def lzss(src, limit=None, N=4096, F=18):
    buf = bytearray(N)
    r = N - F
    out = bytearray()
    i = 0
    flags = 0
    n = len(src)
    while i < n and (limit is None or len(out) < limit):
        flags >>= 1
        if not flags & 0x100:
            flags = src[i] | 0xFF00
            i += 1
            if i >= n:
                break
        if flags & 1:
            c = src[i]
            i += 1
            out.append(c)
            buf[r] = c
            r = (r + 1) & (N - 1)
        else:
            if i + 1 >= n:
                break
            a, b = src[i], src[i + 1]
            i += 2
            p = a | ((b & 0xF0) << 4)
            for k in range((b & 0x0F) + 3):
                c = buf[(p + k) & (N - 1)]
                out.append(c)
                buf[r] = c
                r = (r + 1) & (N - 1)
    return bytes(out[:limit] if limit else out)


def read_dir(d):
    assert d[:4] == b"WRS4", d[:4]
    off = 8 + struct.unpack(">I", d[4:8])[0]
    raw = bytes(c ^ 0xAA for c in lzss(d[off:]))
    entries = []
    for i in range(0, len(raw) - 39, 40):
        rec = raw[i:i + 40]
        name = bytes(c ^ 0xAA for c in rec[:32]).split(b"\0")[0]
        name = bytes(c ^ 0xAA for c in name).decode("latin-1")  # names come out clear after first xor
        csize, usize = struct.unpack(">II", bytes(c ^ 0xAA for c in rec[32:40]))
        entries.append((name, csize, usize))
    return off, entries


def extract(path, outdir=None, verbose=True):
    d = open(path, "rb").read()
    diroff, entries = read_dir(d)
    pos = 8
    results = []
    for name, csize, usize in entries:
        data = lzss(d[pos:pos + csize], limit=usize)
        ok = len(data) == usize
        results.append((name, pos, csize, usize, ok))
        if outdir:
            os.makedirs(outdir, exist_ok=True)
            with open(os.path.join(outdir, os.path.basename(name)), "wb") as f:
                f.write(data)
        pos += csize
    if verbose:
        for r in results:
            print("  %-34s off=%-9d c=%-9d u=%-9d %s" % (r[0], r[1], r[2], r[3], "OK" if r[4] else "SHORT"))
        print("  data end %d, dir at %d (gap %d)" % (pos, diroff, diroff - pos))
    return results


if __name__ == "__main__":
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else None
    paths = [os.path.join(src, f) for f in sorted(os.listdir(src))] if os.path.isdir(src) else [src]
    for p in paths:
        print(p)
        try:
            extract(p, os.path.join(out, os.path.basename(p)) if out else None)
        except Exception as e:
            print("  ERROR", e)
