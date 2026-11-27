/* Release-compatible entry points used by clients compiled with TCL_MEM_DEBUG.
   Source locations are accepted but this runtime has no allocation tracer. */
#ifndef TKGPU_DEBUG_API
#define TKGPU_DEBUG_API(name) Tcl_##name
#endif
#define LOCATION const char *file,int line
#define IGNORE_LOCATION (void)file; (void)line
char *TKGPU_DEBUG_API(DbCkalloc)(unsigned int n,LOCATION) { IGNORE_LOCATION; return Tcl_Alloc(n); }
void TKGPU_DEBUG_API(DbCkfree)(char *p,LOCATION) { IGNORE_LOCATION; Tcl_Free(p); }
char *TKGPU_DEBUG_API(DbCkrealloc)(char *p,unsigned int n,LOCATION) { IGNORE_LOCATION; return Tcl_Realloc(p,n); }
char *TKGPU_DEBUG_API(AttemptDbCkalloc)(unsigned int n,LOCATION) { IGNORE_LOCATION; return Tcl_AttemptAlloc(n); }
char *TKGPU_DEBUG_API(AttemptDbCkrealloc)(char *p,unsigned int n,LOCATION) { IGNORE_LOCATION; return Tcl_AttemptRealloc(p,n); }
void TKGPU_DEBUG_API(DbIncrRefCount)(Tcl_Obj *o,LOCATION) {
    IGNORE_LOCATION;
    if(!o||o->refCount<0||o->refCount==INT_MAX) Tcl_Panic("invalid object retain");
    ++o->refCount;
}
void TKGPU_DEBUG_API(DbDecrRefCount)(Tcl_Obj *o,LOCATION) {
    IGNORE_LOCATION;
    if(!o||o->refCount<0) Tcl_Panic("invalid object release");
    if(--o->refCount<=0) TclFreeObj(o);
}
int TKGPU_DEBUG_API(DbIsShared)(Tcl_Obj *o,LOCATION) { IGNORE_LOCATION; return o&&o->refCount>1; }
Tcl_Obj *TKGPU_DEBUG_API(DbNewBooleanObj)(int v,LOCATION) { IGNORE_LOCATION; return Tcl_NewBooleanObj(v); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewByteArrayObj)(const unsigned char *v,int n,LOCATION) { IGNORE_LOCATION; return Tcl_NewByteArrayObj(v,n); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewDoubleObj)(double v,LOCATION) { IGNORE_LOCATION; return Tcl_NewDoubleObj(v); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewListObj)(int n,Tcl_Obj *const *v,LOCATION) { IGNORE_LOCATION; return Tcl_NewListObj(n,v); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewLongObj)(long v,LOCATION) { IGNORE_LOCATION; return Tcl_NewLongObj(v); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewObj)(LOCATION) { IGNORE_LOCATION; return Tcl_NewObj(); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewStringObj)(const char *v,int n,LOCATION) { IGNORE_LOCATION; return Tcl_NewStringObj(v,n); }
Tcl_Obj *TKGPU_DEBUG_API(DbNewWideIntObj)(Tcl_WideInt v,LOCATION) { IGNORE_LOCATION; return Tcl_NewWideIntObj(v); }
#undef IGNORE_LOCATION
#undef LOCATION
#undef TKGPU_DEBUG_API
