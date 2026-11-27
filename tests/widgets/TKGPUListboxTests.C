#include "TKGPUUi.H"
#include "../../src/render/async/TKGPUSnapshot.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define E(s,v) CHECK(tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v))
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("listbox .l; place .l -width 100 -height 48; .l insert end Alpha {Beta two} Gamma","");
    E(".l size","3"); E(".l index end","3"); E(".l get end","Gamma");
    E(".l get 0 end","Alpha {Beta two} Gamma");
    E(".l selection set 1 2; .l curselection","1 2");
    E(".l selection includes 0","0");
    E(".l insert 1 Delta; .l curselection","2 3");
    E(".l delete 0 1; .l get 0 end","{Beta two} Gamma");
    E(".l curselection","0 1");
    tg_ui *u=tg_interp_ui(i); CHECK(tg_ui_arrange(u,120,80)==TG_OK);
    tg_id id=tg_ui_find(u,".l"); CHECK(id);
    char script[64]; CHECK(tg_ui_apply_event(u,(tg_event){TG_POINTER_DOWN,id,5,14,0},script,sizeof script));
    E(".l curselection","1");
    CHECK(tg_ui_apply_event(u,(tg_event){TG_KEY_DOWN,id,0,0,TG_KEY_ARROW_UP},script,sizeof script));
    E(".l curselection","0");
    E(".l configure -state disabled; .l insert end hidden; .l size","2");
    E(".l configure -state normal; .l insert end C D E F G; .l see end","");
    E(".l nearest 0","3");
    tg_ui *copy=tg_snapshot_create(u); CHECK(copy);
    E(".l delete 0 end; .l size","0");
    char *args[]={".l","size"},out[64];
    CHECK(tg_ui_command(copy,2,args,out,sizeof out)==TG_OK&&!strcmp(out,"7"));
    tg_canvas *canvas=tg_canvas_create(120,80); CHECK(canvas);
    CHECK(tg_ui_paint(copy,canvas)==TG_OK);
    tg_canvas_destroy(canvas); tg_ui_destroy(copy);
    E("destroy .l; winfo exists .l","0");
    tg_destroy(i); return 0;
}
