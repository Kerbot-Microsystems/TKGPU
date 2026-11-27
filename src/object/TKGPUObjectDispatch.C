#include "TKGPUObject.H"
#include "../core/TKGPUCoreInternal.H"
#include <string.h>
int tg_call_objects(tg_interp *i,size_t count,tg_object *const objects[]) {
    if(i&&i->disposing) {
        /* Allow Python Variable.__del__ ("info exists <var>") to complete quietly during teardown. */
        if(count==3&&objects&&objects[0]&&objects[1]) {
            const char *c0=tg_object_bytes(objects[0],NULL),*c1=tg_object_bytes(objects[1],NULL);
            if(c0&&c1&&!strcmp(c0,"info")&&!strcmp(c1,"exists")) return tg_set_result(i,"0",TG_OK);
        }
        return tg_set_result(i,"interpreter is disposing a command",TG_ERROR);
    }
    if(!i||i->depth>=MAX_DEPTH||(i->depth&&!i->compat_callback_depth)||i->disposing||!objects||!count||count>MAX_ARGS) return TG_ERROR;
    for(size_t k=0;k<count;k++) if(!objects[k]) return TG_ERROR;
    int rc=tg_command_try_objects(i,count,objects);
    if(rc>=0) return rc;
    const char *args[MAX_ARGS];
    for(size_t k=0;k<count;k++) {
        size_t length;
        args[k]=tg_object_bytes(objects[k],&length);
        if(!args[k]||length>=1024u*1024u||memchr(args[k],0,length))
            return tg_set_result(i,"object cannot be represented by the current command engine",TG_ERROR);
    }
    return tg_call(i,count,args);
}
