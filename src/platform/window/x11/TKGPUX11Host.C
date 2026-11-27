#include "../../../compat/tk/TKGPUWindow.H"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include "../../../widgets/TKGPUWidgetInternal.H"
#include "../../../widgets/ttk/tree/TKGPUTree.H"
#include "../../time/TKGPUClock.H"
#include "TKGPUUtf8.H"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct { Display *display; Window window; Atom close; int width,height,exposed,grip,grip_x,grip_y,grip_w,grip_h; XImage *image; Pixmap frame; int frame_width,frame_height; int direct_rgb; XIM im; XIC ic; Cursor cursors[XC_num_glyphs]; unsigned cursor_shape; int pointer_x,pointer_y,pointer_valid,focus_pending,force_frame; uint64_t last_frame_ms,resize_until_ms,resize_batch_ms,geometry_ready_ms; } native;
static void update_cursor(tg_window *w,native *n) {
    if(!n->pointer_valid) return;
    tg_ui *u=tg_interp_ui(w->interp);
    const tg_widget *hit=tg_ui_get(u,tg_ui_hit(u,n->pointer_x,n->pointer_y));
    const tg_widget *g=hit;
    const char *name=NULL;
    while(g) { if(g->cursor_name[0]) { name=g->cursor_name; break; } g=tg_ui_get(u,g->parent); }
    unsigned shape=XC_left_ptr;
    if(name) {
        if(!strcmp(name,"hand2")||!strcmp(name,"hand1")||!strcmp(name,"hand")) shape=XC_hand2;
        else if(!strcmp(name,"xterm")||!strcmp(name,"ibeam")) shape=XC_xterm;
        else if(!strcmp(name,"watch")||!strcmp(name,"wait")) shape=XC_watch;
        else if(!strcmp(name,"fleur")||!strcmp(name,"move")||!strcmp(name,"size_all")) shape=XC_fleur;
        else if(!strcmp(name,"cross")||!strcmp(name,"crosshair")) shape=XC_crosshair;
        else if(!strcmp(name,"sb_h_double_arrow")||!strcmp(name,"size_we")) shape=XC_sb_h_double_arrow;
        else if(!strcmp(name,"sb_v_double_arrow")||!strcmp(name,"size_ns")) shape=XC_sb_v_double_arrow;
        else if(!strcmp(name,"bottom_right_corner")||!strcmp(name,"size_nw_se")) shape=XC_bottom_right_corner;
        else if(!strcmp(name,"bottom_left_corner")||!strcmp(name,"size_ne_sw")) shape=XC_bottom_left_corner;
        else if(!strcmp(name,"no")||!strcmp(name,"not_allowed")) shape=XC_X_cursor;
    } else if(hit&&(hit->kind==TG_ENTRY||hit->kind==TG_TEXT)) shape=XC_xterm;
    else if(hit&&hit->kind==TG_SIZEGRIP) shape=XC_bottom_right_corner;
    if(u->drag_tree) shape=XC_sb_h_double_arrow;
    else if(hit&&hit->kind==TG_TREEVIEW) {
        char xs[32],ys[32],out[64];
        snprintf(xs,sizeof xs,"%d",n->pointer_x-hit->rect.x);
        snprintf(ys,sizeof ys,"%d",n->pointer_y-hit->rect.y);
        char *args[]={ (char *)hit->path,"identify","region",xs,ys };
        if(tg_tree_command(u,(tg_widget *)hit,5,args,out,sizeof out)==TG_OK&&!strcmp(out,"separator")) shape=XC_sb_h_double_arrow;
    }
    if(n->cursor_shape==shape+1) return;
    if(!n->cursors[shape]) n->cursors[shape]=XCreateFontCursor(n->display,shape);
    XDefineCursor(n->display,n->window,n->cursors[shape]); n->cursor_shape=shape+1;
    XFlush(n->display);
}
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
static int paint(tg_window *w,native *n,int content_changed) {
    int width=tg_canvas_width(w->canvas),height=tg_canvas_height(w->canvas);
    if(width<=0||height<=0||!ensure_image(n,width,height)) return 0;
    int screen=DefaultScreen(n->display);
    XImage *image=n->image;
    int full_upload=0;
    if(!n->frame||n->frame_width!=width||n->frame_height!=height) {
        Pixmap next=XCreatePixmap(n->display,n->window,(unsigned)width,(unsigned)height,(unsigned)DefaultDepth(n->display,screen));
        if(!next) return 0;
        if(n->frame) XFreePixmap(n->display,n->frame);
        n->frame=next; n->frame_width=width; n->frame_height=height;
        full_upload=1;
    }
    if(w->map_pending&&!w->withdrawn) full_upload=1;

    tg_rect damage={0,0,width,height};
    int has_damage=0;
    if(content_changed) {
        if(full_upload) {
            tg_rect ignored;
            (void)tg_canvas_damage(w->canvas,&ignored);
            has_damage=1;
        } else if(tg_canvas_damage(w->canvas,&damage)) {
            int l=damage.x<0?0:damage.x;
            int t=damage.y<0?0:damage.y;
            int r=(damage.x+damage.w>width)?width:(damage.x+damage.w);
            int b=(damage.y+damage.h>height)?height:(damage.y+damage.h);
            if(r>l&&b>t) {
                damage=(tg_rect){l,t,r-l,b-t};
                has_damage=1;
            }
        }
    }

    if(has_damage) {
        const uint32_t *p=tg_canvas_pixels(w->canvas);
        int dx=damage.x,dy=damage.y,dw=damage.w,dh=damage.h;
        if(n->direct_rgb) {
            size_t row_bytes=(size_t)dw*sizeof *p;
            for(int y=dy;y<dy+dh;y++) {
                memcpy(image->data+(size_t)y*image->bytes_per_line+(size_t)dx*4,
                       p+(size_t)y*width+dx,row_bytes);
            }
        } else for(int y=dy;y<dy+dh;y++) for(int x=dx;x<dx+dw;x++) {
            uint32_t rgb=p[(size_t)y*width+x];
            unsigned long pixel=channel((rgb>>16)&255,image->red_mask)|
                channel((rgb>>8)&255,image->green_mask)|channel(rgb&255,image->blue_mask);
            XPutPixel(image,x,y,pixel);
        }
        /* Upload only the damaged region offscreen, then copy to the window. */
        XPutImage(n->display,n->frame,DefaultGC(n->display,screen),image,
                  dx,dy,dx,dy,(unsigned)dw,(unsigned)dh);
    }

    if(w->map_pending&&!w->withdrawn) {
        /* Supply the first complete frame as the window background before
           mapping, so the compositor never sees a default black rectangle. */
        XSetWindowBackgroundPixmap(n->display,n->window,n->frame);
        XMapRaised(n->display,n->window);
        /* Keep later resize exposures solid, rather than tiling the first
           frame's background pixmap across newly exposed client space. */
        const tg_widget *root=tg_ui_get(tg_interp_ui(w->interp),w->root);
        uint32_t bg=root?root->bg:0xf0f2f5;
        unsigned long background=channel((bg>>16)&255,image->red_mask)|
            channel((bg>>8)&255,image->green_mask)|channel(bg&255,image->blue_mask);
        XSetWindowBackground(n->display,n->window,background);
        w->map_pending=0;
    }

    if(n->exposed||full_upload) {
        XCopyArea(n->display,n->frame,n->window,DefaultGC(n->display,screen),0,0,(unsigned)width,(unsigned)height,0,0);
    } else if(has_damage) {
        XCopyArea(n->display,n->frame,n->window,DefaultGC(n->display,screen),
                  damage.x,damage.y,(unsigned)damage.w,(unsigned)damage.h,damage.x,damage.y);
    }
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
    /* ForgetGravity clears the entire client surface on every WM resize,
       flashing the background before the next complete frame arrives. */
    XSetWindowAttributes attributes={0}; attributes.bit_gravity=NorthWestGravity;
    XChangeWindowAttributes(n->display,n->window,CWBitGravity,&attributes);
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
    w->map_pending=1; XFlush(n->display); w->native=n; return 1;
}
void tg_window_close_native(tg_window *w) {
    native *n=w->native;
    if(n&&n->ic) { XDestroyIC(n->ic); n->ic=NULL; }
    if(n&&n->im) { XCloseIM(n->im); n->im=NULL; }
    if(n&&n->frame) { XFreePixmap(n->display,n->frame); n->frame=0; }
    if(n) for(unsigned k=0;k<XC_num_glyphs;k++) if(n->cursors[k]) { XFreeCursor(n->display,n->cursors[k]); n->cursors[k]=0; }
    if(n&&n->window) { XDestroyWindow(n->display,n->window); n->window=0; XFlush(n->display); }
    if(n&&n->image) { XDestroyImage(n->image); n->image=NULL; }
    w->alive=0;
}
void tg_window_free_native(tg_window *w) {
    native *n=w->native; if(!n) return;
    tg_window_close_native(w); if(n->image) XDestroyImage(n->image);
    XCloseDisplay(n->display); free(n); w->native=NULL;
}
static void flush_motion(tg_window *w,native *n,int *has_motion,int mx,int my,int mx_root,int my_root,unsigned mstate) {
    if(!*has_motion) return;
    *has_motion=0;
    n->pointer_x=mx; n->pointer_y=my; n->pointer_valid=1;
    if(n->grip&&(mstate&Button1Mask)) {
        int width=n->grip_w+mx_root-n->grip_x,height=n->grip_h+my_root-n->grip_y;
        if(width<1) width=1;
        if(width>16384) width=16384;
        if(height<1) height=1;
        if(height>16384) height=16384;
        XResizeWindow(n->display,n->window,(unsigned)width,(unsigned)height);
    } else {
        n->grip=0;
        tg_window_input(w,TG_POINTER_MOVE,mx,my,0);
    }
}
int tg_window_poll_native(tg_window *w) {
    native *n=w->native; if(!n||!n->window) return 0;
    uint64_t now=0; (void)tg_clock_now(&now);
    int events=0,configured=0;
    int previous_width=n->width,previous_height=n->height;
    int has_motion=0,mx=0,my=0,mx_root=0,my_root=0;
    unsigned mstate=0;

    /* Drain a batch of X11 events first and coalesce MotionNotify/ConfigureNotify
       bursts so dragging and resizing render smooth intermediate frames. */
    while(events<32&&XPending(n->display)) {
        XEvent e; XNextEvent(n->display,&e);
        events++;
        if(n->ic&&XFilterEvent(&e,None)) continue;
        switch(e.type) {
        case ConfigureNotify:
            n->width=e.xconfigure.width; n->height=e.xconfigure.height;
            configured=1;
            break;
        case FocusIn:
            if(n->ic) XSetICFocus(n->ic);
            tg_window_input(w,TG_FOCUS_IN,0,0,0);
            break;
        case FocusOut:
            if(n->ic) XUnsetICFocus(n->ic);
            tg_window_input(w,TG_FOCUS_OUT,0,0,0);
            break;
        case Expose:
            n->exposed=1;
            break;
        case ClientMessage:
            if((Atom)e.xclient.data.l[0]==n->close) { tg_window_request_close(w); }
            break;
        case DestroyNotify:
            n->window=0; w->alive=0;
            break;
        case ButtonPress: {
            flush_motion(w,n,&has_motion,mx,my,mx_root,my_root,mstate);
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
            flush_motion(w,n,&has_motion,mx,my,mx_root,my_root,mstate);
            if(n->grip&&e.xbutton.button==Button1) n->grip=0;
            else if(e.xbutton.button==Button1||e.xbutton.button==Button3)
                tg_window_input(w,TG_POINTER_UP,e.xbutton.x,e.xbutton.y,e.xbutton.button==Button3?3u:1u);
            break;
        case LeaveNotify:
            flush_motion(w,n,&has_motion,mx,my,mx_root,my_root,mstate);
            tg_window_input(w,TG_POINTER_EXIT,e.xcrossing.x,e.xcrossing.y,0);
            break;
        case MotionNotify:
            has_motion=1; mx=e.xmotion.x; my=e.xmotion.y;
            mx_root=e.xmotion.x_root; my_root=e.xmotion.y_root;
            mstate=e.xmotion.state;
            break;
        case KeyPress: {
            flush_motion(w,n,&has_motion,mx,my,mx_root,my_root,mstate);
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
        if(!n->window) break;
    }
    flush_motion(w,n,&has_motion,mx,my,mx_root,my_root,mstate);
    if(!n->window) return events>0;

    if(configured&&(n->width!=previous_width||n->height!=previous_height)) {
        w->dirty=w->layout_dirty=1; n->resize_until_ms=now+150;
        tg_window_input(w,TG_CONFIGURE,n->width,n->height,0);
    }
    if(w->map_pending&&!w->withdrawn) tg_window_fit_override(w);
    /* Dispatch geometry/widget callbacks before presenting their new layout,
       pacing intermediate frames at 30-60 FPS during active resize/drag. */
    int painted=0;
    tg_ui *scene=tg_interp_ui(w->interp);
    uint64_t interval=now<n->resize_until_ms?33:16;
    if(!w->withdrawn&&(w->map_pending||n->force_frame||
       now-n->last_frame_ms>=interval)) {
        if(w->dirty||w->layout_dirty||n->exposed||w->map_pending) {
            (void)tg_window_select(w,n->width,n->height);
            if(!scene->len) {
                uint64_t begin=now;
                int content_changed=tg_window_refresh(w,n->width,n->height);
                if((content_changed||n->exposed)&&paint(w,n,content_changed)) {
                    n->exposed=0; n->force_frame=0; n->resize_batch_ms=0; painted=1;
                    (void)tg_clock_now(&n->last_frame_ms);
                    w->present_frames++; w->last_frame_us=(n->last_frame_ms-begin)*1000;
                    w->present_time_us+=w->last_frame_us;
                }
            }
        }
    }
    update_cursor(w,n);
    if(n->focus_pending&&!w->withdrawn&&!w->map_pending) {
        XWindowAttributes attributes;
        if(XGetWindowAttributes(n->display,n->window,&attributes)&&attributes.map_state==IsViewable) {
            XSetInputFocus(n->display,n->window,RevertToParent,CurrentTime);
            n->focus_pending=0; XFlush(n->display);
        }
    }
    return events>0||painted;
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
    n->width=width; n->height=height; n->force_frame=1;
    XFlush(n->display); return 1;
}
int tg_x11_client_size(tg_window *w,int *width,int *height) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    *width=n->width; *height=n->height; return 1;
}
int tg_window_set_visible(tg_window *w,int visible) {
    native *n=w->native; if(!n||!n->window) return 0;
    if(visible) {
        XWindowAttributes attributes;
        if(XGetWindowAttributes(n->display,n->window,&attributes)&&attributes.map_state!=IsUnmapped&&!w->withdrawn) return 1;
        w->map_pending=1; w->dirty=w->layout_dirty=1; n->exposed=1;
    }
    else { w->map_pending=0; n->focus_pending=0; XUnmapWindow(n->display,n->window); }
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
    n->width=width; n->height=height; n->force_frame=1;
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
    /* Also tell reparenting window managers to remove their existing frame. */
    unsigned long hints[5]={2,0,enabled?0:1,0,0};
    Atom motif=XInternAtom(n->display,"_MOTIF_WM_HINTS",False);
    XChangeProperty(n->display,n->window,motif,motif,32,PropModeReplace,(unsigned char *)hints,5);
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

/* EWMH opacity is interpreted by the compositor; an absent compositor keeps
   the window opaque without making an otherwise valid wm command fail. */
int tg_x11_alpha(tg_window *w,double alpha) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    Atom property=XInternAtom(n->display,"_NET_WM_WINDOW_OPACITY",False);
    unsigned long value=(unsigned long)(alpha*4294967295.0);
    XChangeProperty(n->display,n->window,property,XA_CARDINAL,32,PropModeReplace,(unsigned char *)&value,1);
    XFlush(n->display); return 1;
}
int tg_x11_topmost(tg_window *w,int enabled) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    XEvent event={0}; event.xclient.type=ClientMessage;
    event.xclient.window=n->window; event.xclient.format=32;
    event.xclient.message_type=XInternAtom(n->display,"_NET_WM_STATE",False);
    event.xclient.data.l[0]=enabled?1:0;
    event.xclient.data.l[1]=(long)XInternAtom(n->display,"_NET_WM_STATE_ABOVE",False);
    event.xclient.data.l[3]=1;
    XSendEvent(n->display,DefaultRootWindow(n->display),False,SubstructureRedirectMask|SubstructureNotifyMask,&event);
    XFlush(n->display); return 1;
}
int tg_x11_focus(tg_window *w) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    if(w->withdrawn) return 1;
    if(w->map_pending) { n->focus_pending=1; return 1; }
    XWindowAttributes attributes;
    if(!XGetWindowAttributes(n->display,n->window,&attributes)) return 0;
    if(attributes.map_state==IsViewable) XSetInputFocus(n->display,n->window,RevertToParent,CurrentTime);
    XFlush(n->display); return 1;
}
int tg_x11_stack(tg_window *w,int raised) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    if(raised) XRaiseWindow(n->display,n->window); else XLowerWindow(n->display,n->window);
    XFlush(n->display); return 1;
}
