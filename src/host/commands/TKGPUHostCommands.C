#include "TKGPUHost.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>
typedef struct {
    TKGPUHostCommand fn;
    TKGPUHostDispose dispose;
    void *context;
    unsigned active;
    int retired;
} host_command;

static void release(host_command *c) {
    if(c->dispose) c->dispose(c->context);
    free(c);
}
static void retire(void *data) {
    host_command *c=data;
    c->retired=1;
    if(!c->active) release(c);
}
/* Direct UTF-8 callback: no Tcl_Obj allocation/conversion round trip. */
static int invoke(void *data,tg_interp *i,size_t n,const char *const a[]) {
    host_command *c=data;
    c->active++; i->compat_callback_depth++;
    int rc=c->fn(c->context,i,n,a);
    i->compat_callback_depth--; c->active--;
    if(c->retired&&!c->active) {
        int previous=i->disposing;
        i->disposing=1; release(c); i->disposing=previous;
    }
    return rc;
}
int TKGPU_HostRegister(TKGPUHost *h,const char *name,TKGPUHostCommand fn,void *data,TKGPUHostDispose del) {
    if(!TKGPU_HostIsOwner(h)||!fn||!name||!*name||strlen(name)>=MAX_WORD||
       h->disposing||(h->depth&&!h->compat_callback_depth)) return TKGPU_HOST_ERROR;
    host_command *c=malloc(sizeof *c);
    if(!c) return TKGPU_HOST_ERROR;
    *c=(host_command){fn,del,data,0,0};
    size_t k=tg_command_lookup(h,name);
    if(k<h->command_count) {
        tg_registered_command *slot=&h->commands[k];
        tg_registered_command old=*slot;
        slot->fn=invoke; slot->context=c; slot->dispose=retire; slot->object_fn=NULL;
        h->disposing=1;
        if(old.dispose) old.dispose(old.context);
        h->disposing=0;
        return TKGPU_HOST_OK;
    }
    if(tg_command_register(h,name,invoke,c,retire)!=TG_OK) {
        free(c); return TKGPU_HOST_ERROR;
    }
    return TKGPU_HOST_OK;
}
int TKGPU_HostUnregister(TKGPUHost *h,const char *name) {
    return TKGPU_HostIsOwner(h)?tg_command_unregister(h,name):TKGPU_HOST_ERROR;
}
