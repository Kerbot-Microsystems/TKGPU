"""Cross-check codec interoperability and malformed regexp diagnostics."""
import argparse
import ctypes as c
import random
import zlib
from TKGPUReferenceListDifferential import hold, drop


def bind(path):
    lib=c.CDLL(path)
    P,I,S=c.c_void_p,c.c_int,c.c_char_p
    for name,args,result in [
        ("FindExecutable",[S],None),("CreateInterp",[],P),("DeleteInterp",[P],None),
        ("NewByteArrayObj",[S,I],P),("GetObjResult",[P],P),
        ("GetByteArrayFromObj",[P,c.POINTER(I)],P),
        ("GetStringResult",[P],S),("RegExpMatch",[P,S,S],I),
        ("ZlibInflate",[P,I,P,I,P],I),("ZlibDeflate",[P,I,P,I,P],I),
    ]:
        f=getattr(lib,"Tcl_"+name);f.argtypes,f.restype=args,result;setattr(lib,name,f)
    lib.TclFreeObj.argtypes,lib.TclFreeObj.restype=[P],None
    lib.FindExecutable(b"codec-differential")
    return lib


def bytes_of(lib,obj):
    n=c.c_int();p=lib.GetByteArrayFromObj(obj,c.byref(n))
    return c.string_at(p,n.value)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate");parser.add_argument("--reference",default="libtcl8.6.so")
    args=parser.parse_args()
    libs=[bind(args.reference),bind(args.candidate)]
    interps=[lib.CreateInterp() for lib in libs]
    count=0
    try:
        rng=random.Random(629)
        samples=[b"",b"hello world "*1000,bytes(range(256))*100,b"x"*1048576]
        samples += [rng.randbytes(rng.randrange(1,5000)) for _ in range(80)]
        for plain in samples:
            for fmt,bits in [(1,-15),(2,15),(4,31)]:
                for strategy in (zlib.Z_DEFAULT_STRATEGY,zlib.Z_FIXED):
                    compressor=zlib.compressobj(6,zlib.DEFLATED,bits,8,strategy)
                    packed=compressor.compress(plain)+compressor.flush()
                    for lib,interp in zip(libs,interps):
                        obj=hold(lib.NewByteArrayObj(packed,len(packed)))
                        try:
                            assert lib.ZlibInflate(interp,fmt,obj,0,None)==0,(fmt,len(plain),lib._name)
                            assert bytes_of(lib,lib.GetObjResult(interp))==plain
                            if fmt!=1:
                                assert lib.ZlibInflate(interp,8,obj,0,None)==0
                                assert bytes_of(lib,lib.GetObjResult(interp))==plain
                        finally: drop(lib,obj)
                    count+=1
                for level in (0,1,6,9):
                    for lib,interp in zip(libs,interps):
                        obj=hold(lib.NewByteArrayObj(plain,len(plain)))
                        try:
                            rc=lib.ZlibDeflate(interp,fmt,obj,level,None)
                            # Stock Tcl 8.6.13 rejects an empty input with buffer error.
                            if lib is libs[0] and not plain and rc==1: continue
                            assert rc==0,(fmt,level,len(plain),lib._name,lib.GetStringResult(interp))
                            assert zlib.decompress(bytes_of(lib,lib.GetObjResult(interp)),bits)==plain
                        finally: drop(lib,obj)
        # Require end-of-stream and checksum validation, not partial output.
        for fmt,bits in [(1,-15),(2,15),(4,31)]:
            compressor=zlib.compressobj(wbits=bits)
            packed=compressor.compress(b"verify integrity"*300)+compressor.flush()
            damaged=[packed[:1],packed[:-1],b"invalid compressed data"]
            if fmt!=1: damaged.append(packed[:-1]+bytes([packed[-1]^1]))
            for text in damaged:
                results=[]
                for lib,interp in zip(libs,interps):
                    obj=hold(lib.NewByteArrayObj(text,len(text)))
                    try: results.append(lib.ZlibInflate(interp,fmt,obj,0,None))
                    finally: drop(lib,obj)
                assert results==[1,1],(fmt,results,text[:16])
        invalid=[b"[",b"[abc",b"[^",b"[]",b"[z-a]",b")",b"(",b"a(",b"a\\",b"*a",b"a**",b"a++"]
        for pattern in invalid:
            for lib,interp in zip(libs,interps):
                assert lib.RegExpMatch(interp,b"some text",pattern)==-1,(pattern,lib._name)
                assert lib.GetStringResult(interp)
        for text,pattern in [(b"abc",b"[a-z]+"),(b"42",b"[0-9]+"),
                             (b"hello",b"^world$"),(b"[",b"\\["),(b"",b"")]:
            statuses=[lib.RegExpMatch(interp,text,pattern) for lib,interp in zip(libs,interps)]
            assert statuses[0]==statuses[1],(text,pattern,statuses)
        print(f"{count} external compressed streams, raw/zlib/gzip/auto, levels 0/1/6/9, integrity errors and regexp compile errors passed")
    finally:
        for lib,interp in zip(libs,interps):lib.DeleteInterp(interp)


if __name__=="__main__":main()
