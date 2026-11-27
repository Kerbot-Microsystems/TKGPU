import ctypes as c,random,sys
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);r.Tcl_FindExecutable(b'python')
F=c.CFUNCTYPE(None,c.c_void_p,c.c_int);P=c.c_void_p
for lib,prefix in [(r,'Tcl_'),(a,'candidate_')]:
    for n in ('CreateEventSource','DeleteEventSource'):
        f=getattr(lib,prefix+n);f.argtypes=[F,F,P];f.restype=None;setattr(lib,n,f)
r.Tcl_DoOneEvent.argtypes=[c.c_int];r.Tcl_DoOneEvent.restype=c.c_int
a.candidate_pass.argtypes=[c.c_int];a.candidate_pass.restype=c.c_int
logs=[[],[]]
def factory(j,tag):
    @F
    def cb(d,f):logs[j].append((tag,d,f))
    return cb
cb=[(factory(j,'s'),factory(j,'c')) for j in range(2)]
rng=random.Random(15);active=[]
for k in range(30000):
    op=rng.randrange(3);d=rng.randrange(1,30)
    if op==0 and len(active)<100:
        active.insert(0,d)
        for j,l in enumerate((r,a)):l.CreateEventSource(*cb[j],d)
    elif op==1:
        if d in active:active.remove(d)
        for j,l in enumerate((r,a)):l.DeleteEventSource(*cb[j],d)
    else:
        flags=rng.choice([2,18,34,10,6,14,26,62]);logs[0].clear();logs[1].clear()
        assert r.Tcl_DoOneEvent(flags)==a.candidate_pass(flags)
        assert logs[0]==logs[1],(k,flags,logs)
for d in active:r.DeleteEventSource(*cb[0],d)
a.candidate_reset()
class Time(c.Structure):_fields_=[('sec',c.c_long),('usec',c.c_long)]
a.candidate_SetMaxBlockTime.argtypes=[c.POINTER(Time)];a.candidate_wait.restype=c.c_int64
for k in range(1000):
    a.candidate_reset();values=[(rng.randrange(1000),rng.randrange(1000000)) for _ in range(20)]
    for s,u in values:a.candidate_SetMaxBlockTime(c.byref(Time(s,u)))
    assert a.candidate_wait()==min(s*1000+(u+999)//1000 for s,u in values)
print('30000 differential source operations; 20000 timeout inputs passed')
