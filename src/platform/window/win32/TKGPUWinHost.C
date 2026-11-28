#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "../../../compat/tk/TKGPUWindow.H"
#include "../../../widgets/TKGPUWidgetInternal.H"
#include "TKGPUUtf8.H"
#ifdef TKGPU_D3D11_PRESENT
#include "TKGPUD3D11Present.H"
#endif
#if defined(TKGPU_LEGACY_WINDOWS) && !defined(TKGPU_WINDOWS_XP)
static const char classname[]="TKGPU.Preview.MainWindow";
#define TG_DEFPROC DefWindowProcA
#define TG_GETUSER GetWindowLongPtrA
#define TG_SETUSER SetWindowLongPtrA
#define TG_CREATESTRUCT CREATESTRUCTA
#define TG_WNDCLASS WNDCLASSA
#define TG_REGISTER RegisterClassA
#define TG_CREATE CreateWindowA
#define TG_PEEK PeekMessageA
#define TG_DISPATCH DispatchMessageA
#define TG_TITLE "tk"
#else
static const WCHAR classname[]=L"TKGPU.Preview.MainWindow.Unicode";
#define TG_DEFPROC DefWindowProcW
#define TG_GETUSER GetWindowLongPtrW
#define TG_SETUSER SetWindowLongPtrW
#define TG_CREATESTRUCT CREATESTRUCTW
#define TG_WNDCLASS WNDCLASSW
#define TG_REGISTER RegisterClassW
#define TG_CREATE CreateWindowW
#define TG_PEEK PeekMessageW
#define TG_DISPATCH DispatchMessageW
#define TG_TITLE L"tk"
#ifndef WM_UNICHAR
#define WM_UNICHAR 0x0109
#endif
#endif
static HICON framework_icon(int width,int height) {
    MEMORY_BASIC_INFORMATION region;
    HINSTANCE module=GetModuleHandleA(NULL);
    /* The host may be python.exe or another application. Resolve the module
       containing TKGPU's own data without adding a newer Windows API import. */
    if(VirtualQuery(classname,&region,sizeof region)) module=(HINSTANCE)region.AllocationBase;
    return (HICON)LoadImageA(module,MAKEINTRESOURCEA(101),IMAGE_ICON,width,height,LR_SHARED);
}
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    tg_window *w=(tg_window *)(LONG_PTR)TG_GETUSER(hwnd,GWLP_USERDATA);
    if(msg==WM_NCCREATE) {
        TG_CREATESTRUCT *c=(TG_CREATESTRUCT *)lp; w=c->lpCreateParams;
        TG_SETUSER(hwnd,GWLP_USERDATA,(LONG_PTR)w); w->native=hwnd;
    }
    if(!w) return TG_DEFPROC(hwnd,msg,wp,lp);
    RECT view; if(GetClientRect(hwnd,&view)&&view.right>0&&view.bottom>0) tg_window_select(w,view.right,view.bottom);
    switch(msg) {
    case WM_NCCALCSIZE:
        if(wp&&w->caption.enabled&&w->root!=1) return 0;
        break;
    case WM_NCHITTEST: {
        LRESULT hit=TG_DEFPROC(hwnd,msg,wp,lp);
        if(hit==HTCLIENT&&w->caption.enabled) {
            POINT p={(short)LOWORD(lp),(short)HIWORD(lp)};
            if(ScreenToClient(hwnd,&p)) {
                if(!IsZoomed(hwnd)&&(w->resize_x||w->resize_y)) {
                    const int edge=6;
                    int left=w->resize_x&&p.x<edge;
                    int right=w->resize_x&&p.x>=view.right-edge;
                    int top=w->resize_y&&p.y<edge;
                    int bottom=w->resize_y&&p.y>=view.bottom-edge;
                    if(top&&left) return HTTOPLEFT;
                    if(top&&right) return HTTOPRIGHT;
                    if(bottom&&left) return HTBOTTOMLEFT;
                    if(bottom&&right) return HTBOTTOMRIGHT;
                    if(top) return HTTOP;
                    if(bottom) return HTBOTTOM;
                    if(left) return HTLEFT;
                    if(right) return HTRIGHT;
                }
                if(tg_titlebar_hit(&w->caption,tg_interp_ui(w->interp),p.x,p.y)) return HTCAPTION;
            }
        }
        return hit;
    }
    case WM_SETCURSOR: {
        if(LOWORD(lp)!=HTCLIENT) break;
        POINT p;
        if(!GetCursorPos(&p)||!ScreenToClient(hwnd,&p)) break;
        tg_ui *u=tg_interp_ui(w->interp);
        int dialog_edges=tg_base_dialog_resize_hit(u,p.x,p.y);
        if(dialog_edges) {
            unsigned res=32512;
            if((dialog_edges==(1|4))||(dialog_edges==(2|8))) res=32642;
            else if((dialog_edges==(2|4))||(dialog_edges==(1|8))) res=32643;
            else if(dialog_edges&(1|2)) res=32644;
            else if(dialog_edges&(4|8)) res=32645;
            HCURSOR c=LoadCursorA(NULL,MAKEINTRESOURCEA(res));
            if(c) { SetCursor(c); return TRUE; }
        }
        const tg_widget *g=tg_ui_get(u,tg_ui_hit(u,p.x,p.y));
        const int sizegrip=g&&g->kind==TG_SIZEGRIP;
        const char *name=NULL;
        while(g) {
            if(g->cursor_name[0]) { name=g->cursor_name; break; }
            g=tg_ui_get(u,g->parent);
        }
        unsigned resource=32512; /* IDC_ARROW */
        if(name) {
            if(!strcmp(name,"none")) { SetCursor(NULL); return TRUE; }
            if(!strcmp(name,"fleur")||!strcmp(name,"size_all")||!strcmp(name,"move")) resource=32646;
            else if(!strcmp(name,"hand2")||!strcmp(name,"hand1")||!strcmp(name,"hand")) resource=32649;
            else if(!strcmp(name,"xterm")||!strcmp(name,"ibeam")) resource=32513;
            else if(!strcmp(name,"cross")||!strcmp(name,"crosshair")) resource=32515;
            else if(!strcmp(name,"watch")||!strcmp(name,"wait")) resource=32514;
            else if(!strcmp(name,"sb_h_double_arrow")||!strcmp(name,"size_we")) resource=32644;
            else if(!strcmp(name,"sb_v_double_arrow")||!strcmp(name,"size_ns")) resource=32645;
            else if(!strcmp(name,"size_nw_se")||!strcmp(name,"bottom_right_corner")||!strcmp(name,"top_left_corner")) resource=32642;
            else if(!strcmp(name,"size_ne_sw")||!strcmp(name,"bottom_left_corner")||!strcmp(name,"top_right_corner")) resource=32643;
            else if(!strcmp(name,"no")||!strcmp(name,"not_allowed")) resource=32648;
        } else if(sizegrip) resource=32642;
        HCURSOR cursor=LoadCursorA(NULL,MAKEINTRESOURCEA(resource));
        if(!cursor) cursor=LoadCursorA(NULL,MAKEINTRESOURCEA(32512));
        SetCursor(cursor); return TRUE;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *m=(MINMAXINFO *)lp;
        int low_x=w->min_width;
        int low_y=w->min_height;
        int high_x=w->max_width;
        int high_y=w->max_height;
        DWORD style=(DWORD)GetWindowLongA(hwnd,GWL_STYLE),ex=(DWORD)GetWindowLongA(hwnd,GWL_EXSTYLE);
        RECT lo={0,0,low_x,low_y},hi={0,0,high_x,high_y};
        if(AdjustWindowRectEx(&lo,style,GetMenu(hwnd)!=NULL,ex)&&AdjustWindowRectEx(&hi,style,GetMenu(hwnd)!=NULL,ex)) {
            m->ptMinTrackSize.x=lo.right-lo.left; m->ptMinTrackSize.y=lo.bottom-lo.top;
            m->ptMaxTrackSize.x=hi.right-hi.left; m->ptMaxTrackSize.y=hi.bottom-hi.top;
        }
        return 0;
    }
    case WM_ENTERSIZEMOVE:
        w->live_resize=1;
        /* Never publish an old asynchronous frame during the native modal
           resize loop. The ordinary event pump resumes on exit. */
        if(w->renderer) { tg_async_destroy(w->renderer); w->renderer=NULL; }
        return 0;
    case WM_EXITSIZEMOVE: {
        w->live_resize=0; w->dirty=w->layout_dirty=1;
        RECT cr;
        if(GetClientRect(hwnd,&cr)&&cr.right>0&&cr.bottom>0) {
            tg_window_input(w,TG_CONFIGURE,cr.right,cr.bottom,0);
        }
        if(w->interp) {
            int drained=0;
            while(drained<1024&&tg_interp_dispatch_input(w->interp)) drained++;
        }
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    }
    case WM_SIZE:
        if(wp!=SIZE_MINIMIZED) {
            tg_window_input(w,TG_CONFIGURE,LOWORD(lp),HIWORD(lp),0);
            if(w->live_resize&&w->interp) {
                int drained=0;
                while(drained<1024&&tg_interp_dispatch_input(w->interp)) drained++;
            }
        }
        w->dirty=1; InvalidateRect(hwnd,NULL,FALSE);
        if(w->live_resize&&wp!=SIZE_MINIMIZED) UpdateWindow(hwnd);
        return 0;
    case WM_SETFOCUS: tg_window_input(w,TG_FOCUS_IN,0,0,0); return 0;
    case WM_KILLFOCUS: w->pending_surrogate=0; tg_window_input(w,TG_FOCUS_OUT,0,0,0); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT r;
        if(GetClientRect(hwnd,&r)) {
            LARGE_INTEGER qpc_start,qpc_end,qpc_freq;
            int timed=QueryPerformanceFrequency(&qpc_freq)&&qpc_freq.QuadPart>0&&QueryPerformanceCounter(&qpc_start);
            int presented=0;
#ifdef TKGPU_D3D11_PRESENT
            if(w->gpu_raster_enabled&&!w->gpu_raster_failed&&r.right>0&&r.bottom>0) {
                int gpu=tg_d3d11_raster(w,hwnd,r.right,r.bottom);
                if(gpu>0) { presented=1; goto paint_done; }
                if(gpu<0) w->gpu_raster_failed=1;
                /* A GPU-only frame has no current CPU pixels. Repaint fully
                   before upload or GDI fallback, never present a stale buffer. */
                tg_ui *u=tg_interp_ui(w->interp);
                int rc=tg_window_select(w,r.right,r.bottom)?TG_OK:TG_ERROR;
                if(rc==TG_OK&&(tg_canvas_width(w->canvas)!=r.right||tg_canvas_height(w->canvas)!=r.bottom))
                    rc=tg_canvas_resize(w->canvas,r.right,r.bottom);
                if(rc==TG_OK) rc=tg_ui_paint_parallel(u,w->canvas,0,NULL);
                if(rc!=TG_OK) { tg_d3d11_release(w); w->dirty=1; EndPaint(hwnd,&ps); return 0; }
                w->dirty=w->layout_dirty=0; w->layout_width=r.right; w->layout_height=r.bottom;
            }
            if(!w->live_resize&&w->gpu_present_enabled&&!w->gpu_present_failed) {
                int gpu=tg_d3d11_present(w,hwnd,r.right,r.bottom);
                if(gpu>0) { presented=1; goto paint_done; }
                if(gpu<0) w->gpu_present_failed=1;
            }
#endif
            if(w->live_resize&&r.right>0&&r.bottom>0) {
                tg_ui *u=tg_interp_ui(w->interp);
                int rc=tg_window_select(w,r.right,r.bottom)?TG_OK:TG_ERROR;
                if(rc==TG_OK&&(tg_canvas_width(w->canvas)!=r.right||tg_canvas_height(w->canvas)!=r.bottom))
                    rc=tg_canvas_resize(w->canvas,r.right,r.bottom);
                if(rc==TG_OK) rc=tg_ui_paint_parallel(u,w->canvas,0,NULL);
                if(rc==TG_OK) w->dirty=0;
            }
            BITMAPINFO bi={0}; bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth=tg_canvas_width(w->canvas);
            bi.bmiHeader.biHeight=-tg_canvas_height(w->canvas);
            bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32;
            bi.bmiHeader.biCompression=BI_RGB;
            StretchDIBits(dc,0,0,r.right,r.bottom,
                0,0,bi.bmiHeader.biWidth,-bi.bmiHeader.biHeight,
                tg_canvas_pixels(w->canvas),&bi,DIB_RGB_COLORS,SRCCOPY);
            if(r.right>0&&r.bottom>0) presented=1;
        paint_done:
            if(presented) {
                w->present_frames++;
                if(timed&&QueryPerformanceCounter(&qpc_end)&&qpc_end.QuadPart>=qpc_start.QuadPart) {
                    uint64_t delta=(uint64_t)(qpc_end.QuadPart-qpc_start.QuadPart);
                    uint64_t freq=(uint64_t)qpc_freq.QuadPart;
                    uint64_t us=(delta/freq)*UINT64_C(1000000)+((delta%freq)*UINT64_C(1000000))/freq;
                    w->last_frame_us=us;
                    w->present_time_us+=us;
                }
            }
        }
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_MOUSEACTIVATE:
        if(w->override_redirect) return MA_NOACTIVATE;
        return TG_DEFPROC(hwnd,msg,wp,lp);
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: {
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        uint32_t mods=((wp&MK_SHIFT)?TG_MOD_SHIFT:0)|((wp&MK_CONTROL)?TG_MOD_CONTROL:0)|((GetKeyState(VK_MENU)&0x8000)?TG_MOD_ALT:0);
        uint32_t btn=msg==WM_RBUTTONDOWN?3u:msg==WM_MBUTTONDOWN?2u:1u;
        tg_ui *u=tg_interp_ui(w->interp);
        const tg_widget *g=tg_ui_get(u,tg_ui_hit(u,x,y));
        if(msg==WM_LBUTTONDOWN&&g&&g->kind==TG_SIZEGRIP&&g->state!=TG_STATE_DISABLED&&!IsZoomed(hwnd)) {
            POINT p={x,y}; ClientToScreen(hwnd,&p); ReleaseCapture();
            return TG_DEFPROC(hwnd,WM_NCLBUTTONDOWN,HTBOTTOMRIGHT,MAKELPARAM(p.x,p.y));
        }
        if(!w->override_redirect&&GetFocus()!=hwnd) SetFocus(hwnd);
        SetCapture(hwnd); tg_window_input(w,TG_POINTER_DOWN,x,y,btn|mods); return 0;
    }
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {
        uint32_t mods=((wp&MK_SHIFT)?TG_MOD_SHIFT:0)|((wp&MK_CONTROL)?TG_MOD_CONTROL:0)|((GetKeyState(VK_MENU)&0x8000)?TG_MOD_ALT:0);
        uint32_t btn=msg==WM_RBUTTONUP?3u:msg==WM_MBUTTONUP?2u:1u;
        if(GetCapture()==hwnd) ReleaseCapture();
        tg_window_input(w,TG_POINTER_UP,(short)LOWORD(lp),(short)HIWORD(lp),btn|mods); return 0;
    }
    case WM_MOUSEMOVE: {
#ifndef TKGPU_LEGACY_WINDOWS
        if(!w->mouse_tracking) {
            TRACKMOUSEEVENT track={sizeof track,TME_LEAVE,hwnd,0};
            w->mouse_tracking=TrackMouseEvent(&track)!=0;
        }
#endif
        uint32_t mods=((wp&MK_SHIFT)?TG_MOD_SHIFT:0)|((wp&MK_CONTROL)?TG_MOD_CONTROL:0)|((GetKeyState(VK_MENU)&0x8000)?TG_MOD_ALT:0);
        tg_window_input(w,TG_POINTER_MOVE,(short)LOWORD(lp),(short)HIWORD(lp),mods); return 0;
    }
#ifndef TKGPU_LEGACY_WINDOWS
    case WM_MOUSELEAVE: {
        w->mouse_tracking=0;
        POINT p={0,0}; GetCursorPos(&p); ScreenToClient(hwnd,&p);
        tg_window_input(w,TG_POINTER_EXIT,p.x,p.y,0); return 0;
    }
#endif
    case WM_MOUSEWHEEL: {
        POINT sp={(short)LOWORD(lp),(short)HIWORD(lp)};
        HWND target_hwnd=hwnd;
        tg_window *target_w=w;
        HWND under=WindowFromPoint(sp);
        if(under&&under!=hwnd&&GetWindowThreadProcessId(under,NULL)==GetCurrentThreadId()) {
            tg_window *uw=(tg_window *)TG_GETUSER(under,GWLP_USERDATA);
            if(uw&&uw->alive&&uw->interp==w->interp) {
                target_hwnd=under; target_w=uw;
            }
        }
        POINT p=sp; ScreenToClient(target_hwnd,&p);
        uint32_t mods=((LOWORD(wp)&MK_SHIFT)?TG_MOD_SHIFT:0)|
                      ((LOWORD(wp)&MK_CONTROL)?TG_MOD_CONTROL:0)|
                      ((GetKeyState(VK_MENU)&0x8000)?TG_MOD_ALT:0);
        uint32_t delta=(uint32_t)(uint16_t)(short)HIWORD(wp);
        tg_window_input(target_w,TG_WHEEL,p.x,p.y,delta|mods); return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
        uint32_t key=wp==VK_LEFT?TG_KEY_LEFT:wp==VK_RIGHT?TG_KEY_RIGHT:wp==VK_HOME?TG_KEY_HOME:
            wp==VK_END?TG_KEY_END:wp==VK_INSERT?TG_KEY_INSERT:wp==VK_ESCAPE?TG_KEY_ESCAPE:wp==VK_TAB?TG_KEY_TAB:wp==VK_SPACE?32:wp==VK_BACK?TG_KEY_BACKSPACE:wp==VK_DELETE?TG_KEY_DELETE:wp==VK_RETURN?TG_KEY_RETURN:wp==VK_UP?TG_KEY_ARROW_UP:wp==VK_DOWN?TG_KEY_ARROW_DOWN:
            wp==VK_PRIOR?TG_KEY_PRIOR:wp==VK_NEXT?TG_KEY_NEXT:
            (wp>=VK_F1&&wp<=VK_F12)?(TG_KEY_F1+(wp-VK_F1)):0;
        unsigned mods=((GetKeyState(VK_CONTROL)&0x8000)?TG_MOD_CONTROL:0)|((GetKeyState(VK_SHIFT)&0x8000)?TG_MOD_SHIFT:0)|((GetKeyState(VK_MENU)&0x8000)?TG_MOD_ALT:0);
        if(!key&&wp>='A'&&wp<='Z') key=(uint32_t)(wp-'A'+((mods&TG_MOD_SHIFT)?'A':'a'));
        if(!key&&(mods&(TG_MOD_CONTROL|TG_MOD_ALT))) {
            if(wp>='0'&&wp<='9') key=(uint32_t)wp;
            else if(wp==VK_OEM_2) key='/';
            else if(wp==VK_OEM_PLUS) key='=';
            else if(wp==VK_OEM_MINUS) key='-';
            else if(wp==VK_ADD) key='+';
            else if(wp==VK_SUBTRACT) key='-';
        }
        if(key) tg_window_input(w,msg==WM_KEYUP||msg==WM_SYSKEYUP?TG_KEY_UP:TG_KEY_DOWN,0,0,key|mods);
        if(msg==WM_SYSKEYDOWN||msg==WM_SYSKEYUP||!key) return TG_DEFPROC(hwnd,msg,wp,lp);
        return 0;
    }
    case WM_INPUTLANGCHANGEREQUEST:
        return TG_DEFPROC(hwnd,msg,wp,lp);
    case WM_INPUTLANGCHANGE:
        ActivateKeyboardLayout((HKL)lp,KLF_SETFORPROCESS);
        return TG_DEFPROC(hwnd,msg,wp,lp);
    case WM_CHAR:
#if defined(TKGPU_LEGACY_WINDOWS) && !defined(TKGPU_WINDOWS_XP)
        if(wp>=32&&wp<=126) tg_window_input(w,TG_TEXT_INPUT,0,0,(uint32_t)wp);
#else
        if(wp<=0xffffu) {
            uint32_t scalar;
            if(tg_utf16_feed(&w->pending_surrogate,(uint16_t)wp,&scalar)==1&&scalar>=32&&scalar!=127)
                tg_window_input(w,TG_TEXT_INPUT,0,0,scalar);
        }
#endif
        return 0;
#ifndef TKGPU_LEGACY_WINDOWS
    case WM_UNICHAR:
        if(wp==0xffffu) return TRUE;
        w->pending_surrogate=0;
        if(wp>=32&&wp!=127&&wp<=0x10ffffu&&!(wp>=0xd800u&&wp<=0xdfffu))
            tg_window_input(w,TG_TEXT_INPUT,0,0,(uint32_t)wp);
        return 0;
#endif
    case WM_CLOSE:
        tg_window_request_close(w); return 0;
    case WM_NCDESTROY: {
        tg_window *restore_main=(w&&w->root!=1&&w->interp)?(tg_window *)Tk_MainWindow(w->interp):NULL;
#ifdef TKGPU_D3D11_PRESENT
        tg_d3d11_release(w);
#endif
        if(w->icon_small) DestroyIcon((HICON)w->icon_small);
        if(w->icon_big) DestroyIcon((HICON)w->icon_big);
        w->icon_small=w->icon_big=NULL;
        TG_SETUSER(hwnd,GWLP_USERDATA,0); w->native=NULL; w->alive=0;
        if(restore_main&&restore_main!=w&&restore_main->alive&&restore_main->native&&
           IsWindowVisible((HWND)restore_main->native)&&!GetFocus()) {
            SetFocus((HWND)restore_main->native);
        }
        return TG_DEFPROC(hwnd,msg,wp,lp);
    }
    }
    return TG_DEFPROC(hwnd,msg,wp,lp);
}
int tg_window_open(tg_window *w) {
    HINSTANCE instance=GetModuleHandleA(NULL); TG_WNDCLASS wc={0};
#ifdef TKGPU_D3D11_PRESENT
    char enable[8];
    DWORD n=GetEnvironmentVariableA("TKGPU_GPU_PRESENT",enable,sizeof enable);
    w->gpu_present_enabled=n==1&&enable[0]=='1';
    n=GetEnvironmentVariableA("TKGPU_GPU_RASTER",enable,sizeof enable);
    w->gpu_raster_enabled=n==1&&enable[0]=='1';
    if(w->gpu_raster_enabled) w->gpu_present_enabled=1;
#endif
    wc.lpfnWndProc=procedure; wc.hInstance=instance; wc.lpszClassName=classname;
    wc.hCursor=LoadCursorA(NULL,IDC_ARROW);
    wc.hIcon=framework_icon(GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON));
    if(!wc.hIcon) wc.hIcon=LoadIconA(instance,MAKEINTRESOURCEA(1));
    if(!TG_REGISTER(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 0;
    HWND hwnd=TG_CREATE(classname,TG_TITLE,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,660,520,NULL,NULL,instance,w);
    if(!hwnd) return 0;
    if(wc.hIcon) {
        SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)wc.hIcon);
        HICON small=framework_icon(GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON));
        SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)(small?small:wc.hIcon));
    }
    {
        char icon_path[32768];
        DWORD n=GetEnvironmentVariableA("KERBOT_TKGPU_WINDOW_ICON",icon_path,sizeof icon_path);
        if(n>0&&n<sizeof icon_path) tg_window_icon_bitmap(w,icon_path);
    }
    w->withdrawn=1; w->map_pending=1; w->dirty=1; return 1;
}
int tg_window_icon_bitmap(tg_window *w,const char *path) {
    WCHAR wide[32768];
    if(!w||!w->native||!path||!*path||
       !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,32768)) return 0;
    HICON small=(HICON)LoadImageW(NULL,wide,IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_LOADFROMFILE);
    HICON big=(HICON)LoadImageW(NULL,wide,IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_LOADFROMFILE);
    if(!small||!big) { if(small) DestroyIcon(small); if(big) DestroyIcon(big); return 0; }
    SendMessageW((HWND)w->native,WM_SETICON,ICON_SMALL,(LPARAM)small);
    SendMessageW((HWND)w->native,WM_SETICON,ICON_BIG,(LPARAM)big);
    if(w->icon_small) DestroyIcon((HICON)w->icon_small);
    if(w->icon_big) DestroyIcon((HICON)w->icon_big);
    w->icon_small=small; w->icon_big=big;
    return 1;
}
void tg_window_close_native(tg_window *w) { if(w->native) DestroyWindow((HWND)w->native); }
void tg_window_free_native(tg_window *w) { tg_window_close_native(w); }
int tg_window_poll_native(tg_window *w) {
    HWND hwnd=(HWND)w->native; if(!hwnd) return 0;
    char probe[8]; DWORD probe_size=GetEnvironmentVariableA("TKGPU_PROBE",probe,sizeof probe);
    if(w->map_pending&&!(probe_size==1&&probe[0]=='1')) {
        tg_window_fit_override(w);
        RECT init_r;
        if(GetClientRect(hwnd,&init_r)&&init_r.right>0&&init_r.bottom>0) {
            (void)tg_window_refresh(w,init_r.right,init_r.bottom);
        }
        w->map_pending=0; w->withdrawn=0; ShowWindow(hwnd,w->override_redirect?SW_SHOWNOACTIVATE:SW_SHOWNORMAL);
        (void)tg_ui_post(tg_interp_ui(w->interp),(tg_event){TG_MAP,w->root,0,0,0});
    }
    /* Pump a batch of pending Win32 messages first so keyboard/mouse bursts
       are ingested promptly instead of triggering a full repaint per message. */
    MSG msg;
    int messages=0;
    while(messages<32&&TG_PEEK(&msg,NULL,0,0,PM_REMOVE)) {
        TranslateMessage(&msg); TG_DISPATCH(&msg);
        messages++;
    }
    int painted=0;
    RECT r;
    if(!messages&&IsWindowVisible(hwnd)&&!IsIconic(hwnd)&&GetClientRect(hwnd,&r)&&r.right>0&&r.bottom>0) {
#ifdef TKGPU_D3D11_PRESENT
        if(w->gpu_raster_enabled&&!w->gpu_raster_failed) {
            if(w->dirty||w->layout_width!=r.right||w->layout_height!=r.bottom) {
                InvalidateRect(hwnd,NULL,FALSE);
                painted=UpdateWindow(hwnd)!=0;
            }
        } else
#endif
        if(tg_window_refresh(w,r.right,r.bottom)) {
            InvalidateRect(hwnd,NULL,FALSE);
            painted=UpdateWindow(hwnd)!=0;
        }
    }
    if(messages) return 1;
    if(!IsWindowVisible(hwnd)&&GetClientRect(hwnd,&r)&&r.right>0&&r.bottom>0)
        tg_window_select(w,r.right,r.bottom);
    return painted;
}

int tg_window_set_title(tg_window *w,const char *title) {
    HWND hwnd=(HWND)w->native;
    if(!hwnd) return 0;
#if defined(TKGPU_LEGACY_WINDOWS) && !defined(TKGPU_WINDOWS_XP)
    WCHAR wide[512]; char ansi[1024];
    if(!MultiByteToWideChar(CP_UTF8,0,title,-1,wide,512)||
       !WideCharToMultiByte(CP_ACP,0,wide,-1,ansi,sizeof ansi,NULL,NULL)) return 0;
    return SetWindowTextA(hwnd,ansi)!=0;
#else
    WCHAR wide[512];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,title,-1,wide,512)) return 0;
    return SetWindowTextW(hwnd,wide)!=0;
#endif
}
int tg_window_set_size(tg_window *w,int width,int height) {
    HWND hwnd=(HWND)w->native; if(!hwnd) return 0;
    RECT r={0,0,width,height};
    DWORD style=(DWORD)GetWindowLongA(hwnd,GWL_STYLE),ex=(DWORD)GetWindowLongA(hwnd,GWL_EXSTYLE);
    if(!AdjustWindowRectEx(&r,style,GetMenu(hwnd)!=NULL,ex)) return 0;
    return SetWindowPos(hwnd,NULL,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=0;
}
static void tg_reapply_transient_owner(tg_window *w) {
    if(!w||!w->native||!w->transient_owner||!w->transient_owner->native) return;
    HWND hwnd=(HWND)w->native;
    HWND owner=(HWND)w->transient_owner->native;
    if(!IsWindow(owner)) return;
#if defined(TKGPU_LEGACY_WINDOWS) && !defined(TKGPU_WINDOWS_XP)
    SetWindowLongA(hwnd,GWL_HWNDPARENT,(LONG)(LONG_PTR)owner);
#else
    SetWindowLongPtrW(hwnd,GWLP_HWNDPARENT,(LONG_PTR)owner);
#endif
    SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}
int tg_window_set_visible(tg_window *w,int visible) {
    w->map_pending=0;
    HWND hwnd=(HWND)w->native; if(!hwnd) return 0;
    if(visible) tg_reapply_transient_owner(w);
    char probe[8]; DWORD n=GetEnvironmentVariableA("TKGPU_PROBE",probe,sizeof probe);
    if(!(visible&&n==1&&probe[0]=='1')) ShowWindow(hwnd,visible?(w->override_redirect?SW_SHOWNOACTIVATE:SW_RESTORE):SW_HIDE);
    if(visible&&w->transient_owner&&w->transient_owner->native) {
        SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    return 1;
}

int tg_window_origin(tg_window *w,int *x,int *y) {
    POINT p={0,0}; if(!w||!w->native||!ClientToScreen((HWND)w->native,&p)) return 0;
    *x=(int)p.x; *y=(int)p.y; return 1;
}
int tg_window_screen(tg_window *w,int *width,int *height) {
    if(!w->native) return 0;
    *width=GetSystemMetrics(SM_CXSCREEN); *height=GetSystemMetrics(SM_CYSCREEN);
    return *width>0&&*height>0;
}
int tg_window_set_geometry(tg_window *w,int width,int height,int pos,int x,int y,int nx,int ny) {
    if(!pos) return tg_window_set_size(w,width,height);
    HWND hwnd=(HWND)w->native; int sw,sh;
    if(!hwnd||!tg_window_screen(w,&sw,&sh)) return 0;
    RECT r={0,0,width,height};
    DWORD style=(DWORD)GetWindowLongA(hwnd,GWL_STYLE),ex=(DWORD)GetWindowLongA(hwnd,GWL_EXSTYLE);
    if(!AdjustWindowRectEx(&r,style,GetMenu(hwnd)!=NULL,ex)) return 0;
    int ow=r.right-r.left,oh=r.bottom-r.top;
    return SetWindowPos(hwnd,NULL,nx?sw-ow-x:x,ny?sh-oh-y:y,ow,oh,SWP_NOZORDER|SWP_NOACTIVATE)!=0;
}
int tg_window_limits(tg_window *w) {
    HWND hwnd=(HWND)w->native; if(!hwnd) return 0;
    LONG old=GetWindowLongA(hwnd,GWL_STYLE),style=old;
    if(w->override_redirect) return 1;
    if(w->resize_x&&w->resize_y) style|=WS_MAXIMIZEBOX; else style&=~WS_MAXIMIZEBOX;
    if(w->resize_x||w->resize_y) style|=WS_THICKFRAME; else style&=~WS_THICKFRAME;
    if(style!=old) {
        RECT client; GetClientRect(hwnd,&client);
        SetLastError(0); SetWindowLongA(hwnd,GWL_STYLE,style); if(GetLastError()) return 0;
        if(!SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)) return 0;
        if(!tg_window_set_size(w,client.right,client.bottom)) return 0;
    }
    return 1;
}

/* Use the real TKGPU HWND; no Tk window wrapper or Python motion callback. */
int tg_window_custom_caption(tg_window *w,int enabled) {
    HWND hwnd=w?(HWND)w->native:NULL;
    if(!hwnd) return 0;
    if(w->override_redirect) return 1;
    LONG old=GetWindowLongA(hwnd,GWL_STYLE);
    LONG style;
    if(w->root!=1) {
        if(enabled) {
            style=(old&~(WS_OVERLAPPEDWINDOW|WS_POPUP))|WS_POPUP|WS_CLIPCHILDREN;
            if(w->resize_x||w->resize_y) style|=WS_THICKFRAME;
        } else {
            style=(old&~(WS_OVERLAPPEDWINDOW|WS_POPUP))|WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN;
            if(!w->resize_x||!w->resize_y) style&=~WS_MAXIMIZEBOX;
            if(!w->resize_x&&!w->resize_y) style&=~WS_THICKFRAME;
        }
    } else {
        style=enabled?(old&~WS_CAPTION):(old|WS_CAPTION);
    }
    if(style==old) return 1;
    SetLastError(0);
    if(!SetWindowLongA(hwnd,GWL_STYLE,style)&&GetLastError()!=0) return 0;
    if(!SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)) {
        SetWindowLongA(hwnd,GWL_STYLE,old);
        SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        return 0;
    }
    tg_reapply_transient_owner(w);
    w->dirty=w->layout_dirty=1; return 1;
}
int tg_window_override(tg_window *w,int enabled) {
    HWND hwnd=w?(HWND)w->native:NULL;
    if(!hwnd) return 0;
    if(w->override_redirect==enabled) return 1;
    RECT client;
    POINT origin={0,0};
    if(!GetClientRect(hwnd,&client)||!ClientToScreen(hwnd,&origin)) return 0;
    LONG old=GetWindowLongA(hwnd,GWL_STYLE);
    LONG style=old&~(WS_OVERLAPPEDWINDOW|WS_POPUP);
    if(enabled) style|=WS_POPUP|WS_CLIPCHILDREN;
    else {
        style|=WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN;
        if(w->caption.enabled) style&=~WS_CAPTION;
        if(!w->resize_x||!w->resize_y) style&=~WS_MAXIMIZEBOX;
        if(!w->resize_x&&!w->resize_y) style&=~WS_THICKFRAME;
    }
    LONG old_ex=GetWindowLongA(hwnd,GWL_EXSTYLE);
    LONG ex_style=enabled?(old_ex|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW):(old_ex&~(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW));
    RECT bounds={0,0,client.right,client.bottom};
    if(!AdjustWindowRectEx(&bounds,(DWORD)style,GetMenu(hwnd)!=NULL,(DWORD)ex_style)) return 0;
    SetLastError(0);
    if(!SetWindowLongA(hwnd,GWL_STYLE,style)&&GetLastError()) return 0;
    SetLastError(0);
    (void)SetWindowLongA(hwnd,GWL_EXSTYLE,ex_style);
    if(!SetWindowPos(hwnd,NULL,origin.x+bounds.left,origin.y+bounds.top,
                     bounds.right-bounds.left,bounds.bottom-bounds.top,
                     SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)) {
        SetWindowLongA(hwnd,GWL_STYLE,old);
        SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        return 0;
    }
    w->override_redirect=enabled; w->dirty=w->layout_dirty=1;
    return 1;
}
int tg_window_caption_state(tg_window *w) {
    HWND hwnd=w?(HWND)w->native:NULL;
    return hwnd?(IsIconic(hwnd)?2:IsZoomed(hwnd)?1:0):0;
}
void *tg_window_child_host(tg_window *w,tg_id id) {
    if(!w||!w->native||!w->interp) return NULL;
    tg_ui *u=tg_interp_ui(w->interp);
    tg_widget *g=tg_lookup(u,id);
    if(!g) return NULL;
    if(g->native_host&&IsWindow((HWND)g->native_host)) return g->native_host;
    int x=g->rect.x,y=g->rect.y,ww=g->rect.w>0?g->rect.w:1,hh=g->rect.h>0?g->rect.h:1;
    DWORD vis=(g->rect.w>0&&g->rect.h>0)?WS_VISIBLE:0;
    HWND child=CreateWindowExA(0,"STATIC","",WS_CHILD|vis|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
        x,y,ww,hh,(HWND)w->native,NULL,GetModuleHandleA(NULL),NULL);
    g->native_host=child;
    return child;
}
void tg_window_sync_child_hosts(tg_window *w) {
    if(!w||!w->native||!w->interp) return;
    tg_ui *u=tg_interp_ui(w->interp);
    if(!u) return;
    for(size_t k=1;k<u->count;k++) {
        tg_widget *g=&u->items[k];
        if(!g->native_host) continue;
        if(!IsWindow((HWND)g->native_host)) { g->native_host=NULL; continue; }
        /* Check if widget belongs to this toplevel and has non-empty visible rect. */
        const tg_widget *p=g;
        int visible=g->rect.w>0&&g->rect.h>0;
        while(p&&p->id!=w->root&&p->kind!=TG_TOPLEVEL) {
            if(p->rect.w<=0||p->rect.h<=0) visible=0;
            p=tg_ui_get(u,p->parent);
        }
        if(!p||p->id!=w->root) continue;
        if(visible) {
            SetWindowPos((HWND)g->native_host,NULL,g->rect.x,g->rect.y,g->rect.w,g->rect.h,
                SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        } else {
            ShowWindow((HWND)g->native_host,SW_HIDE);
        }
    }
}
int tg_window_caption_action(tg_window *w,int action) {
    HWND hwnd=w?(HWND)w->native:NULL;
    if(!hwnd||(action!=1&&action!=2)) return 0;
    ShowWindow(hwnd,action==2?SW_MINIMIZE:IsZoomed(hwnd)?SW_RESTORE:SW_MAXIMIZE);
    return 1;
}
