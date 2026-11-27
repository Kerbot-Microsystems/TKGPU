#include "TKGPUUi.H"
#include "../../src/widgets/controls/TKGPUControls.H"
#include "../../src/widgets/TKGPUWidgetInternal.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u);
    tg_id b=tg_ui_add(u,".b",TG_BUTTON,"OK"), e=tg_ui_add(u,".e",TG_ENTRY,NULL);
    CHECK(b&&e); tg_widget *button=tg_lookup(u,b),*entry=tg_lookup(u,e);
    CHECK(tg_control_option(button,"-command","incr clicks")==TG_OK);
    char script[512],result[256];
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,b,0,0,0},script,sizeof script)&&!*script);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_UP,e,0,0,0},script,sizeof script)&&!*script);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,b,0,0,0},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_UP,b,0,0,0},script,sizeof script)&&!strcmp(script,"incr clicks"));
    CHECK(tg_control_option(button,"-state","disabled")==TG_OK);
    CHECK(tg_ui_button_script(u,b,script,sizeof script)==TG_OK&&!*script);
    char *insert[]={".e","insert","0","A\xc3\xa9Z"};
    CHECK(tg_entry_command(entry,4,insert,result,sizeof result)==TG_OK&&entry->cursor==3);
    CHECK(tg_ui_focus(u,e)==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_LEFT},script,sizeof script));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,0,0,0,TG_KEY_BACKSPACE},script,sizeof script));
    CHECK(!strcmp(entry->text,"AZ")&&entry->cursor==1);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_TEXT_INPUT,0,0,0,'B'},script,sizeof script));
    CHECK(!strcmp(entry->text,"ABZ"));
    CHECK(tg_control_option(entry,"-state","readonly")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_TEXT_INPUT,0,0,0,'X'},script,sizeof script)&&!strcmp(entry->text,"ABZ"));
    CHECK(tg_ui_remove(u,e)==TG_OK&&tg_ui_focused(u)==0);
    tg_id scroll=tg_ui_add(u,".scroll",TG_SCROLLBAR,NULL); CHECK(scroll);
    tg_widget *sw=tg_lookup(u,scroll);
    sw->rect=(tg_rect){0,0,16,100}; sw->first=0.2; sw->last=0.4;
    CHECK(tg_control_option(sw,"-command",".canvas yview")==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,scroll,5,25,0},script,sizeof script)&&!*script);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_MOVE,0,5,55,0},script,sizeof script)&&!strcmp(script,".canvas yview moveto 0.5"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_UP,0,5,55,0},script,sizeof script)&&!*script);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,scroll,5,80,0},script,sizeof script)&&!strcmp(script,".canvas yview scroll 1 pages"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,scroll,5,25,0},script,sizeof script));
    CHECK(tg_ui_remove(u,scroll)==TG_OK&&!u->drag_scroll);
    tg_id tree_id=tg_ui_add(u,".tree",TG_TREEVIEW,NULL);
    tg_widget *tree=tg_lookup(u,tree_id); CHECK(tree);
    tree->rect=(tg_rect){10,10,200,180};
    char *columns[]={".tree","configure","-columns","a b","-show","headings"};
    CHECK(tg_ui_command(u,6,columns,result,sizeof result)==TG_OK);
    char *column[]={".tree","column","a","-width","100","-minwidth","40","-stretch","0"};
    CHECK(tg_ui_command(u,9,column,result,sizeof result)==TG_OK);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,tree_id,109,15,1},script,sizeof script));
    CHECK(u->drag_tree==tree_id);
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_MOVE,0,159,15,0},script,sizeof script));
    char *query[]={".tree","column","a","-width"};
    CHECK(tg_ui_command(u,4,query,result,sizeof result)==TG_OK&&!strcmp(result,"150"));
    CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_UP,0,-400,15,1},script,sizeof script));
    CHECK(!u->drag_tree);
    CHECK(tg_ui_command(u,4,query,result,sizeof result)==TG_OK&&!strcmp(result,"40"));
    tg_ui_destroy(u); puts("button activation, UTF-8 indices, entry editing and focus passed"); return 0;
}
