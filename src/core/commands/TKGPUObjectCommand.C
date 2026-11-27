#include "../TKGPUCoreInternal.H"
#include <string.h>
int tg_command_try_objects(tg_interp *i,size_t n,tg_object *const v[]) {
    if(i->depth>=i->recursion_limit) return tg_set_result(i,"evaluation depth exceeded",TG_ERROR);
    size_t len; const char *name=tg_object_bytes(v[0],&len);
    if(!name||memchr(name,0,len)) return -1;
    size_t k=tg_command_lookup(i,name);
    if(k<i->command_count) {
        tg_registered_command *c=&i->commands[k];
        if(!c->object_fn) return -1;
        if(i->eval_check&&i->eval_check(i)!=TG_OK) return TG_ERROR;
        /* A limit handler may delete or reallocate the command registry. */
        k=tg_command_lookup(i,name);
        if(k==i->command_count||!i->commands[k].object_fn)
            return tg_set_result(i,"command removed during limit callback",TG_ERROR);
        c=&i->commands[k];
        size_t held=0;
        for(;held<n;held++) if(!tg_object_retain(v[held])) break;
        if(held!=n) {
            while(held) tg_object_release(v[--held]);
            return tg_set_result(i,"invalid command object",TG_ERROR);
        }
        tg_set_result(i,"",TG_OK); i->depth++;
        int rc=c->object_fn(c->context,i,n,v);
        i->depth--;
        /* The caller's vector may hold zero-ref Tcl objects. A temporary
           reference is released normally, matching Tcl evaluation ownership. */
        while(held) tg_object_release(v[--held]);
        if(rc!=TG_OK&&rc!=TG_ERROR&&rc!=TG_BREAK&&rc!=TG_CONTINUE) return tg_set_result(i,"unsupported callback status",TG_ERROR);
        return rc;
    }
    return -1;
}
