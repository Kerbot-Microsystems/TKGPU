#include "TKGPUCompat.H"
#include "../../src/object/TKGPUObjectInternal.H"
#include <stddef.h>
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define OFFSET(f) CHECK(offsetof(struct tg_object,f)==offsetof(TKGPUAbiObjectPrefix,f))
int main(void) {
    OFFSET(refCount); OFFSET(bytes); OFFSET(length); OFFSET(typePtr); OFFSET(internalRep);
    CHECK(offsetof(struct tg_object,binary)>=sizeof(TKGPUAbiObjectPrefix));
    Tcl_Obj *o=Tcl_NewStringObj("hello",-1); CHECK(o);
    CHECK(o->refCount==0&&o->length==5&&o->bytes&&o->typePtr==NULL);
    o->refCount++;
    CHECK(TKGPU_RetainObject(o)==o&&o->refCount==2);
    TKGPU_ReleaseObject(o); CHECK(o->refCount==1);
    if(o->refCount--<=1) TclFreeObj(o);
    o=Tcl_NewIntObj(42); CHECK(o&&o->refCount==0);
    if(o->refCount--<=1) TclFreeObj(o);
    o=Tcl_NewWideIntObj(42); CHECK(o&&o->refCount==0);
    if(o->refCount--<=1) TclFreeObj(o);
    puts("object prefix and non-debug refcount checks passed"); return 0;
}
