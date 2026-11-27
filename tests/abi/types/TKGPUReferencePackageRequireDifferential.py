from pathlib import Path
exec(Path(__file__).with_name('TKGPUReferencePackageDifferential.py').read_text().split('count=0')[0])
for l,pre in [(a,'candidate_'),(r,'Tcl_')]:
 for n,args,ret in [('PkgRequireEx',[P,S,S,I,c.POINTER(P)],S),('PkgRequire',[P,S,S,I],S),('PkgRequireProc',[P,S,I,c.POINTER(P),c.POINTER(P)],I)]:
  f=getattr(l,pre+n);f.argtypes=args;f.restype=ret;setattr(l,n,f)
for n,args,ret in [('NewStringObj',[S,I],P),('DbIncrRefCount',[P,S,I],None),('DbDecrRefCount',[P,S,I],None)]:
 f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
count=0
for batch in range(100):
 ips=[r.CreateInterp(),r.CreateInterp()]
 for k in range(300):
  op=rng.randrange(5);name=rng.choice([b'alpha',b'beta',b'gamma',b'delta',b'epsilon']);v=rng.choice(versions if op<2 else requests);data=rng.randrange(5);exact=rng.randrange(2);out=[]
  vv=[rng.choice(requests[0:len(versions)]+requests[len(versions)+1:]) for _ in range(rng.randrange(5))] if op==4 else []
  args=[r.NewStringObj(s,-1) for s in vv]
  for o in args:r.DbIncrRefCount(o,b't',1)
  for l,i in zip((a,r),ips):
   r.ResetResult(i);d=P(999)
   if op==0:rc=l.PkgProvideEx(i,name,v,data)
   elif op==1:rc=l.PkgProvide(i,name,v)
   elif op==2:rc=l.PkgRequireEx(i,name,v,exact,c.byref(d))
   elif op==3:rc=l.PkgRequire(i,name,v,exact)
   else:rc=l.PkgRequireProc(i,name,len(args),(P*len(args))(*args),c.byref(d))
   out.append((rc,d.value,r.GetStringResult(i)))
  for o in args:r.DbDecrRefCount(o,b't',1)
  if out[0]!=out[1]:raise AssertionError((batch,k,op,name,v,exact,vv,out))
  count+=1
 for i in ips:r.DeleteInterp(i)
print(count,'require and package operations matched')
