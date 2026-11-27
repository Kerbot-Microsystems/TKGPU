#include "TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>

void tg_legacy_result_release(tg_interp *i) {
    char *old=i->legacy_result;
    void (*dispose)(char *)=i->legacy_free;
    i->legacy_result=i->result; i->legacy_free=NULL;
    if(old&&dispose) {
        ++i->assoc_callback_depth;
        if((uintptr_t)dispose==3u) free(old);
        else if((uintptr_t)dispose!=1u) dispose(old);
        --i->assoc_callback_depth;
    }
}
int tg_set_result(tg_interp *i,const char *s,int code) {
    i->result_kind=TG_RESULT_STRING;
    size_t n=strlen(s);
    if(n>=sizeof(i->result)) { strcpy(i->result,"result too long"); code=TG_ERROR; }
    else memmove(i->result,s,n+1);
    /* Copy before releasing: s may alias a legacy string or result object. */
    tg_legacy_result_release(i);
    if(i->compat_result) {
        tg_object *old=i->compat_result; i->compat_result=NULL; tg_object_release(old);
    }
    i->compat_legacy_lookup=0;
    return code;
}
int tg_set_result_kind(tg_interp *i,const char *s,int kind) {
    int rc=tg_set_result(i,s,TG_OK);
    if(rc==TG_OK) i->result_kind=kind;
    return rc;
}
char *tg_copy_string(const char *s) {
    size_t n=strlen(s)+1; char *p=malloc(n); if(p) memcpy(p,s,n); return p;
}
tg_interp *tg_create(void) {
    tg_interp *i=calloc(1,sizeof(*i));
    if(i && !(i->ui=tg_ui_create())) { free(i); return NULL; }
    if(i) { i->legacy_result=i->result; i->recursion_limit=MAX_DEPTH; }
    return i;
}
tg_ui *tg_interp_ui(tg_interp *i) { return i?i->ui:NULL; }
/* Independent per-interpreter extension state. Detach each entry before
   calling user code so a callback can remove another entry without double free. */
struct tg_assoc {
    struct tg_assoc *next;
    char *name;
    tg_interp_delete_proc *proc;
    void *data;
};
struct tg_delete_hook {
    struct tg_delete_hook *next;
    tg_interp_delete_proc *proc;
    void *data;
};
static struct tg_assoc **assoc_find(tg_interp *i,const char *name) {
    struct tg_assoc **p=&i->assoc;
    while(*p&&strcmp((*p)->name,name)) p=&(*p)->next;
    return p;
}
int tg_assoc_set(tg_interp *i,const char *name,tg_interp_delete_proc *proc,void *data) {
    if(!i||!name||i->disposing) return TG_ERROR;
    struct tg_assoc **p=assoc_find(i,name);
    if(*p) { (*p)->proc=proc; (*p)->data=data; return TG_OK; }
    struct tg_assoc *a=malloc(sizeof *a);
    if(!a) return TG_ERROR;
    a->name=tg_copy_string(name);
    if(!a->name) { free(a); return TG_ERROR; }
    a->proc=proc; a->data=data; a->next=i->assoc; i->assoc=a;
    return TG_OK;
}
void *tg_assoc_get(tg_interp *i,const char *name,tg_interp_delete_proc **proc) {
    if(!i||!name) return NULL;
    struct tg_assoc *a=*assoc_find(i,name);
    if(!a) return NULL;
    if(proc) *proc=a->proc;
    return a->data;
}
static void assoc_callback(tg_interp *i,tg_interp_delete_proc *proc,void *data) {
    if(proc) {
        i->assoc_callback_depth++;
        proc(data,i);
        i->assoc_callback_depth--;
    }
}
void tg_assoc_delete(tg_interp *i,const char *name) {
    if(!i||!name) return;
    struct tg_assoc **p=assoc_find(i,name),*a=*p;
    if(!a) return;
    *p=a->next;
    tg_interp_delete_proc *proc=a->proc; void *data=a->data;
    free(a->name); free(a);
    assoc_callback(i,proc,data);
}
int tg_delete_hook_add(tg_interp *i,tg_interp_delete_proc *proc,void *data) {
    if(!i||i->disposing||!proc) return TG_ERROR;
    struct tg_delete_hook *h=malloc(sizeof *h);
    if(!h) return TG_ERROR;
    h->proc=proc; h->data=data; h->next=i->delete_hooks; i->delete_hooks=h;
    return TG_OK;
}
void tg_delete_hook_remove(tg_interp *i,tg_interp_delete_proc *proc,void *data) {
    if(!i) return;
    struct tg_delete_hook **p=&i->delete_hooks;
    while(*p) {
        struct tg_delete_hook *h=*p;
        if(h->proc==proc&&h->data==data) { *p=h->next; free(h); return; }
        p=&h->next;
    }
}
static void extension_clear(tg_interp *i) {
    while(i->delete_hooks) {
        struct tg_delete_hook *h=i->delete_hooks;
        i->delete_hooks=h->next;
        tg_interp_delete_proc *proc=h->proc; void *data=h->data;
        free(h); assoc_callback(i,proc,data);
    }
    while(i->assoc) tg_assoc_delete(i,i->assoc->name);
}
void tg_destroy(tg_interp *i) {
    if(!i||i->depth||i->disposing||i->assoc_callback_depth||i->destroying) return;
    i->deleted=1; i->destroying=1;
    if(i->event_dispose) i->event_dispose(i);
    if(i->tk_dispose) i->tk_dispose(i->tk_host);
    tg_binding_clear(i); tg_trace_clear(i);
    tg_command_clear(i);
    extension_clear(i);
    tg_history_destroy(i->history);
    for(size_t k=0;k<i->count;k++) { free(i->vars[k].key); free(i->vars[k].value); tg_object_release(i->vars[k].object); }
    tg_legacy_result_release(i);
    tg_object_release(i->compat_result);
    tg_object_release(i->compat_variable);
    tg_object_release(i->compat_channel_error);
    tg_object_release(i->compat_namespace_exports);
    free(i->compat_global_namespace);
    free(i->vars); free(i->commands);
    free(i->ime_pending); free(i->scratch); tg_ui_destroy(i->ui); free(i);
}
const char *tg_result(const tg_interp *i) {
    if(!i) return "null interpreter";
    /* The stock stub bootstrap writes a static diagnostic on refusal. */
    return i->legacy_result?i->legacy_result:i->result;
}
int tg_eval(tg_interp *i,const char *script) {
    if(i&&i->deleted) return tg_set_result(i,"attempt to call eval in deleted interpreter",TG_ERROR);
    if(!i||!script||i->depth>=i->recursion_limit||(i->depth&&!i->compat_callback_depth)||i->disposing) return TG_ERROR;
    /* The caller may pass the borrowed result from a preceding evaluation. */
    char *owned=tg_copy_string(script);
    if(!owned) return tg_set_result(i,"out of memory",TG_ERROR);
    const char *p=owned;
    int rc=tg_eval_script(i,&p,0);
    free(owned);
    return rc;
}
