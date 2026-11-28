#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "TKGPUUi.H"
#include <stdio.h>

static tg_interp *interp;
static tg_canvas *canvas;
static tg_id button;
static unsigned clicks;
static void redraw(HWND hwnd,int w,int h) {
    if(!interp||w<=0||h<=0) return;
    if(!canvas) canvas=tg_canvas_create(w,h);
    else if((tg_canvas_width(canvas)!=w||tg_canvas_height(canvas)!=h)&&tg_canvas_resize(canvas,w,h)!=TG_OK) return;
    if(!canvas) return;
    tg_ui *ui=tg_interp_ui(interp);
    if(tg_ui_arrange(ui,w,h)!=TG_OK||tg_ui_paint(ui,canvas)!=TG_OK) return;
    InvalidateRect(hwnd,NULL,FALSE);
}
static LRESULT CALLBACK window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_SIZE: redraw(hwnd,LOWORD(lp),HIWORD(lp)); return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        if(interp) {
            tg_ui *ui=tg_interp_ui(interp);
            int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
            tg_event e={msg==WM_LBUTTONDOWN?TG_POINTER_DOWN:TG_POINTER_UP,tg_ui_hit(ui,x,y),x,y,0},out;
            if(tg_ui_post(ui,e)==TG_OK) while(tg_ui_poll(ui,&out)) {
                if(out.kind==TG_POINTER_UP&&out.target==button) {
                    char label[64]; clicks++;
                    snprintf(label,sizeof label,"Click: %u",clicks);
                    tg_ui_configure(ui,button,label,NULL);
                    RECT area;
                    if(GetClientRect(hwnd,&area)) redraw(hwnd,area.right,area.bottom);
                }
            }
        }
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps);
        if(canvas&&interp) {
            BITMAPINFO bi={0};
            bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth=tg_canvas_width(canvas);
            bi.bmiHeader.biHeight=-tg_canvas_height(canvas);
            bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
            StretchDIBits(dc,0,0,tg_canvas_width(canvas),tg_canvas_height(canvas),
                0,0,tg_canvas_width(canvas),tg_canvas_height(canvas),
                tg_canvas_pixels(canvas),&bi,DIB_RGB_COLORS,SRCCOPY);
        } else FillRect(dc,&ps.rcPaint,(HBRUSH)(COLOR_WINDOW+1));
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show) {
    (void)previous; (void)args;
    interp=tg_create(); if(!interp) return 1;
    if(tg_eval(interp,"frame .bar; pack .bar; button .bar.ok -text Click; place .bar.ok -x 8 -y 2 -width 160 -height 28; canvas .body; place .body -x 16 -y 50 -width 600 -height 350")!=TG_OK) {
        tg_destroy(interp); return 1;
    }
    button=tg_ui_find(tg_interp_ui(interp),".bar.ok");
    WNDCLASSA wc={0}; wc.lpfnWndProc=window_proc; wc.hInstance=instance;
    wc.hIcon=LoadIconA(instance,MAKEINTRESOURCEA(1));
    wc.hCursor=LoadCursorA(NULL,IDC_ARROW); wc.lpszClassName="TKGPU.NativeDemo";
    if(!RegisterClassA(&wc)) { tg_destroy(interp); return 1; }
    HWND hwnd=CreateWindowA(wc.lpszClassName,"TKGPU - independent runtime",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,720,480,NULL,NULL,instance,NULL);
    if(!hwnd) { tg_canvas_destroy(canvas); tg_destroy(interp); return 1; }
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG msg; BOOL rc;
    while((rc=GetMessageA(&msg,NULL,0,0))>0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    tg_canvas_destroy(canvas); tg_destroy(interp);
    return rc<0?1:(int)msg.wParam;
}
