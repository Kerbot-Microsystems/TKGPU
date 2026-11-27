#include "../TKGPUCoreInternal.H"
#include "../TKGPUList.H"
#include <stdlib.h>
#include <string.h>
enum { TRACE_LIMIT=128 };
struct tg_var_trace { char key[128],script[1024]; unsigned serial; int modes,active; };
void tg_trace_clear(tg_interp *i) { free(i->traces); i->traces=NULL; }
static int modes(const char *text) {
    int bits=0;
    if(strchr(text,'w')||strstr(text,"write")) bits|=1;
    if(strchr(text,'u')||strstr(text,"unset")) bits|=2;
    if(strchr(text,'r')||strstr(text,"read")) bits|=4;
    return bits;
}
int tg_trace_command(tg_interp *i,size_t n,char **a) {
    if(strcmp(a[0],"trace")) return -1;
    const char *op=NULL,*key=NULL,*mode_str=NULL,*script=NULL;
    if(n>=2&&!strcmp(a[1],"variable")) {
        if(n!=5) return tg_set_result(i,"invalid trace arguments",TG_ERROR);
        op="add"; key=a[2]; mode_str=a[3]; script=a[4];
    } else if(n>=2&&!strcmp(a[1],"vdelete")) {
        if(n!=5) return tg_set_result(i,"invalid trace arguments",TG_ERROR);
        op="remove"; key=a[2]; mode_str=a[3]; script=a[4];
    } else if(n>=2&&!strcmp(a[1],"vinfo")) {
        if(n!=3) return tg_set_result(i,"invalid trace arguments",TG_ERROR);
        op="info"; key=a[2];
    } else if(n>=4&&!strcmp(a[2],"variable")) {
        op=a[1]; key=a[3];
        if(n==6) { mode_str=a[4]; script=a[5]; }
        else if(n!=4||strcmp(op,"info")) return tg_set_result(i,"invalid trace arguments",TG_ERROR);
    } else return tg_set_result(i,"unsupported variable trace form",TG_ERROR);

    if(key[0]==':'&&key[1]==':') key+=2;
    if(!*key||strlen(key)>=128||strstr(key,"::")) return tg_set_result(i,"unsupported trace variable",TG_ERROR);
    if(!strcmp(op,"info")) {
        char result[MAX_WORD]={0};
        for(int k=TRACE_LIMIT-1;k>=0;k--) if(i->traces&&i->traces[k].serial&&!strcmp(i->traces[k].key,key)) {
            struct tg_var_trace *t=&i->traces[k]; char record[1200]={0};
            const char *ops=t->modes==3?"write unset":t->modes==1?"write":"unset";
            if(!tg_list_append(record,sizeof record,ops)||!tg_list_append(record,sizeof record,t->script)||!tg_list_append(result,sizeof result,record)) return tg_set_result(i,"trace list too long",TG_ERROR);
        }
        return tg_set_result(i,result,TG_OK);
    }
    int bits=modes(mode_str?mode_str:"");
    if(!bits||(script&&strlen(script)>=1024)) return tg_set_result(i,"unsupported trace modes or callback",TG_ERROR);
    if(!strcmp(op,"remove")) {
        for(int k=0;k<TRACE_LIMIT;k++) if(i->traces&&i->traces[k].serial&&i->traces[k].modes==bits&&!strcmp(i->traces[k].key,key)&&!strcmp(i->traces[k].script,script)) { i->traces[k].serial=0; break; }
        return tg_set_result(i,"",TG_OK);
    }
    if(strcmp(op,"add")) return tg_set_result(i,"unsupported trace operation",TG_ERROR);
    if(!i->traces) i->traces=calloc(TRACE_LIMIT,sizeof *i->traces);
    if(!i->traces||i->trace_next==UINT32_MAX) return tg_set_result(i,"trace allocation failed",TG_ERROR);
    for(int k=0;k<TRACE_LIMIT;k++) if(!i->traces[k].serial) {
        struct tg_var_trace *t=&i->traces[k]; memset(t,0,sizeof *t);
        strcpy(t->key,key); strcpy(t->script,script); t->modes=bits; t->serial=++i->trace_next;
        return tg_set_result(i,"",TG_OK);
    }
    return tg_set_result(i,"trace limit reached",TG_ERROR);
}
int tg_trace_notify(tg_interp *i,const char *key,int mode) {
    if(!i->traces) return TG_OK;
    for(int k=TRACE_LIMIT-1;k>=0;k--) {
        struct tg_var_trace *t=&i->traces[k];
        if(!t->serial||t->active||strcmp(t->key,key)||!(t->modes&mode)) continue;
        char script[MAX_WORD]; strcpy(script,t->script); unsigned serial=t->serial;
        if(!tg_list_append(script,sizeof script,key)||!tg_list_append(script,sizeof script,"")||!tg_list_append(script,sizeof script,mode==1?"write":"unset")) return tg_set_result(i,"trace callback too long",TG_ERROR);
        t->active=1; const char *cursor=script;
        int rc=tg_eval_script(i,&cursor,0);
        if(t->serial==serial) t->active=0;
        if(rc!=TG_OK) return rc;
    }
    if(mode==2) for(int k=0;k<TRACE_LIMIT;k++) if(i->traces[k].serial&&!strcmp(i->traces[k].key,key)) i->traces[k].serial=0;
    return TG_OK;
}
