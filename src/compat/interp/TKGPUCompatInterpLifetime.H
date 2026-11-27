/* Reference Stubs deletion must honor external preserves and active facade calls. */
#ifdef TKGPU_REFERENCE_STUBS
extern int Tcl_AsyncReady(void);
extern int Tcl_AsyncInvoke(Tcl_Interp *,int);
extern void Tcl_Preserve(void *);
extern void Tcl_Release(void *);
extern void Tcl_EventuallyFree(void *,void (*)(char *));
static void interp_final_free(char *data) { tg_destroy((Tcl_Interp *)data); }
void tg_compat_lifetime_enter(Tcl_Interp *i) { Tcl_Preserve(i); ++i->compat_lifetime_depth; }
void tg_compat_lifetime_leave(Tcl_Interp *i) { --i->compat_lifetime_depth; Tcl_Release(i); }
#endif
void Tcl_DeleteInterp(Tcl_Interp *i) {
    if(!i||i->deleted||i->destroying) return;
#ifdef TKGPU_REFERENCE_STUBS
    if(i->disposing||((i->depth||i->assoc_callback_depth)&&!i->compat_lifetime_depth)) return;
    i->deleted=1;
    Tcl_EventuallyFree(i,interp_final_free);
#else
    tg_destroy(i);
#endif
}
int Tcl_InterpDeleted(Tcl_Interp *i) { return i?i->deleted:0; }
int Tcl_Eval(Tcl_Interp *i,const char *script) {
    if(!i) return TG_ERROR;
    if(i->deleted) return tg_set_result(i,"attempt to call eval in deleted interpreter",TG_ERROR);
#ifdef TKGPU_REFERENCE_STUBS
    tg_compat_lifetime_enter(i);
    int rc=tg_eval(i,script);
    if(!i->deleted&&Tcl_AsyncReady()) rc=Tcl_AsyncInvoke(i,rc);
    tg_compat_lifetime_leave(i);
    return rc;
#else
    return tg_eval(i,script);
#endif
}
