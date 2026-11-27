"""Run native link comparisons only in a private temporary directory."""
import ctypes as c
import os
import random
import sys
import tempfile
P=c.c_void_p
r=c.CDLL('libtcl8.6.so',use_errno=True)
a=c.CDLL(sys.argv[1],use_errno=True)
r.Tcl_FindExecutable(b'python')
for n,args,ret in [('NewStringObj',[c.c_char_p,c.c_int],P),('DbIncrRefCount',[P,c.c_char_p,c.c_int],None),('DbDecrRefCount',[P,c.c_char_p,c.c_int],None),('GetString',[P],c.c_char_p)]:
    f=getattr(r,'Tcl_'+n);f.argtypes=args;f.restype=ret;setattr(r,n,f)
for lib,prefix in [(a,'candidate_'),(r,'Tcl_')]:
    f=getattr(lib,prefix+'FSLink');f.argtypes=[P,P,c.c_int];f.restype=P;lib.fslink=f
class Obj(c.Structure):
    _fields_=[('refCount',c.c_int)]
def obj(s):
    if s is None:return None
    v=r.NewStringObj(os.fsencode(s),-1);r.DbIncrRefCount(v,b'test',1);return v
def rel(v):
    if v:r.DbDecrRefCount(v,b'test',1)
old=os.getcwd()
rng=random.Random(8572)
with tempfile.TemporaryDirectory() as d:
    try:
        os.chdir(d)
        with open('file','w') as source:source.write('hi')
        os.mkdir('dir');open('dir/f','w').close()
        os.symlink('file','existing');os.symlink('missing','broken')
        os.symlink('dir','dirlink');os.symlink('file','dir/sym')
        for k in range(20000):
            p=rng.choice(['out','dir/out','file','dir','existing','broken','missing/out','dirlink/out','dir/sym','missing'])
            t=rng.choice([None,'file','./file','dir/f','dir//f','f','../file','existing','broken','dir','missing','~','./dir/f'])
            flag=rng.choice([1,2,3])
            values=[]
            for lib in [a,r]:
                x,y=obj(p),obj(t);c.set_errno(0);z=lib.fslink(x,y,flag);e=c.get_errno()
                created=t is not None and p in ['out','dir/out','dirlink/out','missing'] and os.path.lexists(p)
                v=(r.GetString(z) if z else None,e if not z else None,z==y if z else None,Obj.from_address(z).refCount if z else None,os.readlink(p) if os.path.islink(p) else None)
                if created:os.unlink(p)
                if z and z!=y:rel(z)
                rel(x);rel(y);values.append(v)
            assert values[0]==values[1],(k,p,t,flag,values)
    finally:
        os.chdir(old)
print('20000 native link scenarios matched')
