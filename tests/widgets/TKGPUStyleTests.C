#include "TKGPUUi.H"
#include "../../src/widgets/ttk/TKGPUStyle.H"
#include "TKGPURaster.H"
#include "TKGPUAsync.H"
#include "../../src/render/async/TKGPUSnapshot.H"
#include "../../src/widgets/image/TKGPUImageRegistry.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define CALL(...) tg_ui_command(u,sizeof((char *[]){__VA_ARGS__})/sizeof(char *),(char *[]){__VA_ARGS__},out,sizeof out)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u); char out[256];
    CHECK(CALL("ttk::style","configure","TButton","-background","#123456")==TG_OK);
    CHECK(CALL("ttk::style","configure","TButton","-0123456789012345678901234567890123456789","x")==TG_ERROR);
    CHECK(CALL("ttk::style","theme","use","alt")==TG_OK);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,""));
    CHECK(CALL("ttk::style","configure","TButton","-background","#987654")==TG_OK);
    CHECK(CALL("ttk::style","theme","use","unknown")==TG_ERROR);
    CHECK(CALL("ttk::style","theme","use")==TG_OK&&!strcmp(out,"alt"));
    CHECK(CALL("ttk::style","theme","use","clam")==TG_OK);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,"#123456"));
    CHECK(CALL("ttk::style","configure","Accent.TButton","-foreground","white")==TG_OK);
    CHECK(CALL("ttk::style","lookup","Accent.TButton","-background")==TG_OK&&!strcmp(out,"#123456"));
    CHECK(CALL("ttk::button",".b","-style","Accent.TButton")==TG_OK);
    CHECK(CALL(".b","cget","-style")==TG_OK&&!strcmp(out,"Accent.TButton"));
    CHECK(CALL("place",".b","-x","0","-y","0","-width","30","-height","30")==TG_OK);
    CHECK(CALL("ttk::style","configure","TButton","-background","red","-bordercolor","#not-a-color")==TG_ERROR);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,"#123456"));
    CHECK(CALL("ttk::style","configure","TButton","-padding","12 7","-font","{IBM Plex Sans} 10 bold","-bordercolor","#223344")==TG_OK);
    CHECK(CALL("ttk::style","map","TButton","-background","pressed #345678 active #456789 disabled #567890")==TG_OK);
    CHECK(CALL("ttk::style","lookup","Accent.TButton","-background","pressed")==TG_OK&&!strcmp(out,"#345678"));
    CHECK(CALL("ttk::style","configure","TButton","-padding")==TG_OK&&!strcmp(out,"12 7"));
    CHECK(tg_ui_arrange(u,80,80)==TG_OK);
    tg_canvas *a=tg_canvas_create(80,80),*b=tg_canvas_create(80,80); CHECK(a&&b);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_canvas_pixels(a)[0]==0x123456);
    CHECK(tg_ui_paint_parallel(u,b,8,NULL)==TG_OK&&!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),80u*80u*4u));
    CHECK(CALL("ttk::style","configure","Accent.TButton","-background","#ff000080")==TG_OK);
    CHECK(CALL("ttk::style","lookup","Accent.TButton","-background")==TG_OK&&!strcmp(out,"#ff000080"));
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_canvas_pixels(a)[0]==tg_pixel_blend(0x182536,0xff000080u));
    CHECK(CALL("ttk::style","map","Accent.TButton","-background","pressed #0000ff40")==TG_OK);
    CHECK(CALL("ttk::style","lookup","Accent.TButton","-background","pressed")==TG_OK&&!strcmp(out,"#0000ff40"));
    CHECK(CALL(".b","configure","-style","Other.TButton")==TG_OK);
    CHECK(CALL(".b","cget","-style")==TG_OK&&!strcmp(out,"Other.TButton"));
    tg_id id=tg_ui_find(u,".b");
    for(int k=0;k<10000;k++) CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,id,k,0,0})==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,id,1,0,0})==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,id,2,0,0})==TG_OK);
    tg_event e;
    CHECK(tg_ui_poll(u,&e)&&e.kind==TG_POINTER_MOVE&&e.x==9999);
    CHECK(tg_ui_poll(u,&e)&&e.kind==TG_POINTER_DOWN);
    CHECK(tg_ui_poll(u,&e)&&e.kind==TG_POINTER_MOVE&&e.x==2);
    CHECK(!tg_ui_poll(u,&e));
    CHECK(CALL("ttk::style","configure","Horizontal.TScrollbar","-background","#010203")==TG_OK);
    CHECK(CALL("ttk::style","configure","Vertical.TScrollbar","-background","#040506")==TG_OK);
    CHECK(CALL("ttk::scrollbar",".vertical","-orient","vertical")==TG_OK);
    const tg_widget *vertical=tg_ui_get(u,tg_ui_find(u,".vertical"));
    CHECK(vertical&&tg_style_widget_value(u,vertical,"-background",out,sizeof out)>0&&!strcmp(out,"#040506"));
    CHECK(CALL("ttk::style","theme","create","kerbot","-parent","clam")==TG_OK);
    CHECK(CALL("ttk::style","theme","create","kerbot","-parent","clam")==TG_ERROR);
    CHECK(CALL("ttk::style","theme","create","bad","-parent","missing")==TG_ERROR);
    CHECK(CALL("ttk::style","theme","use","kerbot")==TG_OK);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,"#123456"));
    CHECK(CALL("ttk::style","lookup","TButton","-background","pressed")==TG_OK&&!strcmp(out,"#345678"));
    CHECK(CALL("ttk::style","configure","TButton","-background","#abcdef")==TG_OK);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,"#abcdef"));
    CHECK(CALL("ttk::style","theme","create","nested","-parent","kerbot")==TG_OK);
    CHECK(CALL("ttk::style","theme","use","nested")==TG_OK);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,"#abcdef"));
    tg_ui *snapshot=tg_snapshot_create(u); CHECK(snapshot);
    char *lookup[]={"ttk::style","lookup","TButton","-background"};
    CHECK(tg_ui_command(snapshot,4,lookup,out,sizeof out)==TG_OK&&!strcmp(out,"#abcdef"));
    tg_ui_destroy(snapshot);
    CHECK(CALL("ttk::style","theme","use","clam")==TG_OK);
    CHECK(CALL("ttk::style","lookup","TButton","-background")==TG_OK&&!strcmp(out,"#123456"));
    CHECK(tg_image_add(u,"normal",8,8)&&tg_image_add(u,"pressed",8,8));
    CHECK(tg_photo_fill(tg_image_find(u,"normal")->photo,(tg_rect){0,0,8,8},0x00ff00ff)==TG_OK);
    CHECK(tg_photo_fill(tg_image_find(u,"pressed")->photo,(tg_rect){0,0,8,8},0xff0000ff)==TG_OK);
    CHECK(CALL("ttk::style","element","create","Kerbot.image","image","normal pressed pressed")==TG_OK);
    CHECK(CALL("ttk::style","layout","Other.TButton","Kerbot.image -sticky nsew")==TG_OK);
    CHECK(CALL("ttk::style","layout","Other.TButton")==TG_OK&&!strcmp(out,"Kerbot.image -sticky nsew"));
    CHECK(CALL("ttk::style","layout","Other.TButton","Missing.element")==TG_ERROR);
    CHECK(CALL("ttk::style","layout","Other.TButton")==TG_OK&&!strcmp(out,"Kerbot.image -sticky nsew"));
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_canvas_pixels(a)[15*80+15]==0x00ff00);
    CHECK(CALL(".b","identify","15","15")==TG_OK&&!strcmp(out,"Kerbot.image"));
    CHECK(CALL(".b","identify","0","0")==TG_OK&&!strcmp(out,""));
    CHECK(CALL(".b","identify","-1","15")==TG_OK&&!strcmp(out,""));
    CHECK(CALL(".b","identify","invalid","15")==TG_ERROR);
    CHECK(CALL(".b","state","pressed")==TG_OK);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_canvas_pixels(a)[15*80+15]==0xff0000);
    CHECK(tg_ui_paint_parallel(u,b,8,NULL)==TG_OK&&!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),80u*80u*4u));
    const char *classes[]={"TEntry","TCombobox","TSpinbox","Horizontal.TProgressbar","Vertical.TProgressbar",
        "TSeparator","Horizontal.TScale","Vertical.TScale","Horizontal.TScrollbar","Vertical.TScrollbar",
        "TLabelframe","TNotebook","Treeview","TCheckbutton","TRadiobutton","TPanedwindow","TMenubutton","TSizegrip"};
    for(size_t k=0;k<sizeof classes/sizeof classes[0];k++) {
        char *query[]={"ttk::style","layout",(char *)classes[k]};
        CHECK(tg_ui_command(u,3,query,out,sizeof out)==TG_OK&&strstr(out,".body"));
        char *set[]={"ttk::style","layout",(char *)classes[k],"Kerbot.image -sticky ne"};
        CHECK(tg_ui_command(u,4,set,out,sizeof out)==TG_OK);
    }
    CHECK(CALL("ttk::entry",".decor","-style","TEntry")==TG_OK);
    CHECK(CALL("place",".decor","-x","35","-y","0","-width","35","-height","30")==TG_OK);
    CHECK(tg_ui_arrange(u,80,80)==TG_OK);
    CHECK(tg_ui_paint(u,a)==TG_OK&&tg_canvas_pixels(a)[2*80+65]==0x00ff00);
    CHECK(CALL(".decor","identify","30","2")==TG_OK&&!strcmp(out,"Kerbot.image"));
    CHECK(CALL(".decor","identify","2","2")==TG_OK&&!strcmp(out,""));
    CHECK(CALL(".decor","insert","0","text")==TG_OK);
    CHECK(CALL(".decor","get")==TG_OK&&!strcmp(out,"text"));
    CHECK(tg_ui_paint_parallel(u,b,8,NULL)==TG_OK);
    CHECK(tg_ui_paint(u,a)==TG_OK&&!memcmp(tg_canvas_pixels(a),tg_canvas_pixels(b),80u*80u*4u));
    tg_canvas_destroy(a); tg_canvas_destroy(b); tg_ui_destroy(u);
    puts("style inheritance, atomic configuration, parallel pixels and motion coalescing passed"); return 0;
}
