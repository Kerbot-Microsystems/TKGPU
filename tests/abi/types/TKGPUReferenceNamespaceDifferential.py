import ctypes as c,random,sys
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);r.Tcl_FindExecutable(b'python');P=c.c_void_p;I=c.c_int;S=c.c_char_p
for n,args,ret in [('CreateInterp',[],P),('DeleteInterp',[P],None),('NewStringObj',[S,I],P),('GetString',[P],S),('GetStringResult',[P],S),('ResetResult',[P],None),('DbIncrRefCount',[P,S,I],None),('DbDecrRefCount',[P,S,I],None)]:
    f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
for l,pre in [(a,'candidate_'),(r,'Tcl_')]:
    for n,args,ret in [('GetGlobalNamespace',[P],P),('GetCurrentNamespace',[P],P),('FindNamespace',[P,S,P,I],P),('Export',[P,P,S,I],I),('AppendExportList',[P,P,P],I)]:
        f=getattr(l,pre+n);f.argtypes=args;f.restype=ret;setattr(l,n,f)
ips=[r.CreateInterp(),r.CreateInterp()];libs=(a,r);g=[l.GetGlobalNamespace(i) for l,i in zip(libs,ips)]
class Ns(c.Structure):_fields_=[('name',S),('full',S),('data',P),('delete',P),('parent',P)]
for l,i,p in zip(libs,ips,g):
    assert l.GetCurrentNamespace(i)==p
    n=Ns.from_address(p);assert (n.name,n.full,n.data,n.delete,n.parent)==(b'',b'::',None,None,None)
rng=random.Random(123);patterns=[b'',b'a',b'a b',b'*',b'?',b'::bad',b'x::y',b':',b'foo{bar',b'#x',b'\\x'];names=[b'',b':',b'::',b':::',b'::::',b'unknownxyz',b'::unknownxyz',b'a::b']
for k in range(25000):
    op=rng.randrange(3);pattern=rng.choice(patterns);name=rng.choice(names);flags=rng.choice([0,1,2,512,513,514]);reset=rng.randrange(4)==0;use_context=rng.randrange(2);out=[]
    for l,i,p in zip(libs,ips,g):
        r.ResetResult(i)
        if op==0:
            q=l.FindNamespace(i,name,p if use_context else None,flags);out.append((bool(q),q==p,r.GetStringResult(i)))
        else:
            rc=l.Export(i,p if use_context else None,pattern,reset) if op==1 else None
            o=r.NewStringObj(b'prefix',-1);r.DbIncrRefCount(o,b'test',1);rr=l.AppendExportList(i,p if use_context else None,o);out.append((rc,rr,r.GetStringResult(i),r.GetString(o)));r.DbDecrRefCount(o,b'test',1)
    assert out[0]==out[1],(k,op,pattern,name,flags,out)
a.candidate_clear()
for i in ips:r.DeleteInterp(i)
print('25000 namespace/export comparisons passed')
