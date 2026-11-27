#include "TKGPUControls.H"
#include "../listbox/TKGPUListbox.H"
#include "../text/TKGPUTextWidget.H"
#include "../ttk/TKGPUChoice.H"
#include "../ttk/TKGPUNotebook.H"
#include "../ttk/TKGPUToggle.H"
#include "../ttk/TKGPUContainers.H"
#include "../ttk/TKGPUExtra.H"
#include "../ttk/tree/TKGPUTree.H"
#include "../TKGPUWidgetInternal.H"
#include <string.h>
#include <stdio.h>
#include "TKGPUUtf8.H"
static tg_event_kind virtual_event_kind(const char *name) {
    uint32_t h=0x55555555;
    for(size_t k=0;name[k];k++) h=(h*33)^(unsigned char)name[k];
    return (tg_event_kind)(0x30000000|(h&0x000fffff));
}
static int post_virtual(tg_ui *u,tg_id id,const char *name) {
    return tg_ui_post(u,(tg_event){virtual_event_kind(name),id,0,0,0})==TG_OK;
}
int tg_textvar_editable(tg_kind k) { return k==TG_ENTRY||tg_choice_kind(k); }
int tg_textvar_kind(tg_kind k) {
    return tg_textvar_editable(k)||k==TG_LABEL||k==TG_BUTTON||k==TG_MESSAGE||
        k==TG_MENUBUTTON||tg_toggle_kind(k);
}
static const char *global_name(const char *s) { return s[0]==':'&&s[1]==':'?s+2:s; }
int tg_textvar_matches(const tg_widget *w,const char *key) {
    return tg_textvar_kind(w->kind)&&w->textvariable[0]&&!strcmp(global_name(w->textvariable),global_name(key));
}
int tg_textvar_valid(const char *value) {
    size_t n;
    return strlen(value)<sizeof(((tg_widget *)0)->text)&&
        tg_utf8_count((const unsigned char *)value,strlen(value),&n)==TG_OK;
}
int tg_textvar_sync(tg_ui *u,const char *key,const char *value) {
    int changed=0;
    if(!value||!tg_textvar_valid(value)) return 0;
    size_t chars=0;
    (void)tg_utf8_count((const unsigned char *)value,strlen(value),&chars);
    for(size_t k=1;k<u->count;k++) {
        tg_widget *w=&u->items[k];
        if(!tg_textvar_matches(w,key)||!strcmp(w->text,value)) continue;
        strcpy(w->text,value);
        if(w->cursor>chars) w->cursor=chars;
        if(w->sel_first>chars) w->sel_first=chars;
        if(w->sel_last>chars) w->sel_last=chars;
        if(w->sel_anchor>chars) w->sel_anchor=chars;
        changed=1;
    }
    return changed;
}
int tg_control_option(tg_widget *w,const char *name,const char *value) {
    if(!strcmp(name,"-textvariable")&&tg_textvar_kind(w->kind)) {
        const char *key=global_name(value);
        if(strlen(value)>=sizeof w->textvariable||(*value&&(!*key||strstr(key,"::")))) return TG_ERROR;
        strcpy(w->textvariable,value); return TG_OK;
    }
    if(!strcmp(name,"-command")&&(w->kind==TG_BUTTON||w->kind==TG_SCROLLBAR||tg_toggle_kind(w->kind))) {
        if(strlen(value)>=sizeof w->command) return TG_ERROR;
        strcpy(w->command,value); return TG_OK;
    }
    if(!strcmp(name,"-state")&&(w->kind==TG_BUTTON||w->kind==TG_SCROLLBAR||w->kind==TG_MENUBUTTON||w->kind==TG_LISTBOX||w->kind==TG_TEXT||tg_toggle_kind(w->kind)||w->kind==TG_ENTRY||tg_choice_kind(w->kind))) {
        if(!strcmp(value,"normal")) w->state=TG_STATE_NORMAL;
        else if(!strcmp(value,"disabled")) w->state=TG_STATE_DISABLED;
        else if((w->kind==TG_ENTRY||tg_choice_kind(w->kind))&&!strcmp(value,"readonly")) w->state=TG_STATE_READONLY;
        else return TG_ERROR;
        /* -state configuration replaces the disabled/readonly override. */
        w->ttk_state_set&=~(2u|32u); w->ttk_state_clear&=~(2u|32u);
        return TG_OK;
    }
    return -1;
}
int tg_control_query(const tg_widget *w,const char *name,char *out,size_t cap) {
    const char *s=NULL;
    if(!strcmp(name,"-textvariable")&&tg_textvar_kind(w->kind)) s=w->textvariable;
    if(!strcmp(name,"-command")&&(w->kind==TG_BUTTON||w->kind==TG_SCROLLBAR||tg_toggle_kind(w->kind))) s=w->command;
    if(!strcmp(name,"-state")&&(w->kind==TG_BUTTON||w->kind==TG_SCROLLBAR||w->kind==TG_MENUBUTTON||w->kind==TG_LISTBOX||w->kind==TG_TEXT||tg_toggle_kind(w->kind)||w->kind==TG_ENTRY||tg_choice_kind(w->kind)))
        s=w->state==TG_STATE_DISABLED?"disabled":w->state==TG_STATE_READONLY?"readonly":"normal";
    if(!s) return -1;
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return TG_OK;
}
int tg_ui_button_script(const tg_ui *u,tg_id id,char *out,size_t cap) {
    const tg_widget *w=tg_ui_get(u,id);
    if(!w||(w->kind!=TG_BUTTON&&!tg_toggle_kind(w->kind))||!out||!cap) return TG_ERROR;
    const char *s=w->state==TG_STATE_DISABLED?"":w->command;
    if(strlen(s)>=cap) return TG_ERROR;
    strcpy(out,s); return TG_OK;
}
int tg_ui_focus(tg_ui *u,tg_id id) {
    if(!u||(id&&!tg_ui_get(u,id))) return TG_ERROR;
    tg_id previous=u->focus;
    if(previous==id) return TG_OK;
    u->focus=id;
    /* Programmatic focus (focus_set/focus_force) must generate the same
       widget notifications as pointer and keyboard traversal. */
    if(u->native_toplevels) {
        if(previous&&tg_ui_get(u,previous))
            (void)tg_ui_post(u,(tg_event){TG_FOCUS_OUT,previous,0,0,0});
        if(id)
            (void)tg_ui_post(u,(tg_event){TG_FOCUS_IN,id,0,0,0});
    }
    return TG_OK;
}
tg_id tg_ui_focused(const tg_ui *u) { return u?u->focus:0; }
static int scrollbar_input(tg_ui *u,tg_event e,char *script,size_t cap) {
    tg_widget *w=tg_lookup(u,u->drag_scroll?u->drag_scroll:e.target);
    if(u->drag_scroll&&(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_UP)) {
        if(e.kind==TG_POINTER_UP||!w||w->kind!=TG_SCROLLBAR||w->state==TG_STATE_DISABLED) {
            u->drag_scroll=0; return 1;
        }
        int extent=w->vertical?w->rect.h:w->rect.w;
        int pos=w->vertical?e.y-w->rect.y:e.x-w->rect.x;
        if(extent>0&&w->command[0]) {
            double fraction=(double)(pos-u->drag_scroll_offset)/extent;
            double limit=1.0-(w->last-w->first);
            if(limit<0) limit=0;
            if(fraction<0) fraction=0;
            if(fraction>limit) fraction=limit;
            int z=snprintf(script,cap,"%s moveto %.9g",w->command,fraction);
            if(z<0||(size_t)z>=cap) script[0]=0;
        }
        return 1;
    }
    if(e.kind!=TG_POINTER_DOWN||!w||w->kind!=TG_SCROLLBAR||w->state==TG_STATE_DISABLED) return -1;
    int extent=w->vertical?w->rect.h:w->rect.w;
    if(extent<=0) return 1;
    int pos=w->vertical?e.y-w->rect.y:e.x-w->rect.x;
    int first=(int)(extent*w->first),last=(int)(extent*w->last);
    if(first>=extent) first=extent-1;
    if(last<=first) last=first+1;
    u->pressed=0;
    if(pos>=first&&pos<last) {
        u->drag_scroll=w->id; u->drag_scroll_offset=pos-first;
    } else if(w->command[0]) {
        int z=snprintf(script,cap,"%s scroll %d pages",w->command,pos<first?-1:1);
        if(z<0||(size_t)z>=cap) script[0]=0;
    }
    return 1;
}
static int scale_input(tg_ui *u,tg_event e) {
    tg_widget *w=tg_lookup(u,u->drag_scale?u->drag_scale:e.target);
    if(u->drag_scale&&(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_UP)) {
        if(!w||w->kind!=TG_SCALE||w->state==TG_STATE_DISABLED) {
            u->drag_scale=0; return 1;
        }
        int changed=tg_scale_set_at(w,e.x,e.y,u->drag_scale_offset);
        if(changed) u->scale_changed=w->id;
        if(e.kind==TG_POINTER_UP) u->drag_scale=0;
        return 1;
    }
    if(e.kind!=TG_POINTER_DOWN||(e.key&255u)>1u||!w||w->kind!=TG_SCALE||w->state==TG_STATE_DISABLED) return -1;
    int extent=w->vertical?w->rect.h:w->rect.w;
    if(extent<=0) return 1;
    int thumb=tg_scale_thumb_size(w);
    double f=tg_scale_fraction(w);
    int travel=extent-thumb;
    int start=travel>0?(int)(travel*f+0.5):0;
    int pos=w->vertical?(e.y-w->rect.y):(e.x-w->rect.x);
    u->pressed=0;
    u->focus=w->id;
    u->drag_scale=w->id;
    u->drag_scale_offset=(pos>=start&&pos<start+thumb)?(pos-start):(thumb/2);
    if(tg_scale_set_at(w,e.x,e.y,u->drag_scale_offset)) u->scale_changed=w->id;
    return 1;
}
int tg_ui_apply_event(tg_ui *u,tg_event e,char *script,size_t cap) {
    if(!u||!script||!cap) return 0;
    script[0]=0; u->activated=0; u->scale_changed=0;
    int menu=tg_menu_input(u,e,script,cap);
    if(menu>=0) return menu;
    tg_id selected;
    int popup=tg_choice_popup_input(u,e,&selected);
    if(popup>=0) return popup;
    tg_id trigger=tg_choice_trigger(u,e);
    if(trigger) return tg_choice_open(u,trigger);
    unsigned modifiers=0;
    if(e.kind==TG_KEY_DOWN||e.kind==TG_KEY_UP) { modifiers=e.key&~TG_KEY_VALUE_MASK; e.key&=TG_KEY_VALUE_MASK; }
    if(e.kind==TG_POINTER_MOVE&&u->entry_drag) {
        tg_widget *entry=tg_lookup(u,u->entry_drag); if(!entry) u->entry_drag=0;
        else if(entry->kind==TG_TEXT) { int r=tg_document_drag(entry,e.x,e.y); if(r==2) (void)post_virtual(u,entry->id,"Selection"); return r!=0; }
        else return tg_entry_drag(entry,e.x);
    }
    if(e.kind==TG_POINTER_UP||e.kind==TG_FOCUS_OUT) {
        u->entry_drag=0;
        if(e.kind==TG_FOCUS_OUT) u->drag_scale=0;
    }
    if(e.kind==TG_KEY_DOWN) {
        if(tg_notebook_traverse(u,e.key,modifiers)) return 1;
        tg_widget *notebook=tg_lookup(u,u->focus);
        if(notebook&&notebook->kind==TG_NOTEBOOK) {
            if(e.key==TG_KEY_LEFT||e.key==TG_KEY_RIGHT)
                return tg_notebook_step(u,notebook,e.key==TG_KEY_LEFT?-1:1);
            if(e.key==TG_KEY_TAB&&(modifiers&TG_MOD_CONTROL))
                return tg_notebook_step(u,notebook,(modifiers&TG_MOD_SHIFT)?-1:1);
        }
    }
    if(e.kind==TG_KEY_DOWN&&e.key==TG_KEY_TAB) {
        size_t start=0; for(size_t k=0;k<u->count;k++) if(u->items[k].id==u->focus) start=k;
        for(size_t step=1;step<=u->count;step++) {
            size_t k=(modifiers&TG_MOD_SHIFT)?(start+u->count-step)%u->count:(start+step)%u->count;
            tg_widget *candidate=&u->items[k];
            if(candidate->state==TG_STATE_DISABLED||candidate->takefocus==0||candidate->rect.w<=0||candidate->rect.h<=0) continue;
            if(candidate->kind!=TG_ENTRY&&candidate->kind!=TG_BUTTON&&candidate->kind!=TG_NOTEBOOK&&candidate->kind!=TG_SCALE&&!tg_choice_kind(candidate->kind)&&!tg_toggle_kind(candidate->kind)&&candidate->takefocus!=1) continue;
            const tg_widget *parent=candidate;
            while(parent&&parent->id!=1&&parent->kind!=TG_TOPLEVEL) parent=tg_ui_get(u,parent->parent);
            if(u->view_root&&(!parent||parent->id!=u->view_root)) continue;
            u->focus=candidate->id; return 1;
        }
        return 1;
    }
    if(u->drag_tree) {
        tg_widget *tree=tg_lookup(u,u->drag_tree);
        if(!tree||e.kind==TG_FOCUS_OUT) u->drag_tree=0;
        else if(e.kind==TG_POINTER_MOVE||e.kind==TG_POINTER_UP) {
            int changed=tg_tree_resize(u,tree,1,e.x,e.y);
            if(e.kind==TG_POINTER_UP) u->drag_tree=0;
            return changed;
        }
    }
    int hover_changed=0;
    if(e.kind==TG_POINTER_MOVE) { hover_changed=u->hover!=e.target; u->hover=e.target; }
    int handled=tg_container_input(u,e,script,cap); if(handled>=0) return handled||hover_changed;
    handled=scrollbar_input(u,e,script,cap); if(handled>=0) return handled||hover_changed;
    handled=scale_input(u,e); if(handled>=0) return handled||hover_changed;
    tg_widget *w=tg_lookup(u,e.target);
    if(e.kind==TG_POINTER_DOWN&&(e.key&255u)<=1u) {
        u->pressed=0;
        if(w&&w->kind==TG_TEXT) { u->focus=w->id; u->entry_drag=w->id; int r=tg_document_click(w,e.x,e.y); if(r==2) (void)post_virtual(u,w->id,"Selection"); return r!=0; }
        if(w&&w->kind==TG_LISTBOX) { if(w->state!=TG_STATE_DISABLED) { u->focus=w->id; int ch=tg_listbox_click(w,e.x,e.y); if(ch) (void)post_virtual(u,w->id,"ListboxSelect"); return ch; } return 1; }
        if(w&&w->kind==TG_TREEVIEW) { if(tg_tree_resize(u,w,0,e.x,e.y)) { u->drag_tree=w->id; return 1; } int ch=tg_tree_click(u,w,e.x,e.y); if(ch) (void)post_virtual(u,w->id,"TreeviewSelect"); return ch; }
        if(w&&w->kind==TG_NOTEBOOK) return tg_notebook_click(u,w,e.x,e.y);
        if(w&&w->kind==TG_SPINBOX&&w->state!=TG_STATE_DISABLED&&e.x>=w->rect.x+w->rect.w-13&&e.x<w->rect.x+w->rect.w) {
            u->focus=w->id;
            int changed=tg_choice_step(w,e.y<w->rect.y+w->rect.h/2?1:-1);
            if(changed&&strlen(w->command)<cap) strcpy(script,w->command);
            return changed;
        }
        if(w&&(w->kind==TG_BUTTON||w->kind==TG_MENUBUTTON||tg_toggle_kind(w->kind)||w->kind==TG_ENTRY||tg_choice_kind(w->kind))&&w->state!=TG_STATE_DISABLED) {
            u->focus=w->id;
            if(tg_textvar_editable(w->kind)) { (void)tg_entry_click(w,e.x); u->entry_drag=w->id; }
            if(w->kind==TG_BUTTON||tg_toggle_kind(w->kind)) u->pressed=w->id;
        }
        return 1;
    }
    if(e.kind==TG_POINTER_UP) {
        tg_id pressed=u->pressed; u->pressed=0;
        if(pressed&&pressed==e.target) {
            if(tg_ui_button_script(u,pressed,script,cap)==TG_OK) u->activated=pressed;
        }
        return 1;
    }
    w=tg_lookup(u,u->focus);
    if(w&&w->kind==TG_TEXT&&(e.kind==TG_TEXT_INPUT||e.kind==TG_KEY_DOWN)) return tg_document_key(w,e.kind==TG_TEXT_INPUT?e.key:(e.key|modifiers),e.kind==TG_TEXT_INPUT);
    if(e.kind==TG_KEY_DOWN&&w&&w->kind==TG_SCALE&&w->state!=TG_STATE_DISABLED) {
        int ch=0;
        if(e.key==TG_KEY_LEFT||e.key==TG_KEY_ARROW_DOWN) ch=tg_scale_step(w,-1.0);
        else if(e.key==TG_KEY_RIGHT||e.key==TG_KEY_ARROW_UP) ch=tg_scale_step(w,1.0);
        else if(e.key==TG_KEY_PRIOR) ch=tg_scale_step(w,-10.0);
        else if(e.key==TG_KEY_NEXT) ch=tg_scale_step(w,10.0);
        else if(e.key==TG_KEY_HOME) ch=tg_scale_set_value(w,w->minimum);
        else if(e.key==TG_KEY_END) ch=tg_scale_set_value(w,w->maximum);
        if(ch) { u->scale_changed=w->id; return 1; }
    }
    if(e.kind==TG_KEY_DOWN&&w&&w->kind==TG_LISTBOX&&(e.key==TG_KEY_ARROW_UP||e.key==TG_KEY_ARROW_DOWN)) { int ch=tg_listbox_step(w,e.key==TG_KEY_ARROW_UP?-1:1); if(ch) (void)post_virtual(u,w->id,"ListboxSelect"); return ch; }
    if(e.kind==TG_KEY_DOWN&&(e.key==TG_KEY_ARROW_UP||e.key==TG_KEY_ARROW_DOWN)&&w&&tg_choice_kind(w->kind)) {
        int changed=tg_choice_step(w,e.key==TG_KEY_ARROW_UP?1:-1);
        if(changed&&w->kind==TG_SPINBOX&&strlen(w->command)<cap) strcpy(script,w->command);
        if(changed&&w->kind==TG_COMBOBOX) (void)post_virtual(u,w->id,"ComboboxSelected");
        return changed;
    }
    if(e.kind==TG_TEXT_INPUT&&e.key==32&&w&&tg_toggle_kind(w->kind)) {
        if(tg_ui_button_script(u,w->id,script,cap)==TG_OK) u->activated=w->id;
        return 1;
    }
    if(e.kind==TG_TEXT_INPUT&&e.key>=32&&e.key<=0x10ffff) return tg_entry_edit(w,e.key);
    if(e.kind==TG_KEY_DOWN&&e.key>=TG_KEY_LEFT&&e.key<=TG_KEY_DELETE) return (modifiers&TG_MOD_SHIFT)&&e.key<=TG_KEY_END?tg_entry_shift(w,e.key):tg_entry_edit(w,e.key|(modifiers&TG_MOD_CONTROL));
    if(e.kind==TG_KEY_DOWN&&e.key==TG_KEY_RETURN&&w&&w->kind==TG_BUTTON) {
        (void)tg_ui_button_script(u,w->id,script,cap); return 1;
    }
    return hover_changed;
}
