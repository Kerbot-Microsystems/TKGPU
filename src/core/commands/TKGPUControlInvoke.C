#include "../TKGPUCoreInternal.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../widgets/ttk/TKGPUToggle.H"
#include "../../widgets/ttk/TKGPUContainers.H"
#include "../../widgets/ttk/TKGPUChoice.H"
#include "../../widgets/ttk/TKGPUExtra.H"
#include "../../widgets/ttk/TKGPUIndicators.H"
#include "../../widgets/controls/TKGPUControls.H"
#include "../../widgets/ttk/tree/TKGPUTree.H"
#include "../../widgets/canvas/TKGPUCanvasItems.H"
#include "../../widgets/text/TKGPUTextWidget.H"
#include "../../widgets/listbox/TKGPUListbox.H"
#include "../TKGPUList.H"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "TKGPUUtf8.H"
static int invoke(tg_interp *i,const char *script) {
    if(!*script) return tg_set_result(i,"",TG_OK);
    const char *cursor=script;
    return tg_eval_script(i,&cursor,0);
}
static int creation(const char *name) {
    if(!strncmp(name,"ttk::",5)) name+=5;
    return !strcmp(name,"entry")||!strcmp(name,"label")||!strcmp(name,"button")||
        !strcmp(name,"message")||!strcmp(name,"menubutton")||!strcmp(name,"combobox")||
        !strcmp(name,"spinbox")||!strcmp(name,"checkbutton")||!strcmp(name,"radiobutton")||
        !strcmp(name,"scale");
}
static int configuration(size_t n,char **a) {
    return n>=4&&a[0][0]=='.'&&(!strcmp(a[1],"configure")||!strcmp(a[1],"config"));
}
int tg_textvar_prepare(tg_interp *i,size_t n,char **a) {
    if(n<2||(!creation(a[0])&&!configuration(n,a))) return TG_OK;
    for(size_t k=2;k+1<n;k+=2) if(!strcmp(a[k],"-textvariable")&&a[k+1][0]) {
        size_t v=tg_var_find(i,a[k+1]);
        if(v<i->count&&!tg_textvar_valid(i->vars[v].value))
            return tg_set_result(i,"bound text exceeds widget capacity or is invalid UTF-8",TG_ERROR);
    }
    return TG_OK;
}
static int menu_action(tg_interp *i,const tg_menu_action *a) {
    if(a->variable[0]&&tg_var_store(i,a->variable,a->value)!=TG_OK) return TG_ERROR;
    if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
    return invoke(i,a->command);
}
static int menu_refresh(tg_interp *i,tg_id id) {
    if(!i->ui->menu_toggle_count) return TG_OK;
    char saved[MAX_WORD]; snprintf(saved,sizeof saved,"%s",i->result);
    for(unsigned row=0;row<32;row++) {
        tg_widget *w=tg_lookup(i->ui,id); tg_menu_action binding;
        int rc=tg_menu_binding(w,row,&binding);
        if(rc<0) break;
        if(!rc) continue;
        size_t k=tg_var_find(i,binding.variable);
        if(k==i->count) {
            if(tg_var_store(i,binding.variable,binding.value)!=TG_OK) return TG_ERROR;
        } else (void)tg_menu_sync_widget(w,binding.variable,i->vars[k].value);
    }
    return tg_set_result(i,saved,TG_OK);
}
static int publish(tg_interp *i,tg_widget *w) {
    if(!w||!tg_textvar_editable(w->kind)||!w->textvariable[0]) return TG_OK;
    size_t k=tg_var_find(i,w->textvariable);
    if(k<i->count&&!strcmp(i->vars[k].value,w->text)) return TG_OK;
    char saved[MAX_WORD],key[128],value[1024]; snprintf(saved,sizeof saved,"%s",i->result);
    strcpy(key,w->textvariable); strcpy(value,w->text);
    if(tg_var_store(i,key,value)!=TG_OK) {
        k=tg_var_find(i,key);
        if(k<i->count) (void)tg_textvar_sync(i->ui,key,i->vars[k].value);
        return TG_ERROR;
    }
    return tg_set_result(i,saved,TG_OK);
}
static int scale_store_var(tg_interp *i,tg_widget *w) {
    if(!w||w->kind!=TG_SCALE||!w->variable[0]) return TG_OK;
    size_t k=tg_var_find(i,w->variable);
    if(k<i->count&&!strcmp(i->vars[k].value,w->value_text)) return TG_OK;
    char saved[MAX_WORD],key[TG_WIDGET_PATH_CAP],val[32];
    snprintf(saved,sizeof saved,"%s",i->result);
    strcpy(key,w->variable); strcpy(val,w->value_text);
    if(tg_var_store(i,key,val)!=TG_OK) return TG_ERROR;
    return tg_set_result(i,saved,TG_OK);
}
static int scale_publish(tg_interp *i,tg_id id) {
    tg_widget *w=tg_lookup(i->ui,id);
    if(!w||w->kind!=TG_SCALE) return TG_OK;
    char saved[MAX_WORD];
    snprintf(saved,sizeof saved,"%s",i->result);
    if(w->variable[0]) (void)scale_store_var(i,w);
    w=tg_lookup(i->ui,id);
    if(w&&w->kind==TG_SCALE&&w->command[0]) {
        char script[600];
        if(strlen(w->command)<sizeof script) {
            strcpy(script,w->command);
            if(tg_list_append(script,sizeof script,w->value_text)) (void)invoke(i,script);
        }
    }
    return tg_set_result(i,saved,TG_OK);
}
int tg_control_refresh(tg_interp *i,size_t n,char **a) {
    const char *path=NULL;
    int created=n>=2&&creation(a[0]),configured=configuration(n,a);
    if(created) path=a[1];
    else if(configured) path=a[0];
    else if(n>=2&&a[0][0]=='.') {
        tg_widget *w=tg_lookup(i->ui,tg_ui_find(i->ui,a[0]));
        if(w&&w->kind==TG_MENU&&(!strcmp(a[1],"add")||!strcmp(a[1],"insert")||
            (n>=5&&(!strcmp(a[1],"entryconfigure")||!strcmp(a[1],"entryconfig"))))) return menu_refresh(i,w->id);
        if(w&&w->kind==TG_SCALE&&!strcmp(a[1],"set")&&n==3) return scale_store_var(i,w);
        if((!strcmp(a[1],"insert")&&n==4)||(!strcmp(a[1],"delete")&&(n==3||n==4))||
           (!strcmp(a[1],"set")&&n==3)||(!strcmp(a[1],"current")&&n==3)) return publish(i,w);
        return TG_OK;
    }
    if(!path) return TG_OK;
    tg_widget *w=tg_lookup(i->ui,tg_ui_find(i->ui,path));
    if(!w) return TG_OK;
    if(w->kind==TG_SCALE&&w->variable[0]) {
        size_t k=tg_var_find(i,w->variable);
        if(k<i->count) (void)tg_scale_sync_widget(w,i->vars[k].value);
        else (void)scale_store_var(i,w);
    }
    if(w->kind==TG_PROGRESS&&w->variable[0]) {
        size_t k=tg_var_find(i,w->variable);
        if(k<i->count) (void)tg_indicator_option(w,"-value",i->vars[k].value);
        else if(tg_var_store(i,w->variable,w->value_text)!=TG_OK) return TG_ERROR;
    }
    if(tg_toggle_kind(w->kind)&& (w->variable[0]||w->kind==TG_RADIOBUTTON)) {
        size_t k=tg_var_find(i,w->variable);
        tg_toggle_sync(w,k<i->count?i->vars[k].value:NULL);
    }
    if(!w->textvariable[0]||!tg_textvar_kind(w->kind)) return TG_OK;
    size_t k=tg_var_find(i,w->textvariable);
    char saved[MAX_WORD],key[128],value[1024]; snprintf(saved,sizeof saved,"%s",i->result);
    strcpy(key,w->textvariable); strcpy(value,w->text);
    tg_id id=w->id;
    if(k<i->count) {
        tg_entry_validation_bound(i,id,key);
        k=tg_var_find(i,key);
        w=tg_lookup(i->ui,id);
        if(k<i->count&&w&&tg_textvar_matches(w,key))
            (void)tg_textvar_sync(i->ui,key,i->vars[k].value);
        return tg_set_result(i,saved,TG_OK);
    }
    if(tg_var_store(i,key,value)!=TG_OK) {
        w=tg_lookup(i->ui,id);
        if(w&&created) (void)tg_ui_remove(i->ui,id);
        else if(w&&tg_textvar_matches(w,key)) w->textvariable[0]=0;
        return TG_ERROR;
    }
    return tg_set_result(i,saved,TG_OK);
}
static int activate(tg_interp *i,tg_id id) {
    tg_widget *w=tg_lookup(i->ui,id);
    if(!w) return TG_ERROR;
    if(w->state==TG_STATE_DISABLED) return tg_set_result(i,"",TG_OK);
    char script[sizeof w->command];
    if(tg_ui_button_script(i->ui,id,script,sizeof script)!=TG_OK) return TG_ERROR;
    if(tg_toggle_kind(w->kind)) {
        const char *value=w->kind==TG_CHECKBUTTON&&w->checked?w->off_value:w->on_value;
        if(w->kind==TG_CHECKBUTTON&&!w->variable[0]) tg_toggle_sync(w,value);
        else if(tg_var_store(i,w->variable,value)!=TG_OK) return TG_ERROR;
        if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
    }
    return invoke(i,script);
}
int tg_control_invoke(tg_interp *i,size_t n,char **a) {
    if(n==3&&a[0][0]=='.'&&!strcmp(a[1],"invoke")) {
        const tg_widget *m=tg_ui_get(i->ui,tg_ui_find(i->ui,a[0])); tg_menu_action action;
        int rc=tg_menu_prepare_action(m,a[2],&action);
        if(rc>=0) return rc==TG_OK?menu_action(i,&action):tg_set_result(i,"invalid menu item",TG_ERROR);
    }
    if(n!=2||strcmp(a[1],"invoke")||a[0][0]!='.') return -1;
    tg_id id=tg_ui_find(i->ui,a[0]);
    const tg_widget *w=tg_ui_get(i->ui,id);
    if(!w||(w->kind!=TG_BUTTON&&!tg_toggle_kind(w->kind))) return -1;
    return activate(i,id);
}
/* Deliver one changed scrollbar notification outside layout/paint. Mark the
   value before calling user code; callbacks can mutate or destroy widgets. */
int tg_interp_dispatch_scroll(tg_interp *i) {
    if(!i||i->disposing||(i->depth&&!i->event_pump_depth)) return 0;
    for(size_t k=1;k<i->ui->count;k++) {
        tg_widget *w=&i->ui->items[k];
        if((w->kind!=TG_CANVAS&&w->kind!=TG_TEXT&&w->kind!=TG_TREEVIEW&&w->kind!=TG_LISTBOX)||w->rect.w<=0||w->rect.h<=0) continue;
        for(int y=w->kind==TG_LISTBOX?1:0;y<2;y++) {
            const char *prefix=y?w->canvas_yscroll:w->canvas_xscroll;
            if(!*prefix) continue;
            unsigned mask=1u<<y; double first,last;
            if(w->kind==TG_TEXT) {
                if(y) tg_document_view_fractions(w,&first,&last);
                else tg_document_xview_fractions(w,&first,&last);
            }
            else if(w->kind==TG_LISTBOX) tg_listbox_view_fractions(w,&first,&last);
            else if(w->kind==TG_TREEVIEW) tg_tree_view_fractions(i->ui,w,y,&first,&last);
            else tg_canvas_view_fractions(w,y,&first,&last);
            if((w->canvas_scroll_sent&mask)&&w->canvas_scroll_last[y*2]==first&&
               w->canvas_scroll_last[y*2+1]==last) continue;
            char script[512],word[32];
            strcpy(script,prefix);
            snprintf(word,sizeof word,"%.9g",first);
            if(!tg_list_append(script,sizeof script,word)) continue;
            snprintf(word,sizeof word,"%.9g",last);
            if(!tg_list_append(script,sizeof script,word)) continue;
            w->canvas_scroll_sent|=mask;
            w->canvas_scroll_last[y*2]=first; w->canvas_scroll_last[y*2+1]=last;
            if(invoke(i,script)!=TG_OK) {
                char error[MAX_WORD]; snprintf(error,sizeof error,"%s",i->result);
                (void)tg_var_store(i,"errorInfo",error);
            }
            return 1;
        }
    }
    return 0;
}
/* One outstanding native composition commit per interpreter. Post into the
   same event queue as keyboard input; reject stale text or lost focus. */
static const char *ime_text(const tg_widget *w,size_t *length) {
    size_t cursor;
    if(!w||w->state!=TG_STATE_NORMAL) return NULL;
    if(w->kind==TG_TEXT) return tg_document_input_text(w,length,&cursor);
    if(!tg_textvar_editable(w->kind)) return NULL;
    *length=strlen(w->text); return w->text;
}
int tg_ime_queue(tg_interp *i,tg_id id,size_t first,size_t last,const char *expected,size_t length,const char *text) {
    if(!i||i->disposing||i->ime_pending||!expected||!text||first>last||last>length||
       length>1024*1024) return TG_ERROR;
    size_t n=strlen(text),actual=0,count;
    if(n>1024*1024||tg_utf8_count((const unsigned char *)expected,first,&count)!=TG_OK||
       tg_utf8_count((const unsigned char *)expected,last,&count)!=TG_OK||
       tg_utf8_count((const unsigned char *)text,n,&count)!=TG_OK) return TG_ERROR;
    const char *current=ime_text(tg_ui_get(i->ui,id),&actual);
    if(!current||actual!=length||memcmp(current,expected,length)||tg_ui_focused(i->ui)!=id) return TG_ERROR;
    tg_ime_edit *p=malloc(sizeof *p+length+1+n+1); if(!p) return TG_ERROR;
    p->target=id; p->first=first; p->last=last; p->length=length;
    memcpy(p->bytes,expected,length); p->bytes[length]=0; memcpy(p->bytes+length+1,text,n+1);
    i->ime_pending=p;
    if(tg_ui_post(i->ui,(tg_event){TG_IME_REPLACE,0,0,0,0})!=TG_OK) {
        i->ime_pending=NULL; free(p); return TG_ERROR;
    }
    return TG_OK;
}
static void ime_dispatch(tg_interp *i) {
    tg_ime_edit *p=i->ime_pending; i->ime_pending=NULL;
    if(!p) return;
    tg_id id=p->target; tg_widget *w=tg_lookup(i->ui,id); size_t length=0;
    const char *old=ime_text(w,&length),*text=p->bytes+p->length+1;
    if(!old||length!=p->length||memcmp(old,p->bytes,length)||tg_ui_focused(i->ui)!=id) goto done;
    if(w->kind==TG_TEXT) {
        if(tg_document_replace_bytes(w,p->first,p->last,text)!=TG_OK) goto done;
    } else {
        tg_widget candidate=*w;
        if(tg_entry_replace_bytes(&candidate,p->first,p->last,text)!=TG_OK) goto done;
        size_t index; char removed[1024]; const char *change=text;
        if(!*text) {
            size_t n=p->last-p->first; if(n>=sizeof removed) goto done;
            memcpy(removed,w->text+p->first,n); removed[n]=0; change=removed;
        }
        if(tg_utf8_count((const unsigned char *)old,p->first,&index)!=TG_OK||
           !tg_entry_validation_replace(i,id,candidate.text,change,*text?1:0,(int)index)) goto done;
        w=tg_lookup(i->ui,id);
        if(!w||w->state!=TG_STATE_NORMAL||strcmp(w->text,p->bytes)||
           tg_entry_replace_bytes(w,p->first,p->last,text)!=TG_OK) goto done;
        (void)publish(i,w);
    }
    if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
done:
    free(p);
}
int tg_interp_dispatch_input(tg_interp *i) {
    if(!i||(i->depth&&!i->event_pump_depth)||i->disposing) return 0;
    tg_event e;
    if(!tg_ui_poll(i->ui,&e)) return 0;
    if(e.kind==TG_IME_REPLACE) { ime_dispatch(i); return 1; }
    char menu_script[512]="";
    int menu=tg_menu_input(i->ui,e,menu_script,sizeof menu_script);
    if(menu>=0) {
        if(menu&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
        if(i->ui->menu_activation) {
            tg_menu_action action;
            tg_id id=i->ui->menu_activation; unsigned row=i->ui->menu_activation_row;
            i->ui->menu_activation=0;
            if(tg_menu_prepare_row(tg_ui_get(i->ui,id),row,&action)==TG_OK) (void)menu_action(i,&action);
        } else if(menu_script[0]) (void)invoke(i,menu_script);
        return 1;
    }
    tg_id selected;
    int popup=tg_choice_popup_input(i->ui,e,&selected);
    if(popup>=0) {
        if(selected) (void)publish(i,tg_lookup(i->ui,selected));
        if(popup&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return 1;
    }
    if((e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP||e.kind==TG_TEXT_INPUT)&&(e.key&TG_NATIVE_KEY_EVENT)) {
        e.key&=~TG_NATIVE_KEY_EVENT;
        tg_id focus=tg_ui_focused(i->ui);
        const tg_widget *p=tg_ui_get(i->ui,focus);
        while(p&&p->kind!=TG_TOPLEVEL) p=tg_ui_get(i->ui,p->parent);
        if(p&&p->id!=e.target) return 1;
        if(p) e.target=focus;
    }
    int consumed=0;
    tg_id drag=i->ui->dialog_capture;
    int dialog_changed=tg_base_dialog_event(i->ui,e,&consumed);
    if(dialog_changed==2&&i->tk_repaint) i->tk_repaint(i->tk_host,drag);
    else if(dialog_changed&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
    if(consumed) return 1;
    if(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_EXIT) {
        tg_id previous=i->ui->hover,next=e.kind==TG_POINTER_EXIT?0:e.target;
        if(previous!=next) {
            i->ui->hover=next;
            if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
            if(previous&&tg_ui_get(i->ui,previous)) {
                tg_event leave={TG_LEAVE,previous,e.x,e.y,0};
                (void)tg_binding_dispatch(i,leave);
            }
            if(next&&tg_ui_get(i->ui,next)) {
                tg_event enter={TG_ENTER,next,e.x,e.y,0};
                (void)tg_binding_dispatch(i,enter);
            }
        }
        if(e.kind==TG_POINTER_EXIT) return 1;
    }
    if(e.kind==TG_FOCUS_IN||e.kind==TG_FOCUS_OUT) tg_entry_validation_focus(i,e.target,e.kind==TG_FOCUS_IN);
    tg_id prev_menu=i->ui->menu_posted;
    if(tg_binding_dispatch(i,e)||TG_IS_VIRTUAL(e.kind)) return 1;
    if(!prev_menu&&i->ui->menu_posted) {
        if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return 1;
    }
    if(e.target&&!tg_ui_get(i->ui,e.target)) return 1;
    tg_id trigger=tg_choice_trigger(i->ui,e);
    if(trigger) {
        tg_widget *w=tg_lookup(i->ui,trigger); char command[256];
        strcpy(command,w->choice_postcommand);
        if(invoke(i,command)==TG_OK&&tg_choice_open(i->ui,trigger)&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return 1;
    }
    char script[512];
    tg_id old_focus=tg_ui_focused(i->ui);
    if(!tg_entry_validation_input(i,e)) return 1;
    int changed=tg_ui_apply_event(i->ui,e,script,sizeof script);
    tg_id new_focus=tg_ui_focused(i->ui);
    if(new_focus!=old_focus) {
        tg_entry_validation_focus(i,old_focus,0);
        tg_entry_validation_focus(i,new_focus,1);
        if(old_focus&&tg_ui_get(i->ui,old_focus)) (void)tg_binding_dispatch(i,(tg_event){TG_FOCUS_OUT,old_focus,0,0,0});
        if(new_focus&&tg_ui_get(i->ui,new_focus)) (void)tg_binding_dispatch(i,(tg_event){TG_FOCUS_IN,new_focus,0,0,0});
    }
    tg_widget *focused=tg_lookup(i->ui,tg_ui_focused(i->ui));
    if(changed&&(e.kind==TG_KEY_DOWN||e.kind==TG_TEXT_INPUT||
        (e.kind==TG_POINTER_DOWN&&focused&&focused->kind==TG_SPINBOX)))
        (void)publish(i,focused);
    tg_id activated=i->ui->activated; i->ui->activated=0;
    tg_id scale_changed=i->ui->scale_changed; i->ui->scale_changed=0;
    if(changed&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
    if(scale_changed) (void)scale_publish(i,scale_changed);
    if(activated) (void)activate(i,activated);
    else if(script[0]) (void)invoke(i,script);
    return 1;
}
