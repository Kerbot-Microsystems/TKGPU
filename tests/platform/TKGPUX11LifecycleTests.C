#include "../../src/compat/tk/TKGPUWindow.H"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static Window named_window(Display *d,const char *name) {
    Window root,parent,*children=NULL,found=0; unsigned count=0;
    if(!XQueryTree(d,DefaultRootWindow(d),&root,&parent,&children,&count)) return 0;
    for(unsigned k=0;k<count;k++) {
        char *title=NULL;
        if(XFetchName(d,children[k],&title)&&title) {
            if(!strcmp(title,name)) found=children[k];
            XFree(title);
        }
    }
    if(children) XFree(children);
    return found;
}
int main(void) {
    if(!getenv("DISPLAY")) return 77;
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i&&Tk_Init(i)==TG_OK);
    CHECK(Tcl_Eval(i,"wm title . Lifecycle; wm geometry . 910x610+10+10; . configure -background #f0f2f5")==TG_OK);
    Display *d=XOpenDisplay(NULL); CHECK(d);
    Window win=named_window(d,"Lifecycle"); CHECK(win);
    XWindowAttributes a; CHECK(XGetWindowAttributes(d,win,&a)&&a.map_state==IsUnmapped);
    CHECK(a.bit_gravity==NorthWestGravity);
    CHECK(Tcl_Eval(i,"focus -force .; update")==TG_OK);
    CHECK(XGetWindowAttributes(d,win,&a)&&a.map_state==IsViewable&&a.width==910&&a.height==610);
    CHECK(Tcl_Eval(i,"toplevel .tip; wm title .tip Tooltip; wm overrideredirect .tip 1; label .tip.label -text Hint; pack .tip.label; wm geometry .tip +100+100")==TG_OK);
    Window tip=named_window(d,"Tooltip"); CHECK(tip);
    CHECK(XGetWindowAttributes(d,tip,&a)&&a.map_state==IsUnmapped);
    CHECK(Tcl_Eval(i,"update")==TG_OK);
    CHECK(XGetWindowAttributes(d,tip,&a)&&a.map_state==IsViewable&&a.width<300&&a.height<80);
    CHECK(Tcl_Eval(i,"destroy .tip; wm geometry . 1200x700; update")==TG_OK);
    CHECK(XGetWindowAttributes(d,win,&a)&&a.width==1200&&a.height==700);
    XImage *image=XGetImage(d,win,1199,699,1,1,AllPlanes,ZPixmap); CHECK(image);
    CHECK((XGetPixel(image,0,0)&0xffffff)==0xf0f2f5); XDestroyImage(image);
    /* Existing contents must survive the server-side resize even before the
       application receives ConfigureNotify and can publish another frame. */
    GC marker=XCreateGC(d,win,0,NULL); CHECK(marker);
    XSetForeground(d,marker,0xd02030); XFillRectangle(d,win,marker,5,5,20,20);
    XResizeWindow(d,win,1000,720); XSync(d,False);
    image=XGetImage(d,win,10,10,1,1,AllPlanes,ZPixmap); CHECK(image);
    CHECK((XGetPixel(image,0,0)&0xffffff)==0xd02030); XDestroyImage(image); XFreeGC(d,marker);
    /* A WM/user resize must not be undone by the next idle layout pass. */
    XResizeWindow(d,win,800,600); XSync(d,False);
    CHECK(Tcl_Eval(i,"update; frame .panel -background #f0f2f5; pack .panel -fill both -expand 1; update; set changes 0; bind .panel <Configure> {incr changes}")==TG_OK);
    for(int k=0;k<20;k++) CHECK(Tcl_Eval(i,". configure -background #f0f2f5; update")==TG_OK);
    CHECK(Tcl_Eval(i,"set changes")==TG_OK&&!strcmp(Tcl_GetStringResult(i),"0"));
    CHECK(Tcl_Eval(i,"tkgpu::frame_stats")==TG_OK);
    unsigned long long before=strtoull(strchr(Tcl_GetStringResult(i),' ')+1,NULL,10);
    for(int k=0;k<80;k++) {
        XResizeWindow(d,win,900+(unsigned)k,650); XSync(d,False);
        CHECK(Tcl_Eval(i,"update")==TG_OK);
    }
    CHECK(Tcl_Eval(i,"after 80; update; tkgpu::frame_stats")==TG_OK);
    unsigned long long after=strtoull(strchr(Tcl_GetStringResult(i),' ')+1,NULL,10);
    CHECK(after>before&&after-before<80);
    image=XGetImage(d,win,978,649,1,1,AllPlanes,ZPixmap); CHECK(image);
    CHECK((XGetPixel(image,0,0)&0xffffff)==0xf0f2f5); XDestroyImage(image);
    CHECK(Tcl_Eval(i,"wm withdraw .; update")==TG_OK);
    CHECK(XGetWindowAttributes(d,win,&a)&&a.map_state==IsUnmapped);
    Tcl_DeleteInterp(i); XCloseDisplay(d);
    puts("X11 deferred mapping, focus, compact tooltip and full resized-frame pixels passed"); return 0;
}
