#include "TKGPUCompat.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>

typedef struct nre_frame {
    Tcl_NRPostProc *proc;
    void *data[4];
    struct nre_frame *next;
} nre_frame;

typedef struct math_entry {
    char *name;
    int num_args;
    Tcl_ValueType *arg_types;
    Tcl_MathProc *proc;
    void *data;
    struct math_entry *next;
} math_entry;

typedef struct {
    nre_frame *nre_top;
    math_entry *maths;
} nre_state;

static const char nre_key[]="TKGPU::nre-math";

static void dispose_nre(void *data,Tcl_Interp *i) {
    nre_state *s=data;
    (void)i;
    while(s->nre_top) {
        nre_frame *f=s->nre_top; s->nre_top=f->next; free(f);
    }
    while(s->maths) {
        math_entry *m=s->maths; s->maths=m->next;
        free(m->name); free(m->arg_types); free(m);
    }
    free(s);
}

static nre_state *get_nre(Tcl_Interp *i,int create) {
    if(!i||i->disposing||i->destroying) return NULL;
    nre_state *s=Tcl_GetAssocData(i,nre_key,NULL);
    if(!s&&create) {
        s=calloc(1,sizeof *s);
        if(!s) Tcl_Panic("cannot allocate NRE state");
        Tcl_SetAssocData(i,nre_key,dispose_nre,s);
    }
    return s;
}

void Tcl_NRAddCallback(Tcl_Interp *i,Tcl_NRPostProc *postProc,void *d0,void *d1,void *d2,void *d3) {
    if(!i||!postProc) return;
    nre_state *s=get_nre(i,1);
    nre_frame *f=calloc(1,sizeof *f);
    if(!f) Tcl_Panic("cannot allocate NRE callback");
    f->proc=postProc;
    f->data[0]=d0; f->data[1]=d1; f->data[2]=d2; f->data[3]=d3;
    f->next=s->nre_top;
    s->nre_top=f;
}

int Tcl_NRCallObjProc(Tcl_Interp *i,Tcl_ObjCmdProc *objProc,void *clientData,int objc,Tcl_Obj *const objv[]) {
    if(!i||!objProc) return TG_ERROR;
    nre_state *s=get_nre(i,1);
    nre_frame *mark=s->nre_top;
    int rc=objProc(clientData,i,objc,objv);
    while(s->nre_top&&s->nre_top!=mark) {
        nre_frame *f=s->nre_top;
        s->nre_top=f->next;
        Tcl_NRPostProc *proc=f->proc;
        void *data[4]={f->data[0],f->data[1],f->data[2],f->data[3]};
        free(f);
        rc=proc(data,i,rc);
    }
    return rc;
}

typedef struct {
    Tcl_ObjCmdProc *nre_proc;
    void *data;
    Tcl_CmdDeleteProc *del_proc;
} nre_cmd_wrap;

static int nre_wrap_invoke(void *d,Tcl_Interp *i,int objc,Tcl_Obj *const objv[]) {
    nre_cmd_wrap *w=d;
    return Tcl_NRCallObjProc(i,w->nre_proc,w->data,objc,objv);
}

static void nre_wrap_delete(void *d) {
    nre_cmd_wrap *w=d;
    if(w->del_proc) w->del_proc(w->data);
    free(w);
}

Tcl_Command Tcl_NRCreateCommand(Tcl_Interp *i,const char *cmdName,Tcl_ObjCmdProc *proc,
                                Tcl_ObjCmdProc *nreProc,void *clientData,Tcl_CmdDeleteProc *deleteProc) {
    if(proc) return Tcl_CreateObjCommand(i,cmdName,proc,clientData,deleteProc);
    if(!nreProc) return NULL;
    nre_cmd_wrap *w=calloc(1,sizeof *w);
    if(!w) Tcl_Panic("cannot allocate NRE command wrapper");
    w->nre_proc=nreProc; w->data=clientData; w->del_proc=deleteProc;
    Tcl_Command tok=Tcl_CreateObjCommand(i,cmdName,nre_wrap_invoke,w,nre_wrap_delete);
    if(!tok) free(w);
    return tok;
}

int Tcl_NREvalObj(Tcl_Interp *i,Tcl_Obj *objPtr,int flags) {
    return Tcl_EvalObjEx(i,objPtr,flags);
}

int Tcl_NREvalObjv(Tcl_Interp *i,int objc,Tcl_Obj *const objv[],int flags) {
    return Tcl_EvalObjv(i,objc,objv,flags);
}

int Tcl_NRCmdSwap(Tcl_Interp *i,Tcl_Command cmd,int objc,Tcl_Obj *const objv[],int flags) {
    (void)flags;
    Tcl_CmdInfo info;
    if(!cmd||!Tcl_GetCommandInfoFromToken(cmd,&info)) return TG_ERROR;
    if(info.isNativeObjectProc&&info.objProc) {
        return info.objProc(info.objClientData,i,objc,objv);
    }
    return Tcl_EvalObjv(i,objc,objv,0);
}

int Tcl_NRExprObj(Tcl_Interp *i,Tcl_Obj *objPtr,Tcl_Obj *resultPtr) {
    Tcl_Obj *val=NULL;
    int rc=Tcl_ExprObj(i,objPtr,&val);
    if(rc!=TG_OK) return rc;
    if(val) {
        if(resultPtr) {
            int len=0; const char *s=Tcl_GetStringFromObj(val,&len);
            Tcl_SetStringObj(resultPtr,s,len);
        } else {
            Tcl_SetObjResult(i,val);
        }
        TKGPU_ReleaseObject(val);
    }
    return TG_OK;
}

static int is_var_char(unsigned char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
}

Tcl_Obj *Tcl_SubstObj(Tcl_Interp *i,Tcl_Obj *objPtr,int flags) {
    if(!i||i->disposing||!objPtr) return NULL;
    int len=0;
    const char *s=Tcl_GetStringFromObj(objPtr,&len);
    Tcl_Obj *out=Tcl_NewObj();
    if(!out) return NULL;
    const char *p=s,*end=s+len;
    while(p<end) {
        if(*p=='\\'&&(flags&4)) {
            char buf[8]; int consumed=0;
            int n=Tcl_UtfBackslash(p,&consumed,buf);
            Tcl_AppendToObj(out,buf,n);
            p+=consumed>0?consumed:1;
        } else if(*p=='$'&&(flags&2)) {
            const char *q=p+1;
            if(q<end&&*q=='{') {
                const char *close=memchr(q+1,'}',(size_t)(end-(q+1)));
                if(close) {
                    Tcl_Obj *name=Tcl_NewStringObj(q+1,(int)(close-(q+1)));
                    TKGPU_RetainObject(name);
                    Tcl_Obj *val=Tcl_ObjGetVar2(i,name,NULL,TKGPU_LEAVE_ERR_MSG);
                    TKGPU_ReleaseObject(name);
                    if(!val) { TclFreeObj(out); return NULL; }
                    Tcl_AppendObjToObj(out,val);
                    p=close+1;
                    continue;
                }
            } else {
                const char *start=q;
                while(q<end&&is_var_char((unsigned char)*q)) ++q;
                if(q>start) {
                    Tcl_Obj *name=Tcl_NewStringObj(start,(int)(q-start));
                    TKGPU_RetainObject(name);
                    Tcl_Obj *val=Tcl_ObjGetVar2(i,name,NULL,TKGPU_LEAVE_ERR_MSG);
                    TKGPU_ReleaseObject(name);
                    if(!val) { TclFreeObj(out); return NULL; }
                    Tcl_AppendObjToObj(out,val);
                    p=q;
                    continue;
                }
            }
            Tcl_AppendToObj(out,p++,1);
        } else if(*p=='['&&(flags&1)) {
            int depth=1;
            const char *q=p+1;
            while(q<end&&depth>0) {
                if(*q=='\\'&&q+1<end) { q+=2; continue; }
                if(*q=='[') ++depth;
                else if(*q==']') --depth;
                ++q;
            }
            if(depth==0) {
                int rc=Tcl_EvalEx(i,p+1,(int)((q-1)-(p+1)),0);
                if(rc!=TG_OK) { TclFreeObj(out); return NULL; }
                Tcl_AppendObjToObj(out,Tcl_GetObjResult(i));
                p=q;
                continue;
            }
            Tcl_AppendToObj(out,p++,1);
        } else {
            Tcl_AppendToObj(out,p++,1);
        }
    }
    return out;
}

int Tcl_NRSubstObj(Tcl_Interp *i,Tcl_Obj *objPtr,int flags) {
    Tcl_Obj *res=Tcl_SubstObj(i,objPtr,flags);
    if(!res) return TG_ERROR;
    Tcl_SetObjResult(i,res);
    return TG_OK;
}

/* --- Math Functions --- */

void Tcl_CreateMathFunc(Tcl_Interp *i,const char *name,int numArgs,Tcl_ValueType *argTypes,
                        Tcl_MathProc *proc,void *clientData) {
    if(!i||i->disposing||!name||!proc||numArgs<0) return;
    nre_state *s=get_nre(i,1);
    for(math_entry *m=s->maths;m;m=m->next) {
        if(!strcmp(m->name,name)) {
            free(m->arg_types);
            m->num_args=numArgs;
            m->arg_types=numArgs>0?malloc((size_t)numArgs*sizeof *argTypes):NULL;
            if(numArgs>0&&argTypes) memcpy(m->arg_types,argTypes,(size_t)numArgs*sizeof *argTypes);
            m->proc=proc; m->data=clientData;
            return;
        }
    }
    math_entry *m=calloc(1,sizeof *m);
    if(!m) Tcl_Panic("cannot allocate math function");
    size_t n=strlen(name);
    m->name=malloc(n+1);
    if(!m->name) Tcl_Panic("cannot copy math function name");
    memcpy(m->name,name,n+1);
    m->num_args=numArgs;
    if(numArgs>0) {
        m->arg_types=malloc((size_t)numArgs*sizeof *argTypes);
        if(argTypes) memcpy(m->arg_types,argTypes,(size_t)numArgs*sizeof *argTypes);
    }
    m->proc=proc; m->data=clientData;
    m->next=s->maths; s->maths=m;
}

int Tcl_GetMathFuncInfo(Tcl_Interp *i,const char *name,int *numArgsPtr,
                        Tcl_ValueType **argTypesPtr,Tcl_MathProc **procPtr,void **clientDataPtr) {
    nre_state *s=get_nre(i,0);
    if(!s||!name) {
        if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown math function \"%s\"",name?name:""));
        return TG_ERROR;
    }
    for(math_entry *m=s->maths;m;m=m->next) {
        if(!strcmp(m->name,name)) {
            if(numArgsPtr) *numArgsPtr=m->num_args;
            if(argTypesPtr) {
                if(m->num_args>0&&m->arg_types) {
                    *argTypesPtr=(Tcl_ValueType *)Tcl_Alloc((unsigned)((size_t)m->num_args*sizeof(Tcl_ValueType)));
                    memcpy(*argTypesPtr,m->arg_types,(size_t)m->num_args*sizeof(Tcl_ValueType));
                } else {
                    *argTypesPtr=NULL;
                }
            }
            if(procPtr) *procPtr=m->proc;
            if(clientDataPtr) *clientDataPtr=m->data;
            return TG_OK;
        }
    }
    if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("unknown math function \"%s\"",name));
    return TG_ERROR;
}

Tcl_Obj *Tcl_ListMathFuncs(Tcl_Interp *i,const char *pattern) {
    Tcl_Obj *list=Tcl_NewListObj(0,NULL);
    if(!list) return NULL;
    nre_state *s=get_nre(i,0);
    if(s) {
        for(math_entry *m=s->maths;m;m=m->next) {
            if(!pattern||Tcl_StringMatch(m->name,pattern)) {
                Tcl_ListObjAppendElement(i,list,Tcl_NewStringObj(m->name,-1));
            }
        }
    }
    return list;
}

/* --- ParseArgsObjv --- */

enum {
    ARGV_END=0, ARGV_CONSTANT=15, ARGV_INT=16, ARGV_STRING=17,
    ARGV_REST=18, ARGV_FLOAT=19, ARGV_FUNC=20, ARGV_GENFUNC=21, ARGV_HELP=22
};

int Tcl_ParseArgsObjv(Tcl_Interp *i,const Tcl_ArgvInfo *argTable,int *objcPtr,
                      Tcl_Obj *const *objv,Tcl_Obj ***remObjv) {
    if(!objcPtr||!objv||!argTable) return TG_ERROR;
    int argc=*objcPtr;
    Tcl_Obj **rem=remObjv?(Tcl_Obj **)Tcl_Alloc((unsigned)((size_t)(argc+1)*sizeof *rem)):NULL;
    int remCount=0;
    if(rem&&argc>0) rem[remCount++]=objv[0];
    int idx=1;
    while(idx<argc) {
        const char *arg=Tcl_GetString(objv[idx]);
        const Tcl_ArgvInfo *match=NULL;
        for(const Tcl_ArgvInfo *a=argTable;a->type!=ARGV_END;a++) {
            if(a->keyStr&&!strcmp(a->keyStr,arg)) { match=a; break; }
        }
        if(!match) {
            if(rem) rem[remCount++]=objv[idx];
            ++idx;
            continue;
        }
        ++idx;
        if(match->type==ARGV_CONSTANT) {
            if(match->dstPtr) *(int *)match->dstPtr=(int)(intptr_t)match->srcPtr;
        } else if(match->type==ARGV_INT) {
            if(idx>=argc) {
                if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("\"%s\" option requires an additional argument",arg));
                if(rem) Tcl_Free((char *)rem);
                return TG_ERROR;
            }
            int v=0;
            if(Tcl_GetIntFromObj(i,objv[idx++],&v)!=TG_OK) {
                if(rem) Tcl_Free((char *)rem);
                return TG_ERROR;
            }
            if(match->dstPtr) *(int *)match->dstPtr=v;
        } else if(match->type==ARGV_STRING) {
            if(idx>=argc) {
                if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("\"%s\" option requires an additional argument",arg));
                if(rem) Tcl_Free((char *)rem);
                return TG_ERROR;
            }
            const char *s=Tcl_GetString(objv[idx++]);
            if(match->dstPtr) *(const char **)match->dstPtr=s;
        } else if(match->type==ARGV_FLOAT) {
            if(idx>=argc) {
                if(i) Tcl_SetObjResult(i,Tcl_ObjPrintf("\"%s\" option requires an additional argument",arg));
                if(rem) Tcl_Free((char *)rem);
                return TG_ERROR;
            }
            double d=0.0;
            if(Tcl_GetDoubleFromObj(i,objv[idx++],&d)!=TG_OK) {
                if(rem) Tcl_Free((char *)rem);
                return TG_ERROR;
            }
            if(match->dstPtr) *(double *)match->dstPtr=d;
        } else if(match->type==ARGV_REST) {
            if(match->dstPtr) *(int *)match->dstPtr=remCount;
            while(idx<argc) { if(rem) rem[remCount++]=objv[idx]; ++idx; }
        }
    }
    if(rem) { rem[remCount]=NULL; *remObjv=rem; }
    *objcPtr=remCount;
    return TG_OK;
}
