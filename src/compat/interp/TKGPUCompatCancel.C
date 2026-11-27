#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>

typedef struct {
    int canceled;
    int unwind;
    Tcl_Obj *result;
} cancel_state;

static const char cancel_key[]="TKGPU::cancel-eval";

static void dispose_cancel(void *data,Tcl_Interp *i) {
    cancel_state *s=data;
    (void)i;
    if(s->result) TKGPU_ReleaseObject(s->result);
    free(s);
}

static cancel_state *get_cancel(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    cancel_state *s=Tcl_GetAssocData(i,cancel_key,NULL);
    if(!s&&create) {
        s=calloc(1,sizeof *s);
        if(!s) Tcl_Panic("cannot allocate cancel state");
        Tcl_SetAssocData(i,cancel_key,dispose_cancel,s);
    }
    return s;
}

static void clear_cancel(cancel_state *s) {
    if(!s) return;
    s->canceled=0;
    s->unwind=0;
    if(s->result) {
        TKGPU_ReleaseObject(s->result);
        s->result=NULL;
    }
}

static void apply_cancel_error(Tcl_Interp *i,cancel_state *s) {
    const char *msg="eval in progress";
    if(s->result) {
        const char *text=Tcl_GetString(s->result);
        if(text&&*text) msg=text;
        Tcl_SetObjResult(i,s->result);
    } else {
        tg_set_result(i,msg,TG_ERROR);
    }
    if(s->unwind) Tcl_SetErrorCode(i,"TCL","CANCEL","IUNWIND",msg,(char *)NULL);
    else Tcl_SetErrorCode(i,"TCL","CANCEL",msg,(char *)NULL);
}

int Tcl_CancelEval(Tcl_Interp *i,Tcl_Obj *resultObjPtr,void *clientData,int flags) {
    (void)clientData;
    if(!i||i->disposing||i->destroying) return TG_ERROR;
    if(i->depth==0) {
        if(resultObjPtr) {
            TKGPU_RetainObject(resultObjPtr);
            TKGPU_ReleaseObject(resultObjPtr);
        }
        clear_cancel(get_cancel(i,0));
        return TG_OK;
    }
    cancel_state *s=get_cancel(i,1);
    if(!s) return TG_ERROR;
    if(resultObjPtr) TKGPU_RetainObject(resultObjPtr);
    if(s->result) TKGPU_ReleaseObject(s->result);
    s->result=resultObjPtr;
    s->canceled=1;
    s->unwind=(flags&TKGPU_CANCEL_UNWIND)!=0;
    return TG_OK;
}

int Tcl_Canceled(Tcl_Interp *i,int flags) {
    cancel_state *s=get_cancel(i,0);
    if(!s||!s->canceled) return TG_OK;
    if(i->depth==0&&!s->unwind) {
        clear_cancel(s);
        return TG_OK;
    }
    if(flags&TKGPU_LEAVE_ERR_MSG) apply_cancel_error(i,s);
    return TG_ERROR;
}

int tg_compat_cancel_check(Tcl_Interp *i) {
    cancel_state *s=get_cancel(i,0);
    if(!s||!s->canceled) return TG_OK;
    if(i->depth==0) {
        clear_cancel(s);
        return TG_OK;
    }
    apply_cancel_error(i,s);
    return TG_ERROR;
}
