import ctypes as c,random,threading,sys
P=c.c_void_p; I=c.c_int; F=c.CFUNCTYPE(I,P,P,I)
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);r.Tcl_FindExecutable(b'python')
r.Tcl_CreateInterp.restype=P;r.Tcl_DeleteInterp.argtypes=[P]
for lib,p in [(a,'candidate_'),(r,'Tcl_')]:
    for n,args,ret in [('AsyncCreate',[F,P],P),('AsyncDelete',[P],None),('AsyncMark',[P],None),('AsyncReady',[],I),('AsyncInvoke',[P,I],I)]:
        f=getattr(lib,p+n);f.argtypes=args;f.restype=ret;setattr(lib,n,f)
errors=[]
def worker(seed):
    ip=r.Tcl_CreateInterp();rng=random.Random(seed);handles=[{},{}];logs=[[],[]];pending=[{},{}]
    def factory(j,lib):
        @F
        def cb(data,interp,code):
            logs[j].append((data,bool(interp),code,lib.AsyncReady()))
            if data in pending[j]:
                op,k=pending[j].pop(data)
                if k in handles[j]:
                    if op==0:lib.AsyncMark(handles[j][k])
                    else:lib.AsyncDelete(handles[j].pop(k))
            return (code+data)%17
        return cb
    callbacks=[factory(j,lib) for j,lib in enumerate((a,r))];libs=(a,r)
    try:
        for k in range(1,9):
            for j,l in enumerate(libs):handles[j][k]=l.AsyncCreate(callbacks[j],k)
        for step in range(5000):
            op=rng.randrange(6);k=rng.randrange(1,9);target=rng.randrange(1,9);action=rng.randrange(2);code=rng.randrange(17);inp=ip if rng.randrange(2) else None
            out=[]
            for j,l in enumerate(libs):
                if op==0 and k not in handles[j]:handles[j][k]=l.AsyncCreate(callbacks[j],k)
                elif op==1 and k in handles[j]:l.AsyncDelete(handles[j].pop(k));pending[j].pop(k,None)
                elif op==2 and k in handles[j]:l.AsyncMark(handles[j][k])
                elif op==3 and k in handles[j]:pending[j][k]=(action,target);l.AsyncMark(handles[j][k])
                elif op==4:out.append(l.AsyncInvoke(inp,code))
            assert len(out)!=2 or out[0]==out[1],(step,op,out)
            assert a.AsyncReady()==r.AsyncReady() and logs[0]==logs[1] and set(handles[0])==set(handles[1]),(step,op,logs)
            logs[0].clear();logs[1].clear()
    except BaseException as e:errors.append(repr(e))
    finally:
        for j,l in enumerate(libs):
            for h in handles[j].values():l.AsyncDelete(h)
        r.Tcl_DeleteInterp(ip)
threads=[threading.Thread(target=worker,args=(n,)) for n in range(4)]
for t in threads:t.start()
for t in threads:t.join()
assert not errors,errors
print('20000 async handler scenarios matched on four threads')
