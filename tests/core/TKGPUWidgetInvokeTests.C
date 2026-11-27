#include "TKGPUUi.H"
#include "TKGPUCommand.H"
#include "../../src/core/TKGPUCoreInternal.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct { int calls; char proposed[1024],change[1024],reason[32],action[8],index[16]; } validation_probe;
static int probe(void *ctx,tg_interp *i,size_t n,const char *const a[]) {
    validation_probe *p=ctx;
    if(n!=6||strlen(a[1])>=sizeof p->proposed||strlen(a[2])>=sizeof p->change||
       strlen(a[3])>=sizeof p->reason||strlen(a[4])>=sizeof p->action||strlen(a[5])>=sizeof p->index) return TG_ERROR;
    p->calls++; strcpy(p->proposed,a[1]); strcpy(p->change,a[2]); strcpy(p->reason,a[3]); strcpy(p->action,a[4]); strcpy(p->index,a[5]);
    return tg_command_result(i,"1");
}
static int ime_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"set accept 1; set value abcdef; entry .e -textvariable value -validate key -vcmd {set accept}; focus .e")==TG_OK);
    tg_id id=tg_ui_find(tg_interp_ui(i),".e");
    CHECK(tg_ime_queue(i,id,1,4,"abcdef",6,"XYZ")==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,"set value")==TG_OK&&!strcmp(tg_result(i),"aXYZef"));
    CHECK(tg_ime_queue(i,id,0,1,"aXYZef",6,"Q")==TG_OK);
    CHECK(tg_eval(i,"set accept 0")==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,"set value")==TG_OK&&!strcmp(tg_result(i),"aXYZef"));
    CHECK(tg_ime_queue(i,id,0,1,"aXYZef",6,"Q")==TG_OK);
    CHECK(tg_eval(i,"set value changed")==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,"set value")==TG_OK&&!strcmp(tg_result(i),"changed"));
    CHECK(tg_eval(i,"text .t -undo 1; .t insert end {abc def}; focus .t")==TG_OK);
    id=tg_ui_find(tg_interp_ui(i),".t");
    CHECK(tg_ime_queue(i,id,4,7,"abc def",7,"ghi")==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,".t get 1.0 {end - 1c}")==TG_OK&&!strcmp(tg_result(i),"abc ghi"));
    tg_destroy(i); return 0;
}
static int entry_validation_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"set allowed 1; set rejected 0; entry .v -validate key -vcmd {set allowed} -invcmd {incr rejected}; .v insert end abc; .v get")==TG_OK&&!strcmp(tg_result(i),"abc"));
    CHECK(tg_eval(i,"set allowed 0; .v delete 0 2; .v insert end X; .v get")==TG_OK&&!strcmp(tg_result(i),"abc"));
    CHECK(tg_eval(i,"set rejected")==TG_OK&&!strcmp(tg_result(i),"2"));
    CHECK(tg_eval(i,".v cget -validate")==TG_OK&&!strcmp(tg_result(i),"key"));
    CHECK(tg_eval(i,".v configure -validate none; .v insert end X; .v get")==TG_OK&&!strcmp(tg_result(i),"abcX"));
    CHECK(tg_eval(i,"ttk::entry .t -validate none -validatecommand {set allowed}; .t validate")==TG_OK&&!strcmp(tg_result(i),"0"));
    CHECK(tg_eval(i,".t instate invalid")==TG_OK&&!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,"set allowed 1; .t validate")==TG_OK&&!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,".t instate invalid")==TG_OK&&!strcmp(tg_result(i),"0"));
    CHECK(tg_eval(i,"set allowed 0; .v configure -validate key; focus .v")==TG_OK);
    tg_ui *u=tg_interp_ui(i); tg_id id=tg_ui_find(u,".v");
    CHECK(tg_ui_post(u,(tg_event){TG_TEXT_INPUT,id,0,0,'Q'})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,".v get")==TG_OK&&!strcmp(tg_result(i),"abcX"));
    CHECK(tg_eval(i,"set allowed 1")==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_TEXT_INPUT,id,0,0,'Z'})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,".v get")==TG_OK&&!strcmp(tg_result(i),"abcXZ"));
    CHECK(tg_eval(i,".v configure -validate broken")==TG_ERROR);
    CHECK(tg_eval(i,".v cget -validate")==TG_OK&&!strcmp(tg_result(i),"key"));
    CHECK(tg_eval(i,"set allowed 1; set value a; entry .classic -textvariable value -validate all -vcmd {set allowed}; ttk::entry .themed -textvariable value -validate all -vcmd {set allowed}; set allowed 0; set value b")==TG_OK);
    CHECK(tg_eval(i,".classic get")==TG_OK&&!strcmp(tg_result(i),"b"));
    CHECK(tg_eval(i,".classic cget -validate")==TG_OK&&!strcmp(tg_result(i),"none"));
    CHECK(tg_eval(i,".themed get")==TG_OK&&!strcmp(tg_result(i),"b"));
    CHECK(tg_eval(i,".themed cget -validate")==TG_OK&&!strcmp(tg_result(i),"all"));
    validation_probe p={0}; CHECK(tg_command_register(i,"probe",probe,&p,NULL)==TG_OK);
    CHECK(tg_eval(i,"entry .literal -validate key -vcmd {probe %P %S %V %d %i}")==TG_OK);
    const char *args[]={".literal","insert","0","a $ [literal] ; \" \\ \xc3\xa9"};
    CHECK(tg_call(i,4,args)==TG_OK);
    CHECK(p.calls==1&&!strcmp(p.proposed,args[3])&&!strcmp(p.change,args[3])&&!strcmp(p.reason,"key")&&!strcmp(p.action,"1")&&!strcmp(p.index,"0"));
    CHECK(tg_eval(i,".literal delete 0 1")==TG_OK);
    CHECK(p.calls==2&&!strcmp(p.change,"a")&&!strcmp(p.action,"0"));
    CHECK(tg_eval(i,"entry .focus -validate focusin -vcmd {probe %P %S %V %d %i}")==TG_OK);
    tg_id fid=tg_ui_find(u,".focus");
    CHECK(tg_ui_post(u,(tg_event){TG_FOCUS_IN,fid,0,0,0})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(p.calls==3&&!strcmp(p.reason,"focusin")&&!strcmp(p.action,"-1"));
    CHECK(tg_eval(i,"set initial value; entry .bound -textvariable initial -validate all -vcmd {probe %P %S %V %d %i}")==TG_OK);
    CHECK(p.calls==4&&!strcmp(p.reason,"forced")&&!strcmp(p.proposed,"value"));
    CHECK(tg_eval(i,"set other next; .bound configure -textvariable other")==TG_OK);
    CHECK(p.calls==5&&!strcmp(p.reason,"forced")&&!strcmp(p.proposed,"next"));
    CHECK(tg_eval(i,"ttk::entry .quiet -textvariable other -validate all -vcmd {probe %P %S %V %d %i}")==TG_OK);
    CHECK(p.calls==5);
    CHECK(tg_eval(i,"entry .gone -validate key -vcmd {eval {destroy .gone; return 1}}; .gone insert end abc; winfo exists .gone")==TG_OK&&!strcmp(tg_result(i),"0"));
    tg_destroy(i); return 0;
}
static int cascade_capture_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"set calls 0; set choice 0; entry .entry -validate key -vcmd {incr calls}; focus .entry; menu .menu -tearoff 0; menu .menu.sub -tearoff 0; .menu add cascade -label More -menu .menu.sub; .menu.sub add command -label Run -command {set choice 1}; .menu post 0 0; .menu activate 0")==TG_OK);
    tg_ui *u=tg_interp_ui(i); CHECK(tg_ui_arrange(u,640,300)==TG_OK);
    tg_id entry=tg_ui_find(u,".entry"),menu=tg_ui_find(u,".menu");
    CHECK(tg_ui_post(u,(tg_event){TG_TEXT_INPUT,entry,0,0,'X'})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,".entry get")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,"set calls")==TG_OK&&!strcmp(tg_result(i),"0"));
    CHECK(tg_ui_post(u,(tg_event){TG_KEY_DOWN,menu,0,0,TG_KEY_RIGHT})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_KEY_DOWN,menu,0,0,TG_KEY_RETURN})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,"set choice")==TG_OK&&!strcmp(tg_result(i),"1"));
    CHECK(tg_ui_post(u,(tg_event){TG_TEXT_INPUT,entry,0,0,'Y'})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,".entry get")==TG_OK&&!strcmp(tg_result(i),"Y"));
    CHECK(tg_eval(i,"set calls")==TG_OK&&!strcmp(tg_result(i),"1"));
    tg_destroy(i); return 0;
}
static int menu_toggle_regression(void) {
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"menu .tog -tearoff 0; .tog add checkbutton -label Show -variable flag -onvalue yes -offvalue no -command {set seen $flag}; set flag")==TG_OK&&!strcmp(tg_result(i),"no"));
    CHECK(tg_eval(i,".tog invoke 0; set seen")==TG_OK&&!strcmp(tg_result(i),"yes"));
    CHECK(tg_eval(i,".tog invoke 0; set flag")==TG_OK&&!strcmp(tg_result(i),"no"));
    CHECK(tg_eval(i,".tog add radiobutton -label Red -variable color -value red; .tog add radiobutton -label Blue -variable color -value blue; set color")==TG_OK&&!strcmp(tg_result(i),""));
    CHECK(tg_eval(i,".tog invoke 2; set color")==TG_OK&&!strcmp(tg_result(i),"blue"));
    CHECK(tg_eval(i,".tog type 0")==TG_OK&&!strcmp(tg_result(i),"checkbutton"));
    CHECK(tg_eval(i,".tog type 1")==TG_OK&&!strcmp(tg_result(i),"radiobutton"));
    CHECK(tg_eval(i,".tog entrycget 0 -onvalue")==TG_OK&&!strcmp(tg_result(i),"yes"));
    CHECK(tg_eval(i,"set flag yes; .tog entryconfigure 0 -state disabled; .tog invoke 0; set flag")==TG_OK&&!strcmp(tg_result(i),"yes"));
    CHECK(tg_eval(i,".tog entryconfigure 0 -state normal; .tog post 10 10")==TG_OK);
    tg_ui *u=tg_interp_ui(i); CHECK(tg_ui_arrange(u,300,200)==TG_OK);
    tg_id id=tg_ui_find(u,".tog");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,id,15,15,0})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,"set flag")==TG_OK&&!strcmp(tg_result(i),"no"));
    CHECK(tg_eval(i,".tog delete 0 end; destroy .tog; set color red")==TG_OK);
    tg_destroy(i); return 0;
}
int main(void) {
    CHECK(!ime_regression());
    CHECK(entry_validation_regression()==0);
    CHECK(menu_toggle_regression()==0);
    CHECK(cascade_capture_regression()==0);
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"set clicks 0; ttk::button .b -text OK -command {incr clicks}; .b invoke")==TG_OK);
    CHECK(!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,".b configure -state disabled; .b invoke; set clicks")==TG_OK&&!strcmp(tg_result(i),"1"));
    CHECK(tg_eval(i,"ttk::entry .e; .e insert 0 abc; .e delete 1; .e get")==TG_OK&&!strcmp(tg_result(i),"ac"));
    CHECK(tg_eval(i,"focus .e; focus")==TG_OK&&!strcmp(tg_result(i),".e"));
    CHECK(tg_eval(i,".b configure -state normal")==TG_OK);
    tg_ui *u=tg_interp_ui(i); tg_id b=tg_ui_find(u,".b");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,b,0,0,0})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_UP,b,0,0,0})==TG_OK&&tg_interp_dispatch_input(i));
    CHECK(tg_eval(i,"set clicks")==TG_OK&&!strcmp(tg_result(i),"2"));
    tg_destroy(i); puts("widget command and callback integration passed"); return 0;
}
