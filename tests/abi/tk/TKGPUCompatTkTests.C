#include "TKGPUCompat.H"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    Tcl_Interp *a=Tcl_CreateInterp(),*b=Tcl_CreateInterp(); CHECK(a&&b);
    CHECK(Tk_GetNumMainWindows()==0&&!Tk_MainWindow(a));
    if(Tk_Init(a)!=0) {
        fprintf(stderr,"native window unavailable: %s\n",Tcl_GetStringResult(a));
        Tcl_DeleteInterp(a); Tcl_DeleteInterp(b); return 77;
    }
    CHECK(Tk_MainWindow(a)&&Tk_GetNumMainWindows()==1);
    CHECK(Tk_Init(a)==0&&Tk_GetNumMainWindows()==1);
    CHECK(!strcmp(Tcl_GetVar2(a,"tk_version",NULL,0),"8.6"));
    CHECK(Tcl_Eval(a,"wm title . {Kerbot Drone}")==0);
    CHECK(Tcl_Eval(a,"wm title .")==0&&!strcmp(Tcl_GetStringResult(a),"Kerbot Drone"));
    CHECK(Tcl_Eval(a,"wm geometry . 320x200")==0);
    CHECK(Tcl_Eval(a,"wm geometry .")==0&&!strcmp(Tcl_GetStringResult(a),"320x200"));
    CHECK(Tcl_Eval(a,"wm geometry . 999999x200")==1);
    CHECK(Tcl_Eval(a,"wm geometry .")==0&&!strcmp(Tcl_GetStringResult(a),"320x200"));
    CHECK(Tcl_Eval(a,"wm geometry . 360x240+20-0")==0);
    CHECK(Tcl_Eval(a,"wm geometry .")==0&&!strcmp(Tcl_GetStringResult(a),"360x240+20-0"));
    CHECK(Tcl_Eval(a,"wm minsize . 100 80; wm maxsize . 800 600; wm resizable . 0 1")==0);
    CHECK(Tcl_Eval(a,"wm minsize .")==0&&!strcmp(Tcl_GetStringResult(a),"100 80"));
    CHECK(Tcl_Eval(a,"wm resizable .")==0&&!strcmp(Tcl_GetStringResult(a),"0 1"));
    CHECK(Tcl_Eval(a,"wm overrideredirect . 0")==0);
    CHECK(Tcl_Eval(a,"wm overrideredirect .")==0&&!strcmp(Tcl_GetStringResult(a),"0"));
    CHECK(Tcl_Eval(a,"wm overrideredirect . 1")==0);
    CHECK(Tcl_Eval(a,"wm overrideredirect .")==0&&!strcmp(Tcl_GetStringResult(a),"1"));
    CHECK(Tcl_Eval(a,"wm overrideredirect . 0")==0);
    CHECK(Tcl_Eval(a,"wm overrideredirect .")==0&&!strcmp(Tcl_GetStringResult(a),"0"));
    CHECK(Tcl_Eval(a,"wm minsize . 900 80")==1);
    CHECK(Tcl_Eval(a,"wm resizable . 1 1; winfo screenwidth .")==0);
    CHECK(Tcl_GetStringResult(a)[0]>='1'&&Tcl_GetStringResult(a)[0]<='9');
    CHECK(Tcl_Eval(a,"wm withdraw .; wm state .")==0&&!strcmp(Tcl_GetStringResult(a),"withdrawn"));
    CHECK(Tcl_Eval(a,"wm deiconify .; wm state .")==0&&!strcmp(Tcl_GetStringResult(a),"normal"));
    CHECK(Tcl_Eval(a,"wm protocol . WM_DELETE_WINDOW {set closed yes}")==0);
    CHECK(Tcl_Eval(a,"wm protocol . WM_DELETE_WINDOW")==0&&!strcmp(Tcl_GetStringResult(a),"set closed yes"));
    CHECK(Tcl_Eval(a,"wm protocol .")==0&&!strcmp(Tcl_GetStringResult(a),"WM_DELETE_WINDOW"));
#ifdef TKGPU_COCOA
    CHECK(Tcl_Eval(a,"tk windowingsystem")==0&&!strcmp(Tcl_GetStringResult(a),"aqua"));
    CHECK(Tcl_Eval(a,"wm attributes . -alpha 0.75 -topmost 1")==0);
    CHECK(Tcl_Eval(a,"wm attributes . -alpha")==0&&atof(Tcl_GetStringResult(a))==0.75);
    CHECK(Tcl_Eval(a,"wm attributes . -topmost")==0&&!strcmp(Tcl_GetStringResult(a),"1"));
    CHECK(Tcl_Eval(a,"wm attributes . -alpha 1 -topmost 0")==0);
    CHECK(Tcl_Eval(a,"toplevel .child; wm transient .child .")==0);
    CHECK(Tcl_Eval(a,"wm transient .child")==0&&!strcmp(Tcl_GetStringResult(a),"."));
    CHECK(Tcl_Eval(a,"wm transient . .child")==1);
    CHECK(Tcl_Eval(a,"wm transient .child {}; destroy .child")==0);
#endif
    CHECK(Tcl_Eval(a,"wm protocol . WM_DELETE_WINDOW {}")==0);
    CHECK(Tcl_Eval(a,"wm protocol .")==0&&!strcmp(Tcl_GetStringResult(a),""));
    CHECK(Tk_Init(b)==0&&Tk_GetNumMainWindows()==2&&Tk_MainWindow(a)!=Tk_MainWindow(b));
    CHECK(Tcl_Eval(a,"label .l -text Hello; pack .l")==0);
    CHECK(Tcl_Eval(a,"frame .events; bind .events <Enter> {set hover enter}; bind .events <Leave> {set hover leave}; event generate .events <Enter> -when tail; update; set hover")==0&&!strcmp(Tcl_GetStringResult(a),"enter"));
    CHECK(Tcl_Eval(a,"event generate .events <Leave> -when tail; update; set hover")==0&&!strcmp(Tcl_GetStringResult(a),"leave"));
    CHECK(Tcl_Eval(a,"ttk::scrollbar .scroll -command {.view yview}; canvas .view -scrollregion {0 0 100 100} -yscrollcommand {.scroll set}; place .view -width 20 -height 20; update")==0);
    CHECK(Tcl_Eval(a,".view yview moveto 0.3; update; .scroll get")==0&&!strcmp(Tcl_GetStringResult(a),"0.3 0.5"));
    CHECK(Tcl_Eval(a,"destroy .view; destroy .scroll")==0);
    CHECK(Tcl_Eval(a,"font create TestFont -family {IBM Plex Sans} -size 12 -weight bold")==0&&!strcmp(Tcl_GetStringResult(a),"TestFont"));
    CHECK(Tcl_Eval(a,"font configure TestFont -size")==0&&!strcmp(Tcl_GetStringResult(a),"12"));
    CHECK(Tcl_Eval(a,"font config TestFont -size")==0&&!strcmp(Tcl_GetStringResult(a),"12"));
    CHECK(Tcl_Eval(a,"font configure TestFont -size 16; font measure TestFont ABC")==0&&atoi(Tcl_GetStringResult(a))>0);
    CHECK(Tcl_Eval(a,"font configure TestFont -size 0")==1);
    CHECK(Tcl_Eval(a,"font configure TestFont -size")==0&&!strcmp(Tcl_GetStringResult(a),"16"));
    CHECK(Tcl_Eval(a,"font delete TestFont; font names")==0&&!strstr(Tcl_GetStringResult(a),"TestFont"));
    CHECK(Tcl_Eval(a,"set waiting same; after 0 {set waiting same}; vwait waiting")==0);
    CHECK(Tcl_Eval(a,"set waiting value; after 0 {unset waiting}; tkwait variable ::waiting")==0);
    CHECK(Tcl_Eval(a,"frame .wait; after 0 {destroy .wait; frame .wait}; tkwait window .wait")==0);
    CHECK(Tcl_Eval(a,"winfo exists .wait")==0&&!strcmp(Tcl_GetStringResult(a),"1"));
    CHECK(Tcl_Eval(a,"tkwait window .missing")==1);
    CHECK(Tcl_Eval(a,"destroy .wait")==0);
    CHECK(Tcl_Eval(a,"frame .animated; place .animated -x 0 -y 0 -width 20 -height 20")==0);
    CHECK(Tcl_Eval(a,"tkgpu::animate start .animated opacity 0 0")==0);
    CHECK(Tcl_Eval(a,".animated cget -opacity")==0&&!strcmp(Tcl_GetStringResult(a),"0"));
    CHECK(Tcl_Eval(a,"tkgpu::animate start .animated opacity 1 60 linear; tkgpu::animate start .animated x 50 60 ease-in-out; tkgpu::animate active")==0&&!strcmp(Tcl_GetStringResult(a),"2"));
    CHECK(Tcl_Eval(a,"tkgpu::animate start .animated opacity NaN 60")==1);
    CHECK(Tcl_Eval(a,"after 100")==0);
    for(int k=0;k<32;k++) if(!Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT)) break;
    CHECK(Tcl_Eval(a,"tkgpu::animate active")==0&&!strcmp(Tcl_GetStringResult(a),"0"));
    CHECK(Tcl_Eval(a,".animated cget -opacity")==0&&!strcmp(Tcl_GetStringResult(a),"1"));
    CHECK(Tcl_Eval(a,"place info .animated")==0&&strstr(Tcl_GetStringResult(a),"-x 50"));
    CHECK(Tcl_Eval(a,"tkgpu::animate start .animated groupopacity 0 1000; tkgpu::animate cancel .animated groupopacity; tkgpu::animate active")==0&&!strcmp(Tcl_GetStringResult(a),"0"));
    CHECK(Tcl_Eval(a,"tkgpu::animate start .animated opacity 0 1000; destroy .animated")==0);
    CHECK(Tcl_Eval(a,"after 50")==0);
    for(int k=0;k<32;k++) if(!Tcl_DoOneEvent(TKGPU_TIMER_EVENTS|TKGPU_DONT_WAIT)) break;
    CHECK(Tcl_Eval(a,"tkgpu::animate active")==0&&!strcmp(Tcl_GetStringResult(a),"0"));
    CHECK(Tcl_Eval(a,"tkgpu::animate start .l opacity 0 1000")==0);
    for(int k=0;k<32;k++) Tcl_DoOneEvent(TKGPU_WINDOW_EVENTS|TKGPU_DONT_WAIT);
    CHECK(Tcl_Eval(a,"destroy .")==0&&!Tk_MainWindow(a)&&Tk_GetNumMainWindows()==1);
    Tcl_DeleteInterp(a); Tcl_DeleteInterp(b); CHECK(Tk_GetNumMainWindows()==0);
    puts("native Tk root lifecycle passed"); return 0;
}
