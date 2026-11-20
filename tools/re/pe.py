import sys,struct
def exports(path):
    d=open(path,'rb').read()
    pe=struct.unpack_from('<I',d,0x3c)[0]; n=struct.unpack_from('<H',d,pe+6)[0]; optsz=struct.unpack_from('<H',d,pe+20)[0]
    opt=pe+24; secs=[]
    for i in range(n):
        o=opt+optsz+i*40; name=d[o:o+8].rstrip(b'\0'); vs,va,rs,rp=struct.unpack_from('<IIII',d,o+8); secs.append((name,va,vs,rp,rs))
    def r2o(rva):
        for s in secs:
            if s[1]<=rva<s[1]+max(s[2],s[4]): return rva-s[1]+s[3]
    erva,esz=struct.unpack_from('<II',d,opt+96); irva,isz=struct.unpack_from('<II',d,opt+104)
    res={'sections':secs,'exports':[],'imports':{}}
    if erva:
        e=r2o(erva); nn=struct.unpack_from('<I',d,e+24)[0]; an=r2o(struct.unpack_from('<I',d,e+32)[0])
        for i in range(nn):
            no=r2o(struct.unpack_from('<I',d,an+i*4)[0]); res['exports'].append(d[no:d.index(b'\0',no)].decode())
    if irva:
        o=r2o(irva)
        while True:
            ilt,_,_,nm,iat=struct.unpack_from('<IIIII',d,o)
            if not nm: break
            dll=d[r2o(nm):d.index(b'\0',r2o(nm))].decode(); fns=[]
            t=r2o(ilt or iat)
            while True:
                v=struct.unpack_from('<I',d,t)[0]
                if not v: break
                if v&0x80000000: fns.append('#%d'%(v&0xffff))
                else:
                    p=r2o(v)+2; fns.append(d[p:d.index(b'\0',p)].decode())
                t+=4
            res['imports'][dll]=fns; o+=20
    return res
if __name__=='__main__':
    r=exports(sys.argv[1])
    for s in r['sections']: print(s)
    print('EXPORTS',r['exports'])
    for k,v in r['imports'].items(): print('IMPORT',k,len(v), v[:60])
