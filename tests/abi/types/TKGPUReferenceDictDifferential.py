"""Compare independent dictionary representation using stock Tcl object primitives."""
import ctypes as c
import random
import sys
P=c.c_void_p; I=c.c_int
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);r.Tcl_FindExecutable(b'python')
class Search(c.Structure):_fields_=[('next',P),('epoch',I),('dict',P)]
for lib,pre in [(a,'candidate_'),(r,'Tcl_')]:
    for n,args,ret in [('NewDictObj',[],P),('DbNewDictObj',[c.c_char_p,I],P),('DictObjPut',[P,P,P,P],I),('DictObjGet',[P,P,P,c.POINTER(P)],I),('DictObjRemove',[P,P,P],I),('DictObjSize',[P,P,c.POINTER(I)],I),('DictObjFirst',[P,P,c.POINTER(Search),c.POINTER(P),c.POINTER(P),c.POINTER(I)],I),('DictObjNext',[c.POINTER(Search),c.POINTER(P),c.POINTER(P),c.POINTER(I)],None),('DictObjDone',[c.POINTER(Search)],None)]:
        f=getattr(lib,pre+n);f.argtypes=args;f.restype=ret;setattr(lib,n,f)
for n,args,ret in [('NewStringObj',[c.c_char_p,I],P),('GetString',[P],c.c_char_p),('DbIncrRefCount',[P,c.c_char_p,I],None),('DbDecrRefCount',[P,c.c_char_p,I],None),('DuplicateObj',[P],P)]:
    f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
def retain(x):r.DbIncrRefCount(x,b't',1);return x
def drop(x):r.DbDecrRefCount(x,b't',1)
def obj(s):return retain(r.NewStringObj(s,len(s)))
def scan(lib,o):
    s=Search();k=P();v=P();done=I()
    assert not lib.DictObjFirst(None,o,c.byref(s),c.byref(k),c.byref(v),c.byref(done));out=[]
    while not done.value:
        out.append((r.GetString(k),r.GetString(v)));lib.DictObjNext(c.byref(s),c.byref(k),c.byref(v),c.byref(done))
    lib.DictObjDone(c.byref(s));return out
rng=random.Random(42);keys=[b'',b'hello',b'a b',b'#tag',b'{a}',b'\\',b'x\ny',b'1',b'2',b'3',b'4',b'5'];vals=keys+[b'v',b'abc',b'xyz']
count=0
for j in range(1000):
    ds=[retain(lib.NewDictObj() if j%2 else lib.DbNewDictObj(b't',1)) for lib in (a,r)]
    for k in range(50):
        key=obj(rng.choice(keys));value=obj(rng.choice(vals));op=rng.randrange(5);out=[]
        for lib,d in zip((a,r),ds):
            if op==0:out.append(lib.DictObjPut(None,d,key,value))
            elif op==1:out.append(lib.DictObjRemove(None,d,key))
            elif op==2:
                v=P();rc=lib.DictObjGet(None,d,key,c.byref(v));out.append((rc,r.GetString(v) if v.value else None))
            elif op==3:
                n=I();rc=lib.DictObjSize(None,d,c.byref(n));out.append((rc,n.value))
            else:out.append(scan(lib,d))
        assert out[0]==out[1],(j,k,op,out)
        assert r.GetString(ds[0])==r.GetString(ds[1]),(j,k,r.GetString(ds[0]),r.GetString(ds[1]))
        assert scan(a,ds[0])==scan(r,ds[1])
        drop(key);drop(value);count+=1
    for lib,d in zip((a,r),ds):
        dup=retain(r.DuplicateObj(d));assert scan(lib,dup)==scan(lib,d);drop(dup);drop(d)
print(count,'dictionary operations matched; 2000 duplicate checks')
for val in [b'a 1 b 2 a 3',b'',b'{a b} {c d}',b'a',b'{']:
    ds=[obj(val),obj(val)];out=[]
    for lib,d in zip((a,r),ds):
        n=I();rc=lib.DictObjSize(None,d,c.byref(n));out.append((rc,n.value,scan(lib,d) if not rc else []));drop(d)
    assert out[0]==out[1],(val,out)
print('conversion checks matched')
