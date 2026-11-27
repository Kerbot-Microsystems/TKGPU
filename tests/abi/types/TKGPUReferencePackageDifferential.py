import ctypes as c,random,sys
r=c.CDLL('libtcl8.6.so');a=c.CDLL(sys.argv[1]);P=c.c_void_p;I=c.c_int;S=c.c_char_p
r.Tcl_FindExecutable(b'python')
for n,args,ret in [('CreateInterp',[],P),('DeleteInterp',[P],None),('GetStringResult',[P],S),('ResetResult',[P],None)]:
 f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
for l,pre in [(a,'candidate_'),(r,'Tcl_')]:
 for n,args,ret in [('PkgProvideEx',[P,S,S,P],I),('PkgProvide',[P,S,S],I),('PkgPresentEx',[P,S,S,I,c.POINTER(P)],S),('PkgPresent',[P,S,S,I],S)]:
  f=getattr(l,pre+n);f.argtypes=args;f.restype=ret;setattr(l,n,f)
rng=random.Random(167)
versions=[b'0',b'1',b'01',b'1.0',b'1.0.0',b'1.1',b'1.2',b'1.02',b'1.2.0',b'1.2a0',b'1.2a3',b'1.2b1',b'1.2.0a1',b'2',b'2a1',b'2.1',b'3.0',b'1.1b0',b'1.0a2',b'1.0b1',b'2.0.0',b'0.1']
requests=versions+[None,b'',b'invalid',b'1.2-',b'1.2-2',b'1.2-1.2.0',b'0-1.2',b'1.0-3',b'1.2-2-3',b'-2',b'1.2a-2']
count=0
for batch in range(200):
 ips=[r.CreateInterp(),r.CreateInterp()]
 for k in range(250):
  op=rng.randrange(4);name=rng.choice([b'alpha',b'beta',b'gamma',b'delta',b'epsilon']);v=rng.choice(versions if op<2 else requests);data=rng.randrange(5);exact=rng.randrange(2);out=[]
  for l,i in zip((a,r),ips):
   r.ResetResult(i);d=P(999)
   if op==0:rc=l.PkgProvideEx(i,name,v,data)
   elif op==1:rc=l.PkgProvide(i,name,v)
   elif op==2:rc=l.PkgPresentEx(i,name,v,exact,c.byref(d))
   else:rc=l.PkgPresent(i,name,v,exact)
   out.append((rc,d.value,r.GetStringResult(i)))
  if out[0]!=out[1]:raise AssertionError((batch,k,op,name,v,exact,out))
  count+=1
 for i in ips:r.DeleteInterp(i)
print(count,'package registry operations matched')
