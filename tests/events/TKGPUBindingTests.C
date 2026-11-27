#include "TKGPUUi.H"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
#define E(s,v) CHECK(tg_eval(i,s)==TG_OK&&!strcmp(tg_result(i),v))
int main(void) {
    tg_interp *i=tg_create(); CHECK(i);
    E("button .b; bind .b <Button-1> {set hit %W; set x %x}","");
    E("bind .b <Button-1> {+set y %y}","");
    E("bind all <Button-1> {set allhit yes}","");
    E("event generate .b <Button-1> -x 12 -y 14","");
    CHECK(tg_interp_dispatch_input(i));
    E("set hit",".b"); E("set x","12"); E("set y","14"); E("set allhit","yes");
    E("bind .b <Button-1> {if {\"[set callback_result break]\" == \"break\"} break}","");
    E("set allhit no; event generate .b <Button-1>","");
    CHECK(tg_interp_dispatch_input(i)); E("set allhit","no");
    E("bind .b <Button-1> {if {\"[set callback_result ok]\" == \"break\"} break}","");
    E("event generate .b <Button-1>","");
    CHECK(tg_interp_dispatch_input(i)); E("set allhit","yes");
    E("catch {break}","3");
    E("bind .b <Button-1> {set fields [list %# %b %f %h %k %s %t %w %x %y %A %E %K %N %W %T %X %Y %D]}","");
    E("event generate .b <Button-1> -x 2 -y 3","");
    CHECK(tg_interp_dispatch_input(i));
    E("llength $fields","19");
    E("lindex $fields 0","0");
    E("bind .b <Button-1> {}","");
    E("bind .b <Button-1>","");
    E("bind .b <Configure> {set width %w}","");
    E("event generate .b <Configure> -width 100 -height 40","");
    CHECK(tg_interp_dispatch_input(i)); E("set width","100");
    E("frame .f; bind .b <Enter> {set entered %W}; bind .b <Leave> {set left %W}; bind .f <Enter> {set entered %W}","");
    E("event generate .b <Motion> -x 3 -y 4","");
    CHECK(tg_interp_dispatch_input(i)); E("set entered",".b");
    E("event generate .f <Motion> -x 8 -y 8","");
    CHECK(tg_interp_dispatch_input(i)); E("set left",".b"); E("set entered",".f");
    E("bind .f <Leave> {set left %W}","");
    CHECK(tg_ui_post(tg_interp_ui(i),(tg_event){TG_POINTER_EXIT,0,0,0,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i)); E("set left",".f");
    E("destroy .b; button .b; bind .b <Configure>","");
    E("bind .b <Motion> {destroy .b}; event generate .b <Motion>","");
    CHECK(tg_interp_dispatch_input(i));
    E("winfo exists .b","0");
    /* A moving widget must keep its implicit grab across hit targets, even
       when callbacks live on a custom bindtag rather than the widget. */
    E("frame .drag; place .drag -x 0 -y 0 -width 20 -height 20; frame .other; place .other -x 80 -y 0 -width 20 -height 20","");
    E("bindtags .drag {DragTag}; bind DragTag <B1-Motion> {place .drag -x %X -y %Y}; bind DragTag <ButtonRelease-1> {set released %W}","");
    E("set released no","");
    tg_ui *u=tg_interp_ui(i); tg_id drag=tg_ui_find(u,".drag"),other=tg_ui_find(u,".other");
    CHECK(tg_ui_arrange(u,200,100)==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,drag,5,5,1})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_FOCUS_OUT,other,0,0,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,other,80,10,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_arrange(u,200,100)==TG_OK&&tg_ui_get(u,drag)->rect.x==80);
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,other,100,15,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_arrange(u,200,100)==TG_OK&&tg_ui_get(u,drag)->rect.x==100);
    E("set released","no");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_UP,other,100,15,1})==TG_OK);
    CHECK(tg_interp_dispatch_input(i)); E("set released",".drag");
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_MOVE,other,120,20,0})==TG_OK);
    CHECK(tg_interp_dispatch_input(i));
    CHECK(tg_ui_arrange(u,200,100)==TG_OK&&tg_ui_get(u,drag)->rect.x==100);
    E("bind DragTag <Motion> {set localx %x; set localy %y; set rootx %X; set rooty %Y}","");
    E("event generate .drag <Motion> -x 3 -y 4","");
    CHECK(tg_interp_dispatch_input(i));
    E("set localx","3"); E("set localy","4");
    E("set rootx","103"); E("set rooty","19");
    E("bind .other <Map> {set mapped %T}; bind .other <Unmap> {set unmapped %T}; bind .other <Configure> {set configured %T}","");
    E("event generate .other <Map>","");
    CHECK(tg_interp_dispatch_input(i)); E("set mapped","19");
    E("event generate .other <Unmap>","");
    CHECK(tg_interp_dispatch_input(i)); E("set unmapped","18");
    E("event generate .other <Configure> -width 30 -height 20","");
    CHECK(tg_interp_dispatch_input(i)); E("set configured","22");
    /* Click and keyboard messages may all arrive before dispatch begins.
       Return must go to the newly focused entry, not the old one. */
    E("entry .old; entry .cmd; place .old -width 100 -height 20; place .cmd -y 30 -width 100 -height 20; bind .old <Return> {set submitted old}; bind .cmd <Return> {set submitted [.cmd get]}; focus .old","");
    CHECK(tg_ui_arrange(u,200,100)==TG_OK);
    tg_id cmd=tg_ui_find(u,".cmd");
    while(tg_interp_dispatch_input(i)) {}
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_DOWN,cmd,5,35,1})==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_POINTER_UP,cmd,5,35,1})==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_TEXT_INPUT,1,0,0,TG_NATIVE_KEY_EVENT|'x'})==TG_OK);
    CHECK(tg_ui_post(u,(tg_event){TG_KEY_DOWN,1,0,0,TG_NATIVE_KEY_EVENT|TG_KEY_RETURN})==TG_OK);
    while(tg_interp_dispatch_input(i)) {}
    E("set submitted","x");
    E(".old get","");
    E(".cmd get","x");
    tg_destroy(i); return 0;
}
