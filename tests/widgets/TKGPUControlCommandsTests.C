#include "TKGPUUi.H"
#include "../../src/widgets/controls/TKGPUControls.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},result,sizeof result)
static int display_regression(void) {
    tg_widget w={0}; char mask[TG_ENTRY_DISPLAY_CAP];
    memset(w.text,'a',sizeof w.text-1);
    strcpy(w.show,"\xf0\x9f\x94\x92");
    const char *shown=tg_entry_display_text(&w,mask);
    CHECK(shown==mask&&strlen(shown)==4092);
    CHECK(!memcmp(shown,"\xf0\x9f\x94\x92",4)&&!memcmp(shown+4088,"\xf0\x9f\x94\x92",4));
    strcpy(w.text,"A\xc3\xa9\xd7\x90");
    CHECK(strlen(tg_entry_display_text(&w,mask))==12);
    w.show[0]=0; CHECK(tg_entry_display_text(&w,mask)==w.text);
    return 0;
}
int main(void) {
    CHECK(display_regression()==0);
    tg_ui *u=tg_ui_create(); CHECK(u); char result[512];
    CHECK(CALL("ttk::frame",".f")==TG_OK);
    CHECK(CALL("ttk::button",".f.b","-command","incr clicks","-text","Apply")==TG_OK);
    CHECK(CALL(".f.b","cget","-command")==TG_OK&&!strcmp(result,"incr clicks"));
    CHECK(CALL(".f.b","configure","-state","disabled","-unknown","bad")==TG_ERROR);
    CHECK(CALL(".f.b","cget","-state")==TG_OK&&!strcmp(result,"normal"));
    CHECK(CALL(".f.b","configure","-state","disabled")==TG_OK);
    CHECK(CALL(".f.b","cget","-state")==TG_OK&&!strcmp(result,"disabled"));
    CHECK(CALL("label",".classic","-fg","#abc","-bg","black","-width","20","-height","2")==TG_OK);
    CHECK(CALL(".classic","cget","-foreground")==TG_OK&&!strcmp(result,"#aabbcc"));
    CHECK(CALL(".classic","cget","-width")==TG_OK&&!strcmp(result,"20"));
    CHECK(CALL(".classic","config","-fg","white")==TG_OK);
    CHECK(CALL(".classic","cget","-fg")==TG_OK&&!strcmp(result,"#ffffff"));
    CHECK(CALL("ttk::scrollbar",".s","-orient","vertical")==TG_OK);
    CHECK(CALL("place",".s","-x","100","-y","0","-width","16","-height","60")==TG_OK);
    CHECK(CALL(".s","set","0.2","0.6")==TG_OK);
    CHECK(tg_ui_arrange(u,120,60)==TG_OK);
    CHECK(CALL(".s","fraction","5","30")==TG_OK&&!strcmp(result,"0.5"));
    CHECK(CALL(".s","delta","0","-30")==TG_OK&&!strcmp(result,"-0.5"));
    CHECK(CALL(".s","identify","5","20")==TG_OK&&!strcmp(result,"slider"));
    CHECK(CALL(".s","identify","5","2")==TG_OK&&!strcmp(result,"trough1"));
    CHECK(CALL(".s","identify","5","59")==TG_OK&&!strcmp(result,"trough2"));
    CHECK(CALL("destroy",".s")==TG_OK);
    CHECK(CALL("ttk::entry",".wide")==TG_OK);
    CHECK(CALL("place",".wide","-x","0","-y","0","-width","45","-height","22")==TG_OK);
    CHECK(tg_ui_arrange(u,120,60)==TG_OK);
    CHECK(CALL(".wide","insert","0","abcdefghijklmnopqrstuvwxyz0123456789")==TG_OK);
    CHECK(CALL(".wide","xview","moveto","0")==TG_OK);
    double first,last;
    CHECK(CALL(".wide","xview")==TG_OK&&sscanf(result,"%lf %lf",&first,&last)==2&&first==0&&last<1);
    CHECK(CALL(".wide","xview","moveto","1")==TG_OK);
    CHECK(CALL(".wide","xview")==TG_OK&&sscanf(result,"%lf %lf",&first,&last)==2&&first>0&&last==1);
    CHECK(CALL(".wide","xview","scroll","-100","pages")==TG_OK);
    CHECK(CALL(".wide","xview")==TG_OK&&sscanf(result,"%lf %lf",&first,&last)==2&&first==0);
    CHECK(CALL(".wide","xview","moveto","nan")==TG_ERROR);
    CHECK(CALL(".wide","get")==TG_OK&&!strcmp(result,"abcdefghijklmnopqrstuvwxyz0123456789"));
    CHECK(CALL("destroy",".wide")==TG_OK);
    CHECK(CALL("ttk::entry",".e")==TG_OK);
    CHECK(CALL(".e","insert","0","A\xc3\xa9Z")==TG_OK);
    CHECK(CALL(".e","index","end")==TG_OK&&!strcmp(result,"3"));
    CHECK(CALL(".e","delete","1","2")==TG_OK);
    CHECK(CALL(".e","get")==TG_OK&&!strcmp(result,"AZ"));
    CHECK(CALL(".e","selection","range","0","end")==TG_OK);
    CHECK(CALL(".e","selection","present")==TG_OK&&!strcmp(result,"1"));
    CHECK(CALL(".e","index","sel.last")==TG_OK&&!strcmp(result,"2"));
    CHECK(CALL(".e","selection","clear")==TG_OK);
    CHECK(CALL(".e","index","@9")==TG_OK&&!strcmp(result,"1"));
    CHECK(CALL(".e","icursor","1")==TG_OK);
    CHECK(CALL("focus",".e")==TG_OK);
    CHECK(CALL("focus")==TG_OK&&!strcmp(result,".e"));
    CHECK(CALL(".e","selection","range","0","end")==TG_OK);
    char script[512];
    CHECK(tg_ui_apply_event(u,(tg_event){TG_TEXT_INPUT,tg_ui_find(u,".e"),0,0,0x416},script,sizeof script));
    CHECK(CALL(".e","get")==TG_OK&&!strcmp(result,"\xd0\x96"));
    CHECK(CALL(".e","selection","present")==TG_OK&&!strcmp(result,"0"));
    CHECK(CALL(".e","delete","0","end")==TG_OK);
    CHECK(CALL(".e","insert","0","AZ")==TG_OK);
    CHECK(CALL(".e","state","disabled readonly")==TG_OK&&strstr(result,"!disabled")&&strstr(result,"!readonly"));
    CHECK(CALL(".e","instate","disabled readonly")==TG_OK&&!strcmp(result,"1"));
    CHECK(CALL(".e","insert","end","Q")==TG_OK);
    CHECK(CALL(".e","get")==TG_OK&&!strcmp(result,"AZ"));
    CHECK(CALL(".e","state","!disabled")==TG_OK&&!strcmp(result,"disabled"));
    CHECK(CALL(".e","instate","!disabled readonly")==TG_OK&&!strcmp(result,"1"));
    CHECK(CALL(".e","state","bogus")==TG_ERROR);
    CHECK(CALL(".e","configure","-state","normal")==TG_OK);
    CHECK(CALL(".e","instate","!disabled !readonly")==TG_OK&&!strcmp(result,"1"));
    CHECK(CALL(".e","configure","-state","readonly")==TG_OK);
    CHECK(CALL(".e","insert","end","Q")==TG_OK);
    CHECK(CALL(".e","get")==TG_OK&&!strcmp(result,"AZ"));
    CHECK(CALL("place",".e","-x","5","-y","5","-width","90","-height","22")==TG_OK);
    CHECK(tg_ui_arrange(u,120,60)==TG_OK);
    tg_canvas *a=tg_canvas_create(120,60),*b=tg_canvas_create(120,60); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_ui_paint_parallel(u,b,4,NULL)==TG_OK);
    CHECK(!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),120u*60u*4u));
    CHECK(CALL("destroy",".e")==TG_OK);
    CHECK(CALL("focus")==TG_OK&&!*result);
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u);
    tg_interp *i=tg_create(); CHECK(i);
    CHECK(tg_eval(i,"ttk::button .b; .b state disabled; .b instate disabled {set value yes}")==TG_OK&&!strcmp(tg_result(i),"yes"));
    CHECK(tg_eval(i,".b instate {!disabled} {error unexpected}")==TG_OK&&!strcmp(tg_result(i),""));
    tg_destroy(i);
    puts("control command vectors, atomic options and rendering passed"); return 0;
}
