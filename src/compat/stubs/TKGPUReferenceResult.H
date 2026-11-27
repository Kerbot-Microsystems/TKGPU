/* Saved results preserve the facade's owned object representation.
   The public state is opaque to clients; raw legacy string storage is not used. */
#ifndef TKGPU_RESULT_API
#define TKGPU_RESULT_API(name) Tcl_##name
#endif
void TKGPU_RESULT_API(SaveResult)(Tcl_Interp *i,Tcl_SavedResult *state) {
    Tcl_Obj *obj=Tcl_GetObjResult(i);
    if(!obj) Tcl_Panic("cannot save interpreter result");
    Tcl_IncrRefCount(obj);
    memset(state,0,sizeof *state);
    state->objResultPtr=obj;
    Tcl_ResetResult(i);
}
void TKGPU_RESULT_API(DiscardResult)(Tcl_SavedResult *state) {
    Tcl_Obj *obj=state->objResultPtr;
    memset(state,0,sizeof *state);
    if(obj) Tcl_DecrRefCount(obj);
}
void TKGPU_RESULT_API(RestoreResult)(Tcl_Interp *i,Tcl_SavedResult *state) {
    Tcl_Obj *obj=state->objResultPtr;
    if(!obj) Tcl_Panic("invalid saved interpreter result");
    Tcl_SetObjResult(i,obj);
    TKGPU_RESULT_API(DiscardResult)(state);
}
#undef TKGPU_RESULT_API
