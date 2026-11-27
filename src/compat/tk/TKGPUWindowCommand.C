#include "TKGPUWindow.H"
#include "TKGPUHotkeys.H"
#include "TKGPUText.H"
#include "../../core/TKGPUCoreInternal.H"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
static int dimension(const char **s,int *value) {
    int n=0;
    const char *p=*s;
    if(*p<'0'||*p>'9') return 0;
    while(*p>='0'&&*p<='9') {
        n=n*10+(*p++-'0'); if(n>16384) return 0;
    }
    if(!n) return 0;
    *s=p; *value=n; return 1;
}
static int offset(const char **s,int *value,int *negative) {
    const char *p=*s;
    if(*p!='+'&&*p!='-') return 0;
    *negative=(*p++=='-');
    int sign=1;
    if(*p=='-') { sign=-1; p++; }
    else if(*p=='+') { p++; }
    if(*p<'0'||*p>'9') return 0;
    int n=0;
    while(*p>='0'&&*p<='9') { n=n*10+(*p++-'0'); if(n>65535) return 0; }
    *s=p; *value=sign*n; return 1;
}
static int bool_value(const char *s,int *v) {
    if(!strcmp(s,"1")||!strcmp(s,"true")||!strcmp(s,"yes")||!strcmp(s,"on")) { *v=1; return 1; }
    if(!strcmp(s,"0")||!strcmp(s,"false")||!strcmp(s,"no")||!strcmp(s,"off")) { *v=0; return 1; }
    return 0;
}
static int error(Tcl_Interp *i) {
    return tg_set_result(i,"unsupported or invalid root wm command",TG_ERROR);
}
int tg_window_command(Tcl_Interp *i,size_t n,char **a) {
    if(!strcmp(a[0],"tkgpu::titlebar")) {
        tg_window *w=Tk_MainWindow(i);
        if(n>=3&&a[1][0]=='.') {
            w=tg_window_for_widget(i,a[1]);
            a++; n--;
        }
        if(!w||n<2) return error(i);
        if(n==2&&!strcmp(a[1],"supported")) {
#ifdef _WIN32
            return tg_set_result(i,"1",TG_OK);
#else
            return tg_set_result(i,"0",TG_OK);
#endif
        }
        if(!strcmp(a[1],"attach")&&n>=3&&n<=TG_TITLEBAR_SOURCE_LIMIT+2) {
            tg_ui *u=tg_interp_ui(i); tg_id ids[TG_TITLEBAR_SOURCE_LIMIT];
            for(size_t k=2;k<n;k++) ids[k-2]=tg_ui_find(u,a[k]);
            tg_titlebar next;
            if(tg_titlebar_attach(&next,u,ids,n-2)!=TG_OK||!tg_window_custom_caption(w,1)) return error(i);
            w->caption=next; return tg_set_result(i,"",TG_OK);
        }
        if(n==2&&!strcmp(a[1],"detach")) {
            if(w->caption.enabled&&!tg_window_custom_caption(w,0)) return error(i);
            w->caption.enabled=0; return tg_set_result(i,"",TG_OK);
        }
        if(n==2&&!strcmp(a[1],"state")) {
            int state=tg_window_caption_state(w);
            return tg_set_result(i,state==2?"iconic":state==1?"zoomed":"normal",TG_OK);
        }
        if(n==2&&w->caption.enabled&&(!strcmp(a[1],"toggle")||!strcmp(a[1],"minimize"))) {
            if(!tg_window_caption_action(w,!strcmp(a[1],"toggle")?1:2)) return error(i);
            return tg_set_result(i,tg_window_caption_state(w)==1?"zoomed":tg_window_caption_state(w)==2?"iconic":"normal",TG_OK);
        }
        return error(i);
    }
    int animation=tg_animation_command(i,n,a);
    if(animation>=0) return animation;
    if(!strcmp(a[0],"tkgpu::text_backend")) {
        if(n!=1) return error(i);
        return tg_set_result(i,tg_text_native_enabled()?tg_text_native_backend():"bitmap",TG_OK);
    }
    if(!strcmp(a[0],"tkgpu::text_check")) {
        int width,height;
        if(n!=2||!tg_text_native_enabled()||tg_text_native_measure(a[1],2,&width,&height)!=TG_OK) return error(i);
        char b[64]; snprintf(b,sizeof b,"%d %d",width,height); return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[0],"tkgpu::gpu_stats")) {
        tg_window *w=Tk_MainWindow(i);
        if(n!=1||!w) return error(i);
        char b[384];
        int z=snprintf(b,sizeof b,"frames %" PRIu64 " instances %" PRIu64 " texture_hits %" PRIu64
            " texture_uploads %" PRIu64 " upload_bytes %" PRIu64 " layer_passes %" PRIu64,
            w->gpu_frames,w->gpu_instances,w->gpu_texture_hits,w->gpu_texture_uploads,w->gpu_upload_bytes,w->gpu_layer_passes);
        if(z<0||(size_t)z>=sizeof b) return error(i);
        return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[0],"tkgpu::frame_stats")) {
        tg_window *w=Tk_MainWindow(i);
        if(n!=1||!w) return error(i);
        tg_frame_stats st;
        tg_window_frame_stats(i,&st);
        char b[256];
        int z=snprintf(b,sizeof b,"present_frames %" PRIu64 " gpu_frames %" PRIu64
            " last_frame_us %" PRIu64 " total_frame_us %" PRIu64 " windows %" PRIu64,
            st.present_frames,st.gpu_frames,st.last_frame_us,st.total_frame_us,st.windows);
        if(z<0||(size_t)z>=sizeof b) return error(i);
        return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[0],"tkgpu::raster_backend")) {
        tg_window *w=Tk_MainWindow(i);
        if(n!=1||!w) return error(i);
        return tg_set_result(i,w->gpu_raster_active?"d3d11-hardware":"cpu",TG_OK);
    }
    if(!strcmp(a[0],"tkgpu::present_backend")) {
        tg_window *w=Tk_MainWindow(i);
        if(n!=1||!w) return error(i);
        return tg_set_result(i,w->gpu_present_active?"d3d11-hardware":"cpu",TG_OK);
    }
    int hotkey_cmd=tg_hotkey_command(i,n,a); if(hotkey_cmd>=0) return hotkey_cmd;
    int clipboard=tg_clipboard_command(i,n,a); if(clipboard>=0) return clipboard;
    int font=tg_font_command(i,n,a);
    if(font>=0) return font;
    if(!strcmp(a[0],"tk_popup")&&(n==4||n==5)) {
        tg_ui *u=tg_interp_ui(i);
        const tg_widget *g=u?tg_ui_get(u,tg_ui_find(u,a[1])):NULL;
        if(!g||g->kind!=TG_MENU) return tg_set_result(i,"invalid menu for tk_popup",TG_ERROR);
        char *endx=NULL,*endy=NULL; errno=0;
        long rx=strtol(a[2],&endx,10);
        long ry=strtol(a[3],&endy,10);
        if(errno||endx==a[2]||*endx||endy==a[3]||*endy) return tg_set_result(i,"invalid coordinates for tk_popup",TG_ERROR);
        tg_window *w=tg_window_for_widget(i,a[1]);
        if(!w) w=Tk_MainWindow(i);
        int ox=0,oy=0;
        if(w&&tg_window_origin(w,&ox,&oy)) { rx-=ox; ry-=oy; }
        if(rx<0) rx=0;
        if(ry<0) ry=0;
        char xs[32],ys[32],out[256];
        snprintf(xs,sizeof xs,"%ld",rx);
        snprintf(ys,sizeof ys,"%ld",ry);
        char *post_args[]={a[1],"post",xs,ys};
        if(tg_ui_command(u,4,post_args,out,sizeof out)!=TG_OK)
            return tg_set_result(i,"failed to post menu",TG_ERROR);
        if(n==5&&a[4][0]) {
            char *act_args[]={a[1],"activate",a[4]};
            (void)tg_ui_command(u,3,act_args,out,sizeof out);
        }
        if(i->tk_invalidate&&i->tk_host) i->tk_invalidate(i->tk_host);
        return tg_set_result(i,"",TG_OK);
    }
    int dialog=tg_dialog_command(i,n,a);
    if(dialog>=0) return dialog;
    if(n==2&&!strcmp(a[0],"tk")&&!strcmp(a[1],"windowingsystem")) {
        if(!Tk_MainWindow(i)) return error(i);
#ifdef _WIN32
        return tg_set_result(i,"win32",TG_OK);
#elif defined(TKGPU_COCOA)
        return tg_set_result(i,"aqua",TG_OK);
#else
        return tg_set_result(i,"x11",TG_OK);
#endif
    }
    if(n==3&&!strcmp(a[0],"winfo")&&(!strcmp(a[1],"rootx")||!strcmp(a[1],"rooty"))) {
        tg_window *w=tg_window_for_widget(i,a[2]); int x,y;
        tg_ui *u=tg_interp_ui(i);
        const tg_widget *g=tg_ui_get(u,tg_ui_find(u,a[2]));
        if(!w||!g||!tg_window_origin(w,&x,&y)) return error(i);
        char b[32]; snprintf(b,sizeof b,"%d",!strcmp(a[1],"rootx")?x+g->rect.x:y+g->rect.y);
        return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[0],"winfo")&&!strcmp(a[1],"containing")&&(n==4||(n==6&&!strcmp(a[2],"-displayof")))) {
        const char *win_path=(n==6)?a[3]:".";
        int rx=(int)strtol(a[n-2],NULL,10),ry=(int)strtol(a[n-1],NULL,10);
        tg_window *w=tg_window_for_widget(i,win_path);
        int ox=0,oy=0;
        if(!w||!tg_window_origin(w,&ox,&oy)) return tg_set_result(i,"",TG_OK);
        tg_ui *u=tg_interp_ui(i);
        tg_window_select(w,w->layout_width>0?w->layout_width:w->requested_width,w->layout_height>0?w->layout_height:w->requested_height);
        tg_id hit_id=tg_ui_hit(u,rx-ox,ry-oy);
        const tg_widget *g=tg_ui_get(u,hit_id);
        return tg_set_result(i,g?g->path:"",TG_OK);
    }
#ifdef _WIN32
    if(n==3&&!strcmp(a[0],"winfo")&&(!strcmp(a[1],"x")||!strcmp(a[1],"y"))) {
        tg_ui *u=tg_interp_ui(i);
        const tg_widget *g=tg_ui_get(u,tg_ui_find(u,a[2]));
        if(g&&(g->id==1||g->kind==TG_TOPLEVEL)) {
            tg_window *w=tg_window_for_widget(i,a[2]); RECT r;
            if(!w||!GetWindowRect((HWND)w->native,&r)) return error(i);
            char b[32]; snprintf(b,sizeof b,"%ld",(long)(a[1][0]=='x'?r.left:r.top));
            return tg_set_result(i,b,TG_OK);
        }
    }
#endif
    if(n==3&&!strcmp(a[0],"winfo")&&(!strcmp(a[1],"screenwidth")||!strcmp(a[1],"screenheight"))) {
        tg_window *w=tg_window_for_widget(i,a[2]); int width,height;
        if(!w||!tg_ui_find(tg_interp_ui(i),a[2])||!tg_window_screen(w,&width,&height)) return error(i);
        char b[32]; snprintf(b,sizeof b,"%d",!strcmp(a[1],"screenwidth")?width:height);
        return tg_set_result(i,b,TG_OK);
    }
    if(n==3&&!strcmp(a[0],"winfo")&&(!strcmp(a[1],"viewable")||!strcmp(a[1],"ismapped"))) {
        tg_window *w=tg_window_for_widget(i,a[2]);
        tg_ui *u=tg_interp_ui(i);
        const tg_widget *g=tg_ui_get(u,tg_ui_find(u,a[2]));
        if(!w||!g) return error(i);
        /* Posted popup menus have their own mapping lifecycle, independent
           of a withdrawn owner (including hidden-root application dialogs). */
        if(g->kind==TG_MENU) return -1;
        if(g->id==w->root) return tg_set_result(i,w->withdrawn?"0":"1",TG_OK);
        if(!strcmp(a[1],"viewable")&&w->withdrawn) return tg_set_result(i,"0",TG_OK);
        /* A mapped host does not imply that its child is geometry-managed. */
        return -1;
    }
#ifdef _WIN32
    if(n==3&&!strcmp(a[0],"winfo")&&!strcmp(a[1],"id")) {
        tg_window *w=tg_window_for_widget(i,a[2]); char b[64];
        if(!w) return error(i);
        tg_ui *u=tg_interp_ui(i);
        const tg_widget *g=tg_ui_get(u,tg_ui_find(u,a[2]));
        void *handle=w->native;
        if(g&&g->id!=w->root&&g->kind!=TG_TOPLEVEL) {
            void *child=tg_window_child_host(w,g->id);
            if(child) handle=child;
        }
        snprintf(b,sizeof b,"0x%llx",(unsigned long long)(uintptr_t)handle); return tg_set_result(i,b,TG_OK);
    }
    if(((!strcmp(a[0],"raise")||!strcmp(a[0],"lower"))&&(n>=2))||(!strcmp(a[0],"focus")&&(n==2||(n==3&&!strcmp(a[1],"-force"))))) {
        const char *path=n==3&&!strcmp(a[0],"focus")?a[2]:a[1];
        tg_window *w=tg_window_for_widget(i,path);
        if(strcmp(a[0],"focus")) {
            const tg_widget *g=tg_ui_get(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),path));
            if(!g) return error(i);
            if(g->id!=1&&g->kind!=TG_TOPLEVEL) return -1;
        }
        if(!strcmp(a[0],"raise")) {
            if(w) {
                if(w->override_redirect) SetWindowPos((HWND)w->native, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                else SetForegroundWindow((HWND)w->native);
            }
        } else if(!strcmp(a[0],"lower")) {
            if(w) SetWindowPos((HWND)w->native, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else if(!strcmp(a[0],"focus")) {
            if(!w) return error(i);
            SetFocus((HWND)w->native);
            if(tg_ui_focus(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),path))!=TG_OK) return error(i);
            if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        }
        return tg_set_result(i,"",TG_OK);
    }
#endif
#ifdef TKGPU_X11_HOST
    if((!strcmp(a[0],"focus")&&(n==2||(n==3&&!strcmp(a[1],"-force"))))||
       ((!strcmp(a[0],"raise")||!strcmp(a[0],"lower"))&&n==2)) {
        const char *path=n==3?a[2]:a[1];
        tg_ui *u=tg_interp_ui(i);
        tg_id id=tg_ui_find(u,path);
        const tg_widget *g=tg_ui_get(u,id);
        tg_window *w=tg_window_for_widget(i,path);
        if(!g||!w) return error(i);
        if(!strcmp(a[0],"focus")) {
            if(!tg_x11_focus(w)||tg_ui_focus(u,id)!=TG_OK) return error(i);
        } else {
            if(g->id!=w->root) return -1;
            if(!tg_x11_stack(w,!strcmp(a[0],"raise"))) return error(i);
        }
        return tg_set_result(i,"",TG_OK);
    }
#endif
    if(!strcmp(a[0],"grab")) {
        if(n==3&&(!strcmp(a[1],"set")||!strcmp(a[1],"release")||!strcmp(a[1],"status"))) {
            tg_window *w=tg_window_for_widget(i,a[2]); if(!w) return error(i);
            if(!strcmp(a[1],"status")) return tg_set_result(i,w->grabbed?"local":"none",TG_OK);
            if(!tg_window_grab(w,!strcmp(a[1],"set"))) return error(i);
            return tg_set_result(i,"",TG_OK);
        }
        if(n==3&&!strcmp(a[1],"current")) {
            tg_window *w=tg_window_for_widget(i,a[2]);
            if(w&&w->grabbed) { const tg_widget *g=tg_ui_get(tg_interp_ui(i),w->root); return tg_set_result(i,g?g->path:"",TG_OK); }
            return tg_set_result(i,"",TG_OK);
        }
        return error(i);
    }
    if(strcmp(a[0],"wm")) return -1;
    if(n<3) return error(i);
    tg_window *w=tg_window_for_widget(i,a[2]);
    const tg_widget *window_widget=w?tg_ui_get(tg_interp_ui(i),w->root):NULL;
    if(!w||!window_widget||strcmp(window_widget->path,a[2])) return error(i);
#ifdef _WIN32
    if(!strcmp(a[1],"iconbitmap")&&n==4) {
        if(!tg_window_icon_bitmap(w,a[3])) return tg_set_result(i,"cannot load window icon bitmap",TG_ERROR);
        return tg_set_result(i,"",TG_OK);
    }
#endif
#if defined(_WIN32)||defined(TKGPU_COCOA)||defined(TKGPU_X11_HOST)
    if(!strcmp(a[1],"attributes")) {
        char result[128];
        if(n==3) { snprintf(result,sizeof result,"-alpha %.9g -topmost %d",w->alpha,w->topmost); return tg_set_result(i,result,TG_OK); }
        if(n==4) {
            /* Tkinter's wantobjects mode must receive a real Tcl double
               (and integer for topmost), not a numeric-looking string. */
            if(i->host_owner) {
                if(!strcmp(a[3],"-alpha")) {
                    char b[64]; snprintf(b,sizeof b,"%.17g",w->alpha);
                    return tg_set_result_kind(i,b,TG_RESULT_DOUBLE);
                }
                if(!strcmp(a[3],"-topmost"))
                    return tg_set_result_kind(i,w->topmost?"1":"0",TG_RESULT_INTEGER);
                return error(i);
            }
            Tcl_Obj *value=NULL;
            if(!strcmp(a[3],"-alpha")) value=Tcl_NewDoubleObj(w->alpha);
            else if(!strcmp(a[3],"-topmost")) value=Tcl_NewIntObj(w->topmost);
            else return error(i);
            if(!value) return error(i);
            Tcl_SetObjResult(i,value);
            return TG_OK;
        }
        if(n<5||n%2==0) return error(i);
        for(size_t at=3;at<n;at+=2) {
            if(!strcmp(a[at],"-alpha")) {
                char *end; errno=0; double alpha=strtod(a[at+1],&end);
                if(errno||end==a[at+1]||*end||!isfinite(alpha)) return error(i);
                /* Tk clamps out-of-range opacity; fade steps may overshoot
                   zero/one due to floating-point rounding. */
                if(alpha<0.0) alpha=0.0;
                else if(alpha>1.0) alpha=1.0;
#ifdef _WIN32
#ifdef TKGPU_LEGACY_WINDOWS
                /* Win9x has no guaranteed layered-window API. Resolve it
                   only if available, preserving the normal opaque path. */
                HWND hwnd=(HWND)w->native;
                typedef BOOL (WINAPI *layered_fn)(HWND,COLORREF,BYTE,DWORD);
                layered_fn layered=(layered_fn)GetProcAddress(GetModuleHandleA("user32.dll"),"SetLayeredWindowAttributes");
                const LONG layered_style=0x00080000L;
                if(alpha<1&&!layered) return error(i);
                LONG style=GetWindowLongA(hwnd,GWL_EXSTYLE);
                LONG next=alpha<1?style|layered_style:style&~layered_style;
                SetLastError(0); SetWindowLongA(hwnd,GWL_EXSTYLE,next);
                if(GetLastError()) return error(i);
                if(alpha<1&&!layered(hwnd,0,(BYTE)(alpha*255+0.5),2)) {
                    SetWindowLongA(hwnd,GWL_EXSTYLE,style); return error(i);
                }
#else
                HWND hwnd=(HWND)w->native; LONG_PTR style=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
                SetLastError(0); SetWindowLongPtrW(hwnd,GWL_EXSTYLE,alpha<1?style|WS_EX_LAYERED:style&~WS_EX_LAYERED);
                if(GetLastError()) return error(i);
                if(alpha<1&&!SetLayeredWindowAttributes(hwnd,0,(BYTE)(alpha*255+0.5),LWA_ALPHA)) return error(i);
#endif
                InvalidateRect(hwnd,NULL,FALSE);
#elif defined(TKGPU_COCOA)
                if(!tg_cocoa_alpha(w,alpha)) return error(i);
#else
                if(!tg_x11_alpha(w,alpha)) return error(i);
#endif
                w->alpha=alpha;
            } else if(!strcmp(a[at],"-topmost")) {
                int enabled; if(!bool_value(a[at+1],&enabled)) return error(i);
#ifdef _WIN32
                if(!SetWindowPos((HWND)w->native,enabled?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE)) return error(i);
#elif defined(TKGPU_COCOA)
                if(!tg_cocoa_topmost(w,enabled)) return error(i);
#else
                if(!tg_x11_topmost(w,enabled)) return error(i);
#endif
                w->topmost=enabled;
            } else return error(i);
        }
        return tg_set_result(i,"",TG_OK);
    }
#endif
    if(!strcmp(a[1],"iconname")) {
        if(n==3) return tg_set_result(i,w->icon_name,TG_OK);
        if(n!=4||strlen(a[3])>=sizeof w->icon_name) return error(i);
        strcpy(w->icon_name,a[3]); return tg_set_result(i,"",TG_OK);
    }
#if defined(_WIN32)||defined(TKGPU_COCOA)
    if(!strcmp(a[1],"transient")) {
        if(n==3) { const tg_widget *parent=w->transient_owner?tg_ui_get(tg_interp_ui(i),w->transient_owner->root):NULL; return tg_set_result(i,parent?parent->path:"",TG_OK); }
        if(n!=4) return error(i);
        tg_window *parent=*a[3]?tg_window_for_widget(i,a[3]):NULL;
        if(*a[3]&&(!parent||parent==w)) return error(i);
        for(tg_window *p=parent;p;p=p->transient_owner) if(p==w) return error(i);
#ifdef _WIN32
        SetLastError(0);
#ifdef TKGPU_LEGACY_WINDOWS
        SetWindowLongA((HWND)w->native,GWL_HWNDPARENT,(LONG)(LONG_PTR)(parent?parent->native:NULL));
#else
        SetWindowLongPtrW((HWND)w->native,GWLP_HWNDPARENT,(LONG_PTR)(parent?parent->native:NULL));
#endif
        if(GetLastError()) return error(i);
        if(parent&&parent->native) {
            SetWindowPos((HWND)w->native,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        }
#else
        if(!tg_cocoa_transient(w,parent)) return error(i);
#endif
        w->transient_owner=parent; return tg_set_result(i,"",TG_OK);
    }
#endif
    if(!strcmp(a[1],"overrideredirect")) {
        if(n==3) return tg_set_result(i,w->override_redirect?"1":"0",TG_OK);
        int enabled;
        if(n!=4||!bool_value(a[3],&enabled)||!tg_window_override(w,enabled)) return error(i);
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"protocol")) {
        if(n==3) return tg_set_result(i,w->close_protocol[0]?"WM_DELETE_WINDOW":"",TG_OK);
        if((n!=4&&n!=5)||strcmp(a[3],"WM_DELETE_WINDOW")) return error(i);
        if(n==4) return tg_set_result(i,w->close_protocol,TG_OK);
        if(strlen(a[4])>=sizeof w->close_protocol) return error(i);
        strcpy(w->close_protocol,a[4]); return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"title")) {
        if(n==3) return tg_set_result(i,w->title,TG_OK);
        if(n!=4||strlen(a[3])>=sizeof w->title||!tg_window_set_title(w,a[3])) return error(i);
        strcpy(w->title,a[3]); return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"geometry")) {
        if(n==3) {
            char b[96];
            if(w->position_set)
                snprintf(b,sizeof b,"%dx%d%c%d%c%d",w->requested_width,w->requested_height,
                    w->negative_x?'-':'+',w->position_x,w->negative_y?'-':'+',w->position_y);
            else snprintf(b,sizeof b,"%dx%d",w->requested_width,w->requested_height);
            return tg_set_result(i,b,TG_OK);
        }
        if(n!=4) return error(i);
        int width=w->requested_width,height=w->requested_height,x=0,y=0,nx=0,ny=0,pos=0;
        const char *p=a[3]; if(*p=='=') p++;
        int has_size=*p!='+'&&*p!='-';
        if(has_size) {
            if(!dimension(&p,&width)||*p++!='x'||!dimension(&p,&height)) return error(i);
        }
        if(*p) {
            if(!offset(&p,&x,&nx)||!offset(&p,&y,&ny)||*p) return error(i);
            pos=1;
        }
        if((size_t)width*height>16u*1024u*1024u) return error(i);
        if(width<w->min_width) width=w->min_width;
        if(height<w->min_height) height=w->min_height;
        if(width>w->max_width) width=w->max_width;
        if(height>w->max_height) height=w->max_height;
        w->requested_width=width; w->requested_height=height; w->dirty=w->layout_dirty=1; tg_window_select(w,width,height);
        if(!tg_window_set_geometry(w,width,height,pos,x,y,nx,ny)) return error(i);
        if(has_size) w->explicit_size=1;
        if(pos) { w->position_set=1; w->position_x=x; w->position_y=y; w->negative_x=nx; w->negative_y=ny; }
        if(!tg_window_limits(w)) return error(i);
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"minsize")||!strcmp(a[1],"maxsize")||!strcmp(a[1],"resizable")) {
        int resize=!strcmp(a[1],"resizable"),minimum=!strcmp(a[1],"minsize");
        int *x=resize?&w->resize_x:minimum?&w->min_width:&w->max_width;
        int *y=resize?&w->resize_y:minimum?&w->min_height:&w->max_height;
        if(n==3) {
            char b[48]; snprintf(b,sizeof b,"%d %d",*x,*y); return tg_set_result(i,b,TG_OK);
        }
        if(n!=5) return error(i);
        int vx,vy; const char *px=a[3],*py=a[4];
        if(resize) { if(!bool_value(px,&vx)||!bool_value(py,&vy)) return error(i); }
        else if(!dimension(&px,&vx)||*px||!dimension(&py,&vy)||*py) return error(i);
        if(!resize&&(minimum?(vx>w->max_width||vy>w->max_height):(vx<w->min_width||vy<w->min_height))) return error(i);
        int oldx=*x,oldy=*y; *x=vx; *y=vy;
        if(!tg_window_limits(w)) { *x=oldx; *y=oldy; return error(i); }
        return tg_set_result(i,"",TG_OK);
    }
    if(n==3&&(!strcmp(a[1],"withdraw")||!strcmp(a[1],"deiconify"))) {
        int visible=!strcmp(a[1],"deiconify");
        int changed=w->withdrawn==visible;
        if(visible) tg_window_fit_override(w);
        if(!tg_window_set_visible(w,visible)) return error(i);
        w->withdrawn=!visible; w->dirty=1;
        if(changed) (void)tg_ui_post(tg_interp_ui(i),(tg_event){visible?TG_MAP:TG_UNMAP,w->root,0,0,0});
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"state")&&n==3) {
        int state=tg_window_caption_state(w);
        return tg_set_result(i,w->withdrawn?"withdrawn":state==1?"zoomed":state==2?"iconic":"normal",TG_OK);
    }
#ifdef _WIN32
    if((n==4&&!strcmp(a[1],"state"))||(n==3&&!strcmp(a[1],"iconify"))) {
        const char *state=n==3?"iconic":a[3];
        HWND hwnd=(HWND)w->native;
        if(!hwnd) return error(i);
        if(!strcmp(state,"withdrawn")) {
            if(!tg_window_set_visible(w,0)) return error(i);
            w->withdrawn=1;
        } else {
            int cmd=!strcmp(state,"normal")?SW_RESTORE:!strcmp(state,"zoomed")?SW_MAXIMIZE:!strcmp(state,"iconic")?SW_MINIMIZE:-1;
            if(cmd<0) return error(i);
            w->map_pending=0; w->withdrawn=0; ShowWindow(hwnd,cmd);
        }
        w->dirty=w->layout_dirty=1; return tg_set_result(i,"",TG_OK);
    }
#endif
    return error(i);
}
