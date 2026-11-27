/* Original TKGPU validation path. Callbacks receive literal argument vectors,
   and no widget pointer survives a user callback. */
#include "../TKGPUCoreInternal.H"
#include "../TKGPUList.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../widgets/controls/TKGPUControls.H"
#include "TKGPUUtf8.H"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
enum { V_NONE,V_FOCUS,V_IN,V_OUT,V_KEY,V_ALL };
typedef struct {
    const char *old,*proposed,*change,*path,*reason,*mode;
    int action,index;
} validation;
static int bool_value(const char *s,int *value) {
    char buf[16]; size_t n=strlen(s);
    if(!n||n>=sizeof buf) return 0;
    for(size_t k=0;k<=n;k++) buf[k]=(char)tolower((unsigned char)s[k]);
    if(!strcmp(buf,"1")||!strncmp("true",buf,n)||!strncmp("yes",buf,n)||(!strncmp("on",buf,n)&&n>=2)) { *value=1; return 1; }
    if(!strcmp(buf,"0")||!strncmp("false",buf,n)||!strncmp("no",buf,n)||(!strncmp("off",buf,n)&&n>=2)) { *value=0; return 1; }
    return 0;
}
static const char *subst(char ch,const validation *v,const char *action,const char *index) {
    switch(ch) {
    case 'd': return action; case 'i': return index;
    case 'P': return v->proposed; case 's': return v->old;
    case 'S': return v->change; case 'v': return v->mode;
    case 'V': return v->reason; case 'W': return v->path;
    case '%': return "%"; default: return NULL;
    }
}
static int callback(tg_interp *i,const char *prefix,const validation *v) {
    /* Standard Python register() tuple/list prefixes, not arbitrary Tcl
       script syntax. Expanded values never go back through a tokenizer. */
    char storage[MAX_WORD],word[MAX_WORD],action[16],index[16];
    char *args[64]; size_t count=0,used=0; int rc;
    snprintf(action,sizeof action,"%d",v->action);
    snprintf(index,sizeof index,"%d",v->index);
    while((rc=tg_list_next(&prefix,word,sizeof word))>0) {
        if(count==64||used>=sizeof storage) goto bad;
        args[count++]=storage+used;
        for(size_t k=0;word[k];k++) {
            const char *value=NULL;
            if(word[k]=='%'&&word[k+1]) { value=subst(word[k+1],v,action,index); if(value) k++; }
            if(value) {
                size_t len=strlen(value);
                if(len>=sizeof storage-used) goto bad;
                memcpy(storage+used,value,len); used+=len;
            } else { if(used+1>=sizeof storage) goto bad; storage[used++]=word[k]; }
        }
        if(used>=sizeof storage) goto bad;
        storage[used++]=0;
    }
    if(rc<0||!count||i->depth>=i->recursion_limit) goto bad;
    i->depth++;
    int status=tg_execute(i,count,args);
    i->depth--;
    return status;
bad:
    return tg_set_result(i,"invalid or oversized validation command prefix",TG_ERROR);
}
static int active_mode(unsigned mode,const char *reason) {
    if(!strcmp(reason,"forced")) return 1;
    if(!strcmp(reason,"variable")) return mode!=V_NONE;
    if(mode==V_ALL) return 1;
    if(!strcmp(reason,"key")) return mode==V_KEY;
    return mode==V_FOCUS||(!strcmp(reason,"focusin")?mode==V_IN:mode==V_OUT);
}
static int check(tg_interp *i,tg_id id,const char *proposed,const char *change,int action,int index,const char *reason) {
    tg_widget *w=tg_lookup(i->ui,id);
    if(!w||!tg_textvar_editable(w->kind)) return 0;
    if(w->validate_busy) { if(!w->themed) w->validate_mode=V_NONE; return 1; }
    if(!active_mode(w->validate_mode,reason)||!w->validate_command[0]) return 1;
    static const char *modes[]={"none","focus","focusin","focusout","key","all"};
    char old[1024],path[TG_WIDGET_PATH_CAP],prefix[256],invalid[256];
    strcpy(old,w->text); strcpy(path,w->path); strcpy(prefix,w->validate_command); strcpy(invalid,w->invalid_command);
    int variable=!strcmp(reason,"variable");
    validation v={old,proposed,change,path,variable?"forced":reason,modes[w->validate_mode],action,index};
    unsigned mode=w->validate_mode; int ok=0;
    int state=w->state; size_t cursor=w->cursor,first=w->sel_first,last=w->sel_last,anchor=w->sel_anchor;
    w->validate_busy=1;
    int rc=callback(i,prefix,&v);
    int valid=rc==TG_OK&&bool_value(i->result,&ok);
    if(valid&&!ok&&invalid[0]&&strcmp(reason,"variable")&&tg_lookup(i->ui,id)) {
        if(callback(i,invalid,&v)!=TG_OK) valid=0;
    }
    w=tg_lookup(i->ui,id);
    if(!w) return 0;
    w->validate_busy=0;
    if(!valid) { w->validate_mode=V_NONE; return -1; }
    if(variable&&!ok&&!w->themed) w->validate_mode=V_NONE;
    if(w->themed&&action<0) {
        unsigned bit=1u<<8;
        if(ok) { w->ttk_state_set&=~bit; w->ttk_state_clear|=bit; }
        else { w->ttk_state_set|=bit; w->ttk_state_clear&=~bit; }
    }
    /* A callback that changes or destroys the input wins; never overwrite it
       with a pre-callback candidate. Classic Tk disables recursive validation. */
    if(strcmp(w->text,old)||w->validate_mode!=mode||strcmp(w->validate_command,prefix)||
       (action>=0&&(w->state!=state||w->cursor!=cursor||w->sel_first!=first||
                    w->sel_last!=last||w->sel_anchor!=anchor))) return 0;
    return ok;
}
static void background_error(tg_interp *i) {
    char message[MAX_WORD]; snprintf(message,sizeof message,"%s",i->result);
    (void)tg_var_store(i,"errorInfo",message);
}
int tg_entry_validation_command(tg_interp *i,size_t n,char **a) {
    if(n<2||a[0][0]!='.') return -1;
    tg_widget *w=tg_lookup(i->ui,tg_ui_find(i->ui,a[0]));
    if(!w||!tg_textvar_editable(w->kind)) return -1;
    tg_id id=w->id;
    if(n==2&&!strcmp(a[1],"validate")&&w->themed) {
        char proposed[1024]; strcpy(proposed,w->text);
        int rc=check(i,id,proposed,"",-1,-1,"forced");
        if(rc<0) return tg_set_result(i,"entry validation callback failed",TG_ERROR);
        return tg_set_result_kind(i,rc?"1":"0",TG_RESULT_INTEGER);
    }
    int insert=n==4&&!strcmp(a[1],"insert"),erase=(n==3||n==4)&&!strcmp(a[1],"delete");
    if(!insert&&!erase) return -1;
    if(w->validate_busy) { if(!w->themed) w->validate_mode=V_NONE; return -1; }
    if(w->state!=TG_STATE_NORMAL||!active_mode(w->validate_mode,"key")||!w->validate_command[0]) return -1;
    size_t at,last;
    if(!tg_entry_index(w,a[2],&at)) return -1;
    tg_widget candidate=*w; char ignored[16],change[1024];
    if(tg_entry_command(&candidate,n,a,ignored,sizeof ignored)!=TG_OK) return -1;
    if(!strcmp(candidate.text,w->text)) return -1;
    if(insert) strcpy(change,a[3]);
    else {
        last=at+1;
        if(n==4&&!tg_entry_index(w,a[3],&last)) return -1;
        size_t lo=tg_entry_offset(w,at),hi=tg_entry_offset(w,last);
        memcpy(change,w->text+lo,hi-lo); change[hi-lo]=0;
    }
    int rc=check(i,id,candidate.text,change,insert?1:0,(int)at,"key");
    if(rc<0) return tg_set_result(i,"entry validation callback failed",TG_ERROR);
    if(!rc) return tg_set_result(i,"",TG_OK);
    return -1;
}
int tg_entry_validation_input(tg_interp *i,tg_event e) {
    if(e.kind!=TG_KEY_DOWN&&e.kind!=TG_TEXT_INPUT) return 1;
    tg_id id=tg_ui_focused(i->ui);
    tg_widget *w=tg_lookup(i->ui,id);
    if(!w||!tg_textvar_editable(w->kind)||w->state!=TG_STATE_NORMAL||!active_mode(w->validate_mode,"key")||!w->validate_command[0]) return 1;
    uint32_t key=e.key&TG_KEY_VALUE_MASK;
    if(e.kind==TG_KEY_DOWN&&key!=TG_KEY_DELETE&&key!=TG_KEY_BACKSPACE) return 1;
    if(e.kind==TG_TEXT_INPUT&&(key<32||key>0x10ffff)) return 1;
    tg_widget candidate=*w;
    size_t at=w->cursor,last=at;
    int selection=w->sel_first<w->sel_last,insert=e.kind==TG_TEXT_INPUT;
    if(selection) { at=w->sel_first; last=w->sel_last; }
    else if(!insert) {
        if(key==TG_KEY_BACKSPACE&&at) at--;
        else if(key==TG_KEY_DELETE) last++;
    }
    if(!tg_entry_edit(&candidate,key)||!strcmp(w->text,candidate.text)) return 1;
    char change[1024];
    if(insert) {
        unsigned char bytes[5]; size_t used;
        if(tg_utf8_encode(key,bytes,4,&used)!=TG_OK) return 1;
        memcpy(change,bytes,used); change[used]=0;
    } else {
        size_t lo=tg_entry_offset(w,at),hi=tg_entry_offset(w,last);
        memcpy(change,w->text+lo,hi-lo); change[hi-lo]=0;
    }
    int rc=check(i,id,candidate.text,change,insert?1:0,(int)at,"key");
    if(rc<0) background_error(i);
    return rc>0&&tg_ui_focused(i->ui)==id;
}
int tg_entry_validation_replace(tg_interp *i,tg_id id,const char *proposed,const char *change,int action,int index) {
    int rc=check(i,id,proposed,change,action,index,"key");
    if(rc<0) background_error(i);
    return rc>0&&tg_ui_focused(i->ui)==id;
}
void tg_entry_validation_focus(tg_interp *i,tg_id id,int entering) {
    tg_widget *w=tg_lookup(i->ui,id);
    if(!w||!tg_textvar_editable(w->kind)||!active_mode(w->validate_mode,entering?"focusin":"focusout")) return;
    char proposed[1024]; strcpy(proposed,w->text);
    if(check(i,id,proposed,"",-1,-1,entering?"focusin":"focusout")<0) background_error(i);
}

void tg_entry_validation_bound(tg_interp *i,tg_id id,const char *key) {
    tg_widget *w=tg_lookup(i->ui,id);
    if(!w||w->themed||!tg_textvar_editable(w->kind)||!w->validate_mode||
       !w->validate_command[0]||!tg_textvar_matches(w,key)) return;
    size_t k=tg_var_find(i,key);
    if(k==i->count||!strcmp(w->text,i->vars[k].value)||!tg_textvar_valid(i->vars[k].value)) return;
    char proposed[1024]; strcpy(proposed,i->vars[k].value);
    if(check(i,id,proposed,"",-1,-1,"variable")<0) background_error(i);
}
/* Classic Entry validates external variable writes, but the variable remains
   authoritative even when the callback returns false. ttk does not validate
   variable writes. Copy IDs before callbacks: widget arrays can compact. */
void tg_entry_validation_variable(tg_interp *i,const char *key) {
    tg_id ids[TG_WIDGET_LIMIT]; size_t count=0;
    for(size_t k=1;k<i->ui->count;k++) {
        tg_widget *w=&i->ui->items[k];
        if(!w->themed&&tg_textvar_editable(w->kind)&&w->validate_mode!=V_NONE&&
           w->validate_command[0]&&tg_textvar_matches(w,key)) ids[count++]=w->id;
    }
    for(size_t j=0;j<count;j++) {
        tg_entry_validation_bound(i,ids[j],key);
    }
}
