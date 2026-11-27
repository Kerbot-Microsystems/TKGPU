#include "../../../compat/tk/TKGPUWindow.H"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "TKGPUUtf8.H"
#include <stdlib.h>
#include <string.h>
typedef struct { Display *display; Window window; Atom close; int width,height,exposed,grip,grip_x,grip_y,grip_w,grip_h; XImage *image; int direct_rgb; XIM im; XIC ic; } native;
static uint32_t keysym_scalar(KeySym sym) {
    if((sym&0xff000000ul)==0x01000000ul) return (uint32_t)(sym&0xfffffful);
    if(sym>=0x20&&sym<=0xff) return (uint32_t)sym;
    /* Historical X11 Cyrillic keysyms predate direct Unicode encoding. */
    static const uint16_t letters[32]={
        0x044e,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433,
        0x0445,0x0438,0x0439,0x043a,0x043b,0x043c,0x043d,0x043e,
        0x043f,0x044f,0x0440,0x0441,0x0442,0x0443,0x0436,0x0432,
        0x044c,0x044b,0x0437,0x0448,0x044d,0x0449,0x0447,0x044a
    };
    if(sym>=0x06c0&&sym<=0x06df) return letters[sym-0x06c0];
    if(sym>=0x06e0&&sym<=0x06ff) return letters[sym-0x06e0]-32;
    switch(sym) {
    case 0x06a3: return 0x0451; case 0x06b3: return 0x0401;
    case 0x06a4: return 0x0454; case 0x06b4: return 0x0404;
    case 0x06a6: return 0x0456; case 0x06b6: return 0x0406;
    case 0x06a7: return 0x0457; case 0x06b7: return 0x0407;
    case 0x06ad: return 0x0491; case 0x06bd: return 0x0490;
    default: return 0;
    }
}
static unsigned long channel(unsigned value,unsigned long mask) {
    if(!mask) return 0;
    unsigned shift=0;
    while(!(mask&1ul)) { mask>>=1; shift++; }
    return (unsigned long)(((uint64_t)value*mask+127)/255)<<shift;
}
static int ensure_image(native *n,int width,int height) {
    if(n->image&&n->image->width==width&&n->image->height==height) return 1;
    int screen=DefaultScreen(n->display); Visual *visual=DefaultVisual(n->display,screen);
    XImage *image=XCreateImage(n->display,visual,(unsigned)DefaultDepth(n->display,screen),
                               ZPixmap,0,NULL,(unsigned)width,(unsigned)height,32,0);
    if(!image) return 0;
    if(image->bytes_per_line<=0||(size_t)image->bytes_per_line>SIZE_MAX/(size_t)height) {
        XDestroyImage(image); return 0;
    }
    image->data=calloc((size_t)height,(size_t)image->bytes_per_line);
    if(!image->data) { XDestroyImage(image); return 0; }
    /* Only use a byte copy when both pixel layout and byte order match.
       Other TrueColor visuals use XPutPixel's conversion path. */
    uint32_t endian=1;
    int order=*(const unsigned char *)&endian?LSBFirst:MSBFirst;
    n->direct_rgb=image->bits_per_pixel==32&&image->byte_order==order&&
        image->red_mask==0x00ff0000ul&&image->green_mask==0x0000ff00ul&&
        image->blue_mask==0x000000fful&&(size_t)image->bytes_per_line>=(size_t)width*4;
    if(n->image) XDestroyImage(n->image);
    n->image=image; return 1;
}
static int paint(tg_window *w,native *n) {
    int width=tg_canvas_width(w->canvas),height=tg_canvas_height(w->canvas);
    if(width<=0||height<=0||!ensure_image(n,width,height)) return 0;
    int screen=DefaultScreen(n->display);
    XImage *image=n->image;
    const uint32_t *p=tg_canvas_pixels(w->canvas);
    if(n->direct_rgb) {
        size_t bytes=(size_t)width*sizeof *p;
        if((size_t)image->bytes_per_line==bytes) memcpy(image->data,p,bytes*(size_t)height);
        else for(int y=0;y<height;y++)
            memcpy(image->data+(size_t)y*image->bytes_per_line,p+(size_t)y*width,bytes);
    } else for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
        uint32_t rgb=p[(size_t)y*width+x];
        unsigned long pixel=channel((rgb>>16)&255,image->red_mask)|
            channel((rgb>>8)&255,image->green_mask)|channel(rgb&255,image->blue_mask);
        XPutPixel(image,x,y,pixel);
    }
    XPutImage(n->display,n->window,DefaultGC(n->display,screen),image,0,0,0,0,(unsigned)width,(unsigned)height);
    XFlush(n->display); return 1;
}
int tg_window_open(tg_window *w) {
    native *n=calloc(1,sizeof *n); if(!n) return 0;
    n->display=XOpenDisplay(NULL);
    if(!n->display) { free(n); return 0; }
    int screen=DefaultScreen(n->display);
    if(DefaultVisual(n->display,screen)->class!=TrueColor) { XCloseDisplay(n->display); free(n); return 0; }
    n->width=640; n->height=480;
    n->window=XCreateSimpleWindow(n->display,RootWindow(n->display,screen),0,0,640,480,0,0,0);
    if(!n->window) { XCloseDisplay(n->display); free(n); return 0; }
    n->close=XInternAtom(n->display,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(n->display,n->window,&n->close,1);
    const char *app_class=getenv("TKGPU_APP_CLASS");
    XClassHint class_hint={(char *)"tkgpu",(char *)(app_class&&*app_class?app_class:"TKGPU")};
    XSetClassHint(n->display,n->window,&class_hint);
    XStoreName(n->display,n->window,"tk");
    XSelectInput(n->display,n->window,ExposureMask|StructureNotifyMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|LeaveWindowMask|KeyPressMask|KeyReleaseMask|FocusChangeMask);
    n->im=XOpenIM(n->display,NULL,NULL,NULL);
    if(n->im) {
        n->ic=XCreateIC(n->im,XNInputStyle,XIMPreeditNothing|XIMStatusNothing,
                        XNClientWindow,n->window,XNFocusWindow,n->window,NULL);
        if(n->ic) {
            long mask=0;
            if(!XGetICValues(n->ic,XNFilterEvents,&mask,NULL)) {
                XWindowAttributes attr;
                if(XGetWindowAttributes(n->display,n->window,&attr))
                    XSelectInput(n->display,n->window,attr.your_event_mask|mask);
            }
        }
    }
    XMapWindow(n->display,n->window); XFlush(n->display); w->native=n; return 1;
}
void tg_window_close_native(tg_window *w) {
    native *n=w->native;
    if(n&&n->ic) { XDestroyIC(n->ic); n->ic=NULL; }
    if(n&&n->im) { XCloseIM(n->im); n->im=NULL; }
    if(n&&n->window) { XDestroyWindow(n->display,n->window); n->window=0; XFlush(n->display); }
    if(n&&n->image) { XDestroyImage(n->image); n->image=NULL; }
    w->alive=0;
}
void tg_window_free_native(tg_window *w) {
    native *n=w->native; if(!n) return;
    tg_window_close_native(w); if(n->image) XDestroyImage(n->image);
    XCloseDisplay(n->display); free(n); w->native=NULL;
}
int tg_window_poll_native(tg_window *w) {
    native *n=w->native; if(!n||!n->window) return 0;
    /* Publish completed frames even while motion events keep XPending true. */
    int painted=0;
    if(tg_window_refresh(w,n->width,n->height)) n->exposed=1;
    if(n->exposed&&paint(w,n)) { n->exposed=0; painted=1; }
    if(XPending(n->display)) {
        XEvent e; XNextEvent(n->display,&e);
        if(n->ic&&XFilterEvent(&e,None)) return 1;
        switch(e.type) {
        case ConfigureNotify: n->width=e.xconfigure.width; n->height=e.xconfigure.height; w->dirty=1; tg_window_input(w,TG_CONFIGURE,n->width,n->height,0); break;
        case FocusIn: if(n->ic) XSetICFocus(n->ic); tg_window_input(w,TG_FOCUS_IN,0,0,0); break;
        case FocusOut: if(n->ic) XUnsetICFocus(n->ic); tg_window_input(w,TG_FOCUS_OUT,0,0,0); break;
        case Expose: n->exposed=1; break;
        case ClientMessage:
            if((Atom)e.xclient.data.l[0]==n->close) { tg_window_request_close(w); } break;
        case DestroyNotify: n->window=0; w->alive=0; break;
        case ButtonPress: {
            tg_ui *u=tg_interp_ui(w->interp);
            const tg_widget *g=tg_ui_get(u,tg_ui_hit(u,e.xbutton.x,e.xbutton.y));
            if(e.xbutton.button==Button1&&g&&g->kind==TG_SIZEGRIP&&g->state!=TG_STATE_DISABLED) {
                n->grip=1; n->grip_x=e.xbutton.x_root; n->grip_y=e.xbutton.y_root; n->grip_w=n->width; n->grip_h=n->height;
            } else if(e.xbutton.button==Button4||e.xbutton.button==Button5) {
                uint32_t mods=(e.xbutton.state&ShiftMask?TG_MOD_SHIFT:0)|
                    (e.xbutton.state&ControlMask?TG_MOD_CONTROL:0)|(e.xbutton.state&Mod1Mask?TG_MOD_ALT:0);
                tg_window_input(w,TG_WHEEL,e.xbutton.x,e.xbutton.y,
                    (uint32_t)(uint16_t)(int16_t)(e.xbutton.button==Button4?120:-120)|mods);
            } else if(e.xbutton.button==Button1||e.xbutton.button==Button3)
                tg_window_input(w,TG_POINTER_DOWN,e.xbutton.x,e.xbutton.y,e.xbutton.button==Button3?3u:1u);
            break;
        }
        case ButtonRelease:
            if(n->grip&&e.xbutton.button==Button1) n->grip=0;
            else if(e.xbutton.button==Button1||e.xbutton.button==Button3)
                tg_window_input(w,TG_POINTER_UP,e.xbutton.x,e.xbutton.y,e.xbutton.button==Button3?3u:1u);
            break;
        case LeaveNotify:
            tg_window_input(w,TG_POINTER_EXIT,e.xcrossing.x,e.xcrossing.y,0); break;
        case MotionNotify:
            if(n->grip&&(e.xmotion.state&Button1Mask)) {
                int width=n->grip_w+e.xmotion.x_root-n->grip_x,height=n->grip_h+e.xmotion.y_root-n->grip_y;
                if(width<1) width=1;
                if(width>16384) width=16384;
                if(height<1) height=1;
                if(height>16384) height=16384;
                XResizeWindow(n->display,n->window,(unsigned)width,(unsigned)height);
            } else { n->grip=0; tg_window_input(w,TG_POINTER_MOVE,e.xmotion.x,e.xmotion.y,0); }
            break;
        case KeyPress: {
            char text[256]; KeySym sym=0;
            Status status=0;
            int len=n->ic?Xutf8LookupString(n->ic,&e.xkey,text,sizeof text,&sym,&status):
                XLookupString(&e.xkey,text,sizeof text,&sym,NULL);
            if(n->ic&&status==XBufferOverflow) break;
            uint32_t mods=(e.xkey.state&ShiftMask?TG_MOD_SHIFT:0)|
                (e.xkey.state&ControlMask?TG_MOD_CONTROL:0)|(e.xkey.state&Mod1Mask?TG_MOD_ALT:0);
            uint32_t key=sym==XK_Left?TG_KEY_LEFT:sym==XK_Right?TG_KEY_RIGHT:sym==XK_Home?TG_KEY_HOME:
                sym==XK_End?TG_KEY_END:sym==XK_BackSpace?TG_KEY_BACKSPACE:sym==XK_Delete?TG_KEY_DELETE:
                (sym==XK_Return||sym==XK_KP_Enter)?TG_KEY_RETURN:sym==XK_Up?TG_KEY_ARROW_UP:sym==XK_Down?TG_KEY_ARROW_DOWN:
                (sym==XK_Tab||sym==XK_ISO_Left_Tab)?TG_KEY_TAB:sym==XK_Escape?TG_KEY_ESCAPE:
                sym==XK_Prior?TG_KEY_PRIOR:sym==XK_Next?TG_KEY_NEXT:sym==XK_Insert?TG_KEY_INSERT:
                sym>=XK_F1&&sym<=XK_F12?TG_KEY_F1+(uint32_t)(sym-XK_F1):0;
            if(!key&&(mods&(TG_MOD_CONTROL|TG_MOD_ALT))&&!(e.xkey.state&Mod5Mask)) {
                uint32_t scalar=keysym_scalar(sym);
                if(scalar>=32&&scalar<=0x10ffff) key=scalar;
            }
            if(key) tg_window_input(w,TG_KEY_DOWN,0,0,key|mods);
            else if(n->ic&&(status==XLookupChars||status==XLookupBoth)&&len>0&&len<=(int)sizeof text) {
                size_t offset=0;
                while(offset<(size_t)len) {
                    uint32_t scalar; size_t used;
                    if(tg_utf8_decode((const unsigned char *)text+offset,(size_t)len-offset,&scalar,&used)!=TG_OK) break;
                    if(scalar>=32&&scalar!=127) tg_window_input(w,TG_TEXT_INPUT,0,0,scalar);
                    offset+=used;
                }
            } else if(!n->ic||status==XLookupKeySym) {
                uint32_t scalar=keysym_scalar(sym);
                if(!scalar&&!n->ic&&len==1) scalar=(unsigned char)text[0];
                if(scalar>=32&&scalar!=127&&scalar<=0x10ffff&&!(scalar>=0xd800&&scalar<=0xdfff))
                    tg_window_input(w,TG_TEXT_INPUT,0,0,scalar);
            }
            break;
        }
        }
        return 1;
    }
    return painted;
}

int tg_window_set_title(tg_window *w,const char *title) {
    native *n=w->native; if(!n||!n->window) return 0;
    XStoreName(n->display,n->window,title);
    Atom property=XInternAtom(n->display,"_NET_WM_NAME",False);
    Atom utf8=XInternAtom(n->display,"UTF8_STRING",False);
    XChangeProperty(n->display,n->window,property,utf8,8,PropModeReplace,
                    (const unsigned char *)title,(int)strlen(title));
    XFlush(n->display); return 1;
}
int tg_window_set_size(tg_window *w,int width,int height) {
    native *n=w->native; if(!n||!n->window) return 0;
    XResizeWindow(n->display,n->window,(unsigned)width,(unsigned)height);
    XFlush(n->display); return 1;
}
int tg_window_set_visible(tg_window *w,int visible) {
    native *n=w->native; if(!n||!n->window) return 0;
    if(visible) XMapRaised(n->display,n->window);
    else XUnmapWindow(n->display,n->window);
    XFlush(n->display); return 1;
}

int tg_window_origin(tg_window *w,int *x,int *y) {
    native *n=w?w->native:NULL; Window child;
    if(!n||!n->window) return 0;
    return XTranslateCoordinates(n->display,n->window,DefaultRootWindow(n->display),0,0,x,y,&child)!=0;
}
int tg_window_screen(tg_window *w,int *width,int *height) {
    native *n=w->native; if(!n||!n->window) return 0;
    int screen=DefaultScreen(n->display);
    *width=DisplayWidth(n->display,screen); *height=DisplayHeight(n->display,screen);
    return *width>0&&*height>0;
}
int tg_window_set_geometry(tg_window *w,int width,int height,int pos,int x,int y,int nx,int ny) {
    if(!pos) return tg_window_set_size(w,width,height);
    native *n=w->native; int sw,sh;
    if(!n||!n->window||!tg_window_screen(w,&sw,&sh)) return 0;
    XMoveResizeWindow(n->display,n->window,nx?sw-width-x:x,ny?sh-height-y:y,(unsigned)width,(unsigned)height);
    XFlush(n->display); return 1;
}
int tg_window_limits(tg_window *w) {
    native *n=w->native; if(!n||!n->window) return 0;
    XSizeHints hints={0}; long supplied=0;
    (void)XGetWMNormalHints(n->display,n->window,&hints,&supplied);
    hints.flags|=PMinSize|PMaxSize;
    hints.min_width=w->resize_x?w->min_width:w->requested_width;
    hints.min_height=w->resize_y?w->min_height:w->requested_height;
    hints.max_width=w->resize_x?w->max_width:w->requested_width;
    hints.max_height=w->resize_y?w->max_height:w->requested_height;
    XSetWMNormalHints(n->display,n->window,&hints); XFlush(n->display); return 1;
}

int tg_window_override(tg_window *w,int enabled) {
    native *n=w?w->native:NULL;
    if(!n||!n->window) return 0;
    if(w->override_redirect==enabled) return 1;
    XWindowAttributes current;
    if(!XGetWindowAttributes(n->display,n->window,&current)) return 0;
    int x=0,y=0;
    if(!tg_window_origin(w,&x,&y)) return 0;
    int mapped=current.map_state!=IsUnmapped;
    if(mapped) XUnmapWindow(n->display,n->window);
    XSetWindowAttributes attrs={0}; attrs.override_redirect=enabled?True:False;
    XChangeWindowAttributes(n->display,n->window,CWOverrideRedirect,&attrs);
    XMoveWindow(n->display,n->window,x,y);
    if(mapped) XMapRaised(n->display,n->window);
    XFlush(n->display);
    w->override_redirect=enabled; w->dirty=w->layout_dirty=1;
    return 1;
}
/* The application's platform policy retains the system titlebar here. */
int tg_window_custom_caption(tg_window *w,int enabled) { (void)w;(void)enabled;return 0; }
int tg_window_caption_action(tg_window *w,int action) { (void)w;(void)action;return 0; }
int tg_window_caption_state(tg_window *w) { (void)w;return 0; }
