/* Built as a separate shared binary using only public headers. */
#include "TKGPUCompat.H"
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
static int freed;
static Tcl_ObjType type;
static TKGPUAbiObjectPrefix *prefix(Tcl_Obj *o) { return (TKGPUAbiObjectPrefix *)(void *)o; }
static void dispose(Tcl_Obj *o) {
    Tcl_Free(prefix(o)->internalRep.pointer); freed++;
}
static void duplicate(Tcl_Obj *src,Tcl_Obj *dst) {
    char *data=Tcl_AttemptAlloc(1);
    if(!data) return;
    *data=*(char *)prefix(src)->internalRep.pointer;
    prefix(dst)->internalRep.pointer=data; prefix(dst)->typePtr=&type;
}
static int convert(Tcl_Interp *i,Tcl_Obj *o) {
    (void)i;
    const char *s=Tcl_GetString(o);
    if(!s||strlen(s)!=2||s[0]!='4') return 1;
    char *data=Tcl_AttemptAlloc(1); if(!data) return 1;
    *data=s[1];
    TKGPUAbiObjectPrefix *p=prefix(o);
    if(p->typePtr&&p->typePtr->freeIntRepProc) p->typePtr->freeIntRepProc(o);
    p->internalRep.pointer=data; p->typePtr=&type;
    return 0;
}
static void update(Tcl_Obj *o) {
    TKGPUAbiObjectPrefix *p=prefix(o);
    char *s=Tcl_AttemptAlloc(3); if(!s) return;
    s[0]='4'; s[1]=*(char *)p->internalRep.pointer; s[2]=0;
    p->bytes=s; p->length=2;
}
static Tcl_ObjType type={"tkgpu-test-external",dispose,duplicate,update,convert};
EXPORT void tkgpu_test_external_change(Tcl_Obj *o,char value) {
    *(char *)prefix(o)->internalRep.pointer=value; Tcl_InvalidateStringRep(o);
}
EXPORT const Tcl_ObjType *tkgpu_test_external_register(void) { Tcl_RegisterObjType(&type); return &type; }
EXPORT int tkgpu_test_external_freed(void) { return freed; }
