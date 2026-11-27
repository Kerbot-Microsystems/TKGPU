/* Save bounded result/error metadata without rolling back ordinary variables. */
#ifndef STATE_API
#define STATE_API(n) Tcl_##n
#endif
struct Tcl_InterpState_ {
    int status;
    Tcl_Obj *result,*error_info,*error_code;
};
static Tcl_Obj *state_hold(Tcl_Obj *o) { if(o) Tcl_IncrRefCount(o); return o; }
static void state_drop(Tcl_Obj *o) { if(o) Tcl_DecrRefCount(o); }
Tcl_InterpState STATE_API(SaveInterpState)(Tcl_Interp *i,int status) {
    if(!i) return NULL;
    Tcl_InterpState s=calloc(1,sizeof *s);
    if(!s) Tcl_Panic("interpreter state allocation failed");
    s->status=status; s->result=state_hold(Tcl_GetObjResult(i));
    s->error_info=state_hold(STATE_GET_ERROR(i,"errorInfo"));
    s->error_code=state_hold(STATE_GET_ERROR(i,"errorCode"));
    return s;
}
void STATE_API(DiscardInterpState)(Tcl_InterpState s) {
    if(!s) return;
    state_drop(s->result);state_drop(s->error_info);state_drop(s->error_code);free(s);
}
int STATE_API(RestoreInterpState)(Tcl_Interp *i,Tcl_InterpState s) {
    if(!s) return 1;
    int status=s->status;
    if(!i) {STATE_API(DiscardInterpState)(s);return 1;}
    STATE_RESTORE_ERROR(i,"errorInfo",s->error_info);
    STATE_RESTORE_ERROR(i,"errorCode",s->error_code);
    Tcl_SetObjResult(i,s->result);
    STATE_API(DiscardInterpState)(s);return status;
}
#undef STATE_API
