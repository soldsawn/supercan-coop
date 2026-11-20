import sys,struct
d=open(sys.argv[1],'rb').read(200000)
print(d[:8].hex(), struct.unpack('>I',d[4:8])[0], struct.unpack('<I',d[4:8])[0])
def lzss(src, N=4096, F=18, T=2, initfill=0x20, poslsb=True, rpos=None):
    buf=bytearray([initfill]*N); r=N-F if rpos is None else rpos; out=bytearray(); i=0; flags=0
    while i<len(src) and len(out)<4000:
        flags>>=1
        if not flags&0x100:
            flags=src[i]|0xff00; i+=1
        if flags&1:
            c=src[i]; i+=1; out.append(c); buf[r]=c; r=(r+1)%N
        else:
            a=src[i]; b=src[i+1]; i+=2
            p=a|((b&0xf0)<<4); l=(b&0x0f)+T+1
            for k in range(l):
                c=buf[(p+k)%N]; out.append(c); buf[r]=c; r=(r+1)%N
    return bytes(out)
o=lzss(d[8:])
print(o[:1500])
