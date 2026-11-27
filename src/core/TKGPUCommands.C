#include "TKGPUCoreInternal.H"
#include "TKGPUExpression.H"
#include "../widgets/ttk/TKGPUStyle.H"
#include "../widgets/image/TKGPUImageRegistry.H"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

static int integer(const char *s,int64_t *v) {
    char *e; errno=0; intmax_t x=strtoimax(s,&e,10);
    if(s==e||*e||errno||x<INT64_MIN||x>INT64_MAX) return 0;
    *v=(int64_t)x; return 1;
}
/* Only commands known to be read-only avoid native frame invalidation.
   Do not classify arbitrary callbacks or widget-specific operations here. */
static int ui_read_only(tg_interp *i,size_t n,char **a) {
    if(n>=2&&a[0][0]=='.') {
        const tg_widget *w=tg_ui_get(i->ui,tg_ui_find(i->ui,a[0]));
        if(w&&w->kind==TG_TEXT&&((n==5&&!strcmp(a[1],"compare"))||
           (n>=4&&!strcmp(a[1],"count"))||
           (n==3&&!strcmp(a[1],"dlineinfo"))||
           (n==3&&!strcmp(a[1],"bbox"))||
           (n==3&&!strcmp(a[1],"edit")&&!strcmp(a[2],"modified"))||
           (n==3&&!strcmp(a[1],"mark")&&!strcmp(a[2],"names"))||
           (n==4&&!strcmp(a[1],"mark")&&!strcmp(a[2],"gravity"))||
           (!strcmp(a[1],"tag")&&((n==3&&!strcmp(a[2],"names"))||
            (n==4&&(!strcmp(a[2],"names")||!strcmp(a[2],"ranges")))||
            (n==5&&!strcmp(a[2],"cget"))||
            ((n==5||n==6)&&(!strcmp(a[2],"nextrange")||!strcmp(a[2],"prevrange"))))))) return 1;
        if(w&&w->kind==TG_SCROLLBAR&&((n==2&&!strcmp(a[1],"get"))||
           (n==4&&(!strcmp(a[1],"fraction")||!strcmp(a[1],"delta")||!strcmp(a[1],"identify"))))) return 1;
        if(w&&w->kind==TG_MENU&&((n==3&&(!strcmp(a[1],"index")||!strcmp(a[1],"type")||!strcmp(a[1],"yposition")))||
           (n==4&&!strcmp(a[1],"entrycget"))||
           ((n==3||n==4)&&(!strcmp(a[1],"entryconfigure")||!strcmp(a[1],"entryconfig"))))) return 1;
        if(w&&w->kind==TG_TREEVIEW&&
           ((n==2&&(!strcmp(a[1],"children")||!strcmp(a[1],"selection")||!strcmp(a[1],"focus")))||
            (n==3&&(!strcmp(a[1],"index")||!strcmp(a[1],"exists")||!strcmp(a[1],"children")||
                      !strcmp(a[1],"item")||!strcmp(a[1],"set")||!strcmp(a[1],"bbox")||
                      !strcmp(a[1],"next")||!strcmp(a[1],"prev")||!strcmp(a[1],"parent")))||
            (n==4&&(!strcmp(a[1],"item")||!strcmp(a[1],"set")||!strcmp(a[1],"bbox")))||
            (n==5&&!strcmp(a[1],"identify")))) return 1;
        if(w&&w->kind==TG_PANEDWINDOW&&
           ((n==2&&!strcmp(a[1],"panes"))||
            ((n==3||n==4)&&!strcmp(a[1],"pane"))||
            (n==3&&!strcmp(a[1],"sashpos"))||
            (n==4&&!strcmp(a[1],"identify")))) return 1;
        if(w&&w->kind==TG_NOTEBOOK&&
           ((n==2&&(!strcmp(a[1],"tabs")||!strcmp(a[1],"select")))||
            ((n==3||n==4)&&!strcmp(a[1],"tab"))||
            (n==3&&(!strcmp(a[1],"index")||!strcmp(a[1],"bbox")))||
            ((n==4||n==5)&&!strcmp(a[1],"identify")))) return 1;
    }
    if(a[0][0]=='.'&&((n==3&&!strcmp(a[1],"index"))||
       ((n==3||n==4)&&!strcmp(a[1],"get")))) return 1;
    if(!strcmp(a[0],"winfo")&&n==3) return 1;
    if(!strcmp(a[0],"focus")&&n==1) return 1;
    if(a[0][0]=='.'&&((n==2&&(!strcmp(a[1],"xview")||!strcmp(a[1],"yview")))||(n>=3&&(!strcmp(a[1],"canvasx")||!strcmp(a[1],"canvasy")||!strcmp(a[1],"gettags")||!strcmp(a[1],"type")||!strcmp(a[1],"bbox")||!strcmp(a[1],"itemcget")))||(n==3&&!strcmp(a[1],"coords"))||(n>=3&&!strcmp(a[1],"find"))||((n==3||n==4)&&(!strcmp(a[1],"itemconfigure")||!strcmp(a[1],"itemconfig"))))) return 1;
    if(a[0][0]=='.'&&((n==3&&(!strcmp(a[1],"cget")||!strcmp(a[1],"instate")))||(n==2&&!strcmp(a[1],"state")))) return 1;
    if(!strcmp(a[0],"ttk::style")&&n>=3) {
        if(!strcmp(a[1],"lookup")) return 1;
        if((!strcmp(a[1],"configure")||!strcmp(a[1],"map"))&&n<=4) return 1;
        if(!strcmp(a[1],"theme")&&n==3) return 1;
    }
    if(n==3&&(!strcmp(a[0],"grid")||!strcmp(a[0],"pack")||!strcmp(a[0],"place"))&&(!strcmp(a[1],"slaves")||!strcmp(a[1],"info"))) return 1;
    return 0;
}
int tg_execute(tg_interp *i,size_t n,char **a) {
    if(i->deleted) return tg_set_result(i,"attempt to call eval in deleted interpreter",TG_ERROR);
    tg_set_result(i,"",TG_OK);
    if(!n) return TG_OK;
    if(i->eval_check&&i->eval_check(i)!=TG_OK) return TG_ERROR;
    int trace=tg_trace_command(i,n,a); if(trace>=0) return trace;
    int native=tg_command_dispatch(i,n,a);
    if(native>=0) return native;
    if(i->event_command) { int event=i->event_command(i,n,a); if(event>=0) return event; }
    if(i->tk_command) { int window=i->tk_command(i,n,a); if(window>=0) {
        if(window==TG_OK) tg_host_ui_result_kind(i,n,a);
        return window;
    } }
    if(!strcmp(a[0],"return")) {
        return tg_set_result(i, n > 1 ? a[1] : "", TG_OK);
    }
    if(!strcmp(a[0],"lower")||(!strcmp(a[0],"raise")&&n!=2)) {
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[0],"set")) {
        if(n==3) return tg_var_store(i,a[1],a[2]);
        if(n==2) {
            size_t k=tg_var_find(i,a[1]);
            return k<i->count&&i->vars[k].kind!=TG_VAR_ARRAY?tg_set_result_kind(i,i->vars[k].value,i->vars[k].kind):tg_set_result(i,"unknown variable",TG_ERROR);
        }
        return tg_set_result(i,"wrong # args: set name ?value?",TG_ERROR);
    }
    if(!strcmp(a[0],"unset")) {
        if(n!=2) return tg_set_result(i,"wrong # args: unset name",TG_ERROR);
        return tg_var_unset(i,a[1]);
    }
    if(!strcmp(a[0],"append")) {
        if(n<2) return tg_set_result(i,"wrong # args: append name ?value ...?",TG_ERROR);
        char b[MAX_WORD]; size_t len=0,k=tg_var_find(i,a[1]);
        if(k<i->count) { len=strlen(i->vars[k].value); memcpy(b,i->vars[k].value,len); }
        for(size_t j=2;j<n;j++) {
            size_t m=strlen(a[j]);
            if(m>=sizeof(b)-len) return tg_set_result(i,"append result too long",TG_ERROR);
            memcpy(b+len,a[j],m); len+=m;
        }
        b[len]=0;
        return tg_var_store(i,a[1],b);
    }
    if(!strcmp(a[0],"incr")) {
        if(n<2 || n>3) return tg_set_result(i,"wrong # args: incr name ?increment?",TG_ERROR);
        size_t k=tg_var_find(i,a[1]); int64_t x=0,y=1;
        if((k<i->count && !integer(i->vars[k].value,&x)) ||
           (n==3 && !integer(a[2],&y))) return tg_set_result(i,"expected integer",TG_ERROR);
        if((y>0 && x>INT64_MAX-y)||(y<0 && x<INT64_MIN-y))
            return tg_set_result(i,"integer overflow in prototype",TG_ERROR);
        char b[64]; snprintf(b,sizeof b,"%" PRId64,x+y); return tg_var_store_kind(i,a[1],b,TG_RESULT_INTEGER);
    }
    if(!strcmp(a[0],"expr")) {
        if(n<2) return tg_set_result(i,"wrong # args: expr expression",TG_ERROR);
        char expression[MAX_WORD]; size_t used=0;
        for(size_t j=1;j<n;j++) {
            size_t m=strlen(a[j]);
            if(m>=sizeof(expression)-used-(j>1?1u:0u)) return tg_set_result(i,"expression too long",TG_ERROR);
            if(j>1) expression[used++]=' ';
            memcpy(expression+used,a[j],m); used+=m;
        }
        expression[used]=0;
        return tg_eval_numeric_result(i,expression);
    }
    if(!strcmp(a[0],"error")) {
        if(n<2||n>4) return tg_set_result(i,"wrong # args: error message ?errorInfo? ?errorCode?",TG_ERROR);
        if(tg_var_store(i,"errorInfo",n>=3&&*a[2]?a[2]:a[1])!=TG_OK||
           tg_var_store(i,"errorCode",n==4?a[3]:"NONE")!=TG_OK) return TG_ERROR;
        return tg_set_result(i,a[1],TG_ERROR);
    }
    if(!strcmp(a[0],"info")&&n==3&&!strcmp(a[1],"exists"))
        return tg_set_result_kind(i,tg_var_find(i,a[2])<i->count?"1":"0",TG_RESULT_INTEGER);
    if(!strcmp(a[0],"info")&&n>=2&&!strcmp(a[1],"commands")) {
        if(n==3) {
            if(tg_image_find(i->ui,a[2])||tg_ui_find(i->ui,a[2]))
                return tg_set_result(i,a[2],TG_OK);
            if(tg_command_lookup(i,a[2])<i->command_count)
                return tg_set_result(i,a[2],TG_OK);
            return tg_set_result(i,"",TG_OK);
        }
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[0],"ttk::setTheme")||
       (n==4&&!strcmp(a[0],"ttk::style")&&!strcmp(a[1],"theme")&&!strcmp(a[2],"use"))) {
        const char *name=!strcmp(a[0],"ttk::setTheme")?(n==2?a[1]:NULL):a[3];
        if(!name||tg_style_select_theme(i->ui,name)!=TG_OK)
            return tg_set_result(i,"unknown or invalid ttk theme",TG_ERROR);
        int rc=tg_var_store(i,"ttk::currentTheme",name);
        if(rc!=TG_OK) return rc;
        if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return tg_set_result(i,"",TG_OK);
    }
    if(n==5&&!strcmp(a[0],"ttk::style")&&!strcmp(a[1],"theme")&&!strcmp(a[2],"settings")) {
        char saved[64]; strcpy(saved,tg_style_current_theme(i->ui));
        if(tg_style_select_theme(i->ui,a[3])!=TG_OK)
            return tg_set_result(i,"unknown ttk theme",TG_ERROR);
        const char *script=a[4]; int rc=tg_eval_script(i,&script,0);
        (void)tg_style_select_theme(i->ui,saved);
        if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return rc;
    }
    if(n>=6&&!strcmp(a[0],"ttk::style")&&!strcmp(a[1],"theme")&&!strcmp(a[2],"create")) {
        if(n%2) return tg_set_result(i,"invalid theme creation options",TG_ERROR);
        char *parent="default",*settings=NULL; int seen_parent=0;
        for(size_t k=4;k<n;k+=2) {
            if(!strcmp(a[k],"-parent")&&!seen_parent) { parent=a[k+1]; seen_parent=1; }
            else if(!strcmp(a[k],"-settings")&&!settings) settings=a[k+1];
            else return tg_set_result(i,"invalid theme creation option",TG_ERROR);
        }
        char *args[]={"ttk::style","theme","create",a[3],"-parent",parent};
        int rc=tg_ui_command(i->ui,6,args,i->result,sizeof i->result);
        if(rc!=TG_OK||!settings) return rc;
        char saved[64]; strcpy(saved,tg_style_current_theme(i->ui));
        if(tg_style_select_theme(i->ui,a[3])!=TG_OK) return TG_ERROR;
        const char *script=settings; rc=tg_eval_script(i,&script,0);
        (void)tg_style_select_theme(i->ui,saved);
        if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return rc;
    }
    int rc=tg_binding_command(i,n,a);
    if(rc>=0) return rc;
    rc=tg_control_invoke(i,n,a);
    if(rc>=0) return rc;
    if(n==4&&a[0][0]=='.'&&!strcmp(a[1],"instate")) {
        const tg_widget *w=tg_ui_get(i->ui,tg_ui_find(i->ui,a[0]));
        int match;
        if(tg_ttk_state_matches(i->ui,w,a[2],&match)!=TG_OK)
            return tg_set_result(i,"invalid ttk state specification",TG_ERROR);
        if(!match) return tg_set_result(i,"",TG_OK);
        const char *script=a[3]; return tg_eval_script(i,&script,0);
    }
    rc=tg_history_command(i,n,a);
    if(rc>=0) return rc;
    rc=tg_control_command(i,n,a);
    if(rc>=0) return rc;
    rc=tg_list_command(i,n,a);
    if(rc>=0) return rc;
    if(i->tk_close&&n==2&&!strcmp(a[0],"destroy")&&!strcmp(a[1],".")) {
        i->tk_close(i->tk_host); return tg_set_result(i,"",TG_OK);
    }
    int validation=tg_entry_validation_command(i,n,a);
    if(validation>=0) return validation;
    if(tg_textvar_prepare(i,n,a)!=TG_OK) return TG_ERROR;
    rc=tg_ui_command(i->ui,n,a,i->result,sizeof(i->result));
    if(rc>=0) {
        if(rc==TG_OK) { rc=tg_control_refresh(i,n,a); if(!strcmp(a[0],"destroy")) tg_binding_prune(i); }
        if(rc==TG_OK&&!ui_read_only(i,n,a)) {
            int paint_only=0;
            tg_id paint_target=0;
            if(n>=2&&a[0][0]=='.') {
                tg_id wid=tg_ui_find(i->ui,a[0]);
                const tg_widget *cw=tg_ui_get(i->ui,wid);
                if(cw&&cw->kind==TG_CANVAS&&
                   ((!strcmp(a[1],"create")&&n>=3&&strcmp(a[2],"window")!=0)||
                    !strcmp(a[1],"itemconfigure")||!strcmp(a[1],"itemconfig")||
                    !strcmp(a[1],"coords")||!strcmp(a[1],"move")||!strcmp(a[1],"scale")||
                    !strcmp(a[1],"addtag")||!strcmp(a[1],"dtag")||!strcmp(a[1],"raise")||
                    !strcmp(a[1],"lower")||!strcmp(a[1],"scan")||!strcmp(a[1],"xview")||
                    !strcmp(a[1],"yview")||!strcmp(a[1],"delete"))) {
                    paint_only=1; paint_target=wid;
                }
            }
            if(paint_only&&i->tk_repaint) i->tk_repaint(i->tk_host,paint_target);
            else if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        }
        /* Only successful, explicitly known UI operations produce tags. */
        i->result_kind=TG_RESULT_STRING;
        if(rc==TG_OK) tg_host_ui_result_kind(i,n,a);
        return rc;
    }
    char unknown[512];
    snprintf(unknown,sizeof unknown,"invalid command name \"%.200s\"",a[0]);
    return tg_set_result(i,unknown,TG_ERROR);
}
