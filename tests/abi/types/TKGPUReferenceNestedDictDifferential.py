"""Nested paths against stock Tcl using the isolated dictionary harness."""
import ctypes as c, random, sys
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);P=c.c_void_p;I=c.c_int
r.Tcl_FindExecutable(b'python')
for n,args,ret in [('CreateInterp',[],P),('DeleteInterp',[P],None),('NewStringObj',[c.c_char_p,I],P),('GetString',[P],c.c_char_p),('GetStringResult',[P],c.c_char_p),('ResetResult',[P],None),('DbIncrRefCount',[P,c.c_char_p,I],None),('DbDecrRefCount',[P,c.c_char_p,I],None)]:
    f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
for l,p in [(a,'candidate_'),(r,'Tcl_')]:
    for n,args,ret in [('DictObjPutKeyList',[P,P,I,c.POINTER(P),P],I),('DictObjRemoveKeyList',[P,P,I,c.POINTER(P)],I)]:
        f=getattr(l,p+n);f.argtypes=args;f.restype=ret;setattr(l,n,f)
def obj(s):
    o=r.NewStringObj(s,-1);r.DbIncrRefCount(o,b't',1);return o
def drop(o):r.DbDecrRefCount(o,b't',1)
ip=r.CreateInterp();rng=random.Random(82)
count=0
for j in range(1000):
    init=rng.choice([b'',b'a 1',b'a {b 2 c 3}',b'a {b {c 1}}',b'a {} b {a 1}',b'a 1 a 2'])
    d=[obj(init),obj(init)]
    for k in range(30):
        keys=[obj(rng.choice([b'a',b'b',b'c',b'x',b''])) for _ in range(rng.randrange(1,6))];v=obj(rng.choice([b'1',b'foo bar',b'',b'x y',b'{bad']))
        kv=(P*len(keys))(*keys);put=rng.randrange(2);out=[]
        for l,o in zip((a,r),d):
            r.ResetResult(ip);rc=l.DictObjPutKeyList(ip,o,len(keys),kv,v) if put else l.DictObjRemoveKeyList(ip,o,len(keys),kv)
            out.append((rc,r.GetString(o),r.GetStringResult(ip)))
        assert out[0][:2]==out[1][:2],(j,k,init,put,out)
        for o in keys+[v]:drop(o)
        count+=1
    for o in d:drop(o)
r.DeleteInterp(ip);print(count,'nested operations matched')
