import ctypes as c,random,sys
P=c.c_void_p;I=c.c_int;S=c.c_char_p
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);r.Tcl_FindExecutable.argtypes=[S];r.Tcl_FindExecutable(b'python')
for n,args,ret in [('CreateInterp',[],P),('DeleteInterp',[P],None),('NewStringObj',[S,I],P),('GetString',[P],S),('GetStringResult',[P],S),('Eval',[P,S],I),('SetObjResult',[P,P],None),('GetObjResult',[P],P),('GetErrorLine',[P],I),('SetErrorLine',[P,I],None),('DbIncrRefCount',[P,S,I],None),('DbDecrRefCount',[P,S,I],None),('GetVar',[P,S,I],S)]:
    f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
for l,p in [(a,'candidate_'),(r,'Tcl_')]:
    for n,args,ret in [('SaveInterpState',[P,I],P),('RestoreInterpState',[P,P],I),('DiscardInterpState',[P],None),('SetChannelErrorInterp',[P,P],None),('GetChannelErrorInterp',[P,c.POINTER(P)],None)]:
        f=getattr(l,p+n);f.argtypes=args;f.restype=ret;setattr(l,n,f)
def hold(x):r.DbIncrRefCount(x,b'test',1);return x
def drop(x):r.DbDecrRefCount(x,b'test',1)
rng=random.Random(5);ips=[r.CreateInterp(),r.CreateInterp()]
objs=[hold(r.NewStringObj(f'msg {j}'.encode(),-1)) for j in range(32)]
for k in range(20000):
    j=rng.randrange(33);op=rng.randrange(3);out=[]
    for l,i in zip((a,r),ips):
        if op==0:l.SetChannelErrorInterp(i,objs[j] if j<32 else None)
        else:
            v=P();l.GetChannelErrorInterp(i,c.byref(v));out.append(v.value)
            if v.value:drop(v)
    assert len(out)!=2 or out[0]==out[1],(k,out)
for l,i in zip((a,r),ips):l.SetChannelErrorInterp(i,None)
assert all(I.from_address(o).value==1 for o in objs)
for o in objs:drop(o)
print('20000 channel error operations matched')
diagnostic_differences=0
for k in range(10000):
    status=rng.randrange(-5,8);discard=rng.randrange(2);out=[]
    for l,i in zip((a,r),ips):
        r.Eval(i,b'error original');r.SetErrorLine(i,8);s=l.SaveInterpState(i,status);before=r.GetStringResult(i)
        r.Eval(i,b'error changed');r.SetErrorLine(i,15)
        if discard:l.DiscardInterpState(s);rc=None
        else:rc=l.RestoreInterpState(i,s)
        out.append((before,rc,r.GetStringResult(i),r.GetErrorLine(i),r.GetVar(i,b'errorInfo',0),r.GetVar(i,b'errorCode',0)))
    assert out[0][:4]==out[1][:4],(k,out)
    diagnostic_differences+=out[0][4:]!=out[1][4:]
print('10000 state result/status/error-line operations matched')
print('Unqualified lazy diagnostic variable differences:',diagnostic_differences)
for i in ips:r.DeleteInterp(i)
