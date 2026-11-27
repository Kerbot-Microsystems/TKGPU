"""Native attribute differential, restricted to private temporary files."""
import ctypes as c
import ctypes.util
import grp
import os
import pwd
import random
import sys
import tempfile

P = c.c_void_p
def main():
    ref = c.CDLL(ctypes.util.find_library("tcl8.6"), use_errno=True)
    candidate = c.CDLL(sys.argv[1], use_errno=True)
    ref.Tcl_FindExecutable.argtypes = [c.c_char_p]
    ref.Tcl_FindExecutable(b"python")
    for name, args, result in [
        ("CreateInterp", [], P), ("DeleteInterp", [P], None),
        ("NewStringObj", [c.c_char_p, c.c_int], P),
        ("GetString", [P], c.c_char_p), ("GetStringResult", [P], c.c_char_p),
        ("ResetResult", [P], None), ("DbIncrRefCount", [P,c.c_char_p,c.c_int],None),
        ("DbDecrRefCount", [P,c.c_char_p,c.c_int],None)]:
        fn=getattr(ref,"Tcl_"+name);fn.argtypes=args;fn.restype=result
        setattr(ref,name,fn)
    for lib,prefix in [(candidate,"candidate_"),(ref,"Tcl_")]:
        for name,args,result in [
            ("FSFileAttrStrings",[P,c.POINTER(P)],c.POINTER(c.c_char_p)),
            ("FSFileAttrsGet",[P,c.c_int,P,c.POINTER(P)],c.c_int),
            ("FSFileAttrsSet",[P,c.c_int,P,P],c.c_int)]:
            fn=getattr(lib,prefix+name);fn.argtypes=args;fn.restype=result
            setattr(lib,name,fn)
    def obj(s):
        b=os.fsencode(s);o=ref.NewStringObj(b,len(b))
        ref.DbIncrRefCount(o,b"test",1);return o
    def release(o):ref.DbDecrRefCount(o,b"test",1)
    ip=ref.CreateInterp()
    rng=random.Random(321)
    with tempfile.TemporaryDirectory() as d:
        paths=[d,d+"/f",d+"/fifo",d+"/link",d+"/bad",d+"/missing",""]
        open(paths[1],"w").close();os.mkfifo(paths[2])
        os.symlink("f",paths[3]);os.symlink("missing",paths[4])
        count=0
        for k in range(5000):
            os.chmod(paths[1],rng.randrange(4096));os.chmod(paths[2],rng.randrange(4096))
            p=rng.choice(paths);o=obj(p)
            x=P(123);y=P(123)
            a=candidate.FSFileAttrStrings(o,c.byref(x));b=ref.FSFileAttrStrings(o,c.byref(y))
            assert bool(a)==bool(b) and x.value==y.value
            count+=1
            if a:
                assert [a[j] for j in range(4)]==[b[j] for j in range(4)]
                for j in range(3):
                    res=[]
                    for lib in [candidate,ref]:
                        ref.ResetResult(ip);v=P()
                        rc=lib.FSFileAttrsGet(ip,j,o,c.byref(v))
                        res.append((rc,ref.GetString(v) if v.value else None,ref.GetStringResult(ip)))
                        if v.value:release(v)
                    assert res[0]==res[1],(p,j,res)
                    count+=1
            release(o)
        p=paths[1];o=obj(p);rng=random.Random(431)
        for k in range(10000):
            mode=rng.randrange(4096)
            val=rng.choice([str(rng.randrange(-4096,4096)),oct(rng.randrange(4096)),
                "0"+format(rng.randrange(4096),"o"),
                ",".join("".join(rng.choices("ugoa",k=rng.randrange(4)))+rng.choice("+-=")+"".join(rng.choices("rwxst",k=rng.randrange(6))) for _ in range(rng.randrange(1,5))),
                rng.choice(["u+r,","bad","u","","rwsr-Sr-t","rwxrwxrwT","u+x g+x","u+x,,"])])
            v=obj(val);r=[]
            for lib in [candidate,ref]:
                os.chmod(p,mode);ref.ResetResult(ip);rc=lib.FSFileAttrsSet(ip,2,o,v)
                r.append((rc,os.stat(p).st_mode&0o7777,ref.GetStringResult(ip)))
            assert r[0]==r[1],(mode,val,r)
            release(v);count+=1
        rng=random.Random(34)
        for k in range(2000):
            idx=rng.randrange(2);num=os.getuid() if idx else os.getgid()
            name=pwd.getpwuid(num).pw_name if idx else grp.getgrgid(num).gr_name
            v=obj(rng.choice([str(num),name,"tkgpu-missing-xyz-347","-1"]));r=[]
            for lib in [candidate,ref]:
                ref.ResetResult(ip);rc=lib.FSFileAttrsSet(ip,idx,o,v);s=os.stat(p)
                r.append((rc,s.st_uid,s.st_gid,ref.GetStringResult(ip)))
            assert r[0]==r[1],r
            release(v);count+=1
        release(o)
    ref.DeleteInterp(ip)
    print(count,"native attribute comparisons passed")
if __name__=="__main__":main()
