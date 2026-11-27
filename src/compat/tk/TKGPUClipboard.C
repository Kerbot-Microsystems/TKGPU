#include "TKGPUWindow.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#include "../../widgets/controls/TKGPUControls.H"
#include "TKGPUUtf8.H"
#include <string.h>
#if defined(_WIN32)||defined(TKGPU_COCOA)
static size_t byte_at(const char *text,size_t index) {
    size_t at=0,n=strlen(text);
    while(at<n&&index--) { uint32_t scalar; size_t used; if(tg_utf8_decode((const unsigned char *)text+at,n-at,&scalar,&used)!=TG_OK) return n; at+=used; }
    return at;
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef TKGPU_LEGACY_WINDOWS
static int put(HWND hwnd,const char *text) {
    WCHAR wide[MAX_WORD];
    int count=MultiByteToWideChar(CP_UTF8,0,text,-1,wide,MAX_WORD);
    if(count<1) return 0;
    int bytes=WideCharToMultiByte(CP_ACP,0,wide,-1,NULL,0,NULL,NULL);
    if(bytes<1) return 0;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(size_t)bytes); if(!memory) return 0;
    char *ansi=GlobalLock(memory); if(!ansi) { GlobalFree(memory); return 0; }
    int converted=WideCharToMultiByte(CP_ACP,0,wide,-1,ansi,bytes,NULL,NULL);
    GlobalUnlock(memory);
    if(!converted||!OpenClipboard(hwnd)) { GlobalFree(memory); return 0; }
    int ok=EmptyClipboard()&&SetClipboardData(CF_TEXT,memory)!=NULL;
    CloseClipboard(); if(!ok) GlobalFree(memory); return ok;
}
static int get(HWND hwnd,char *text,size_t capacity) {
    if(!capacity||capacity>MAX_WORD||!OpenClipboard(hwnd)) return 0;
    HGLOBAL memory=GetClipboardData(CF_TEXT);
    if(!memory) { CloseClipboard(); return 0; }
    const char *ansi=GlobalLock(memory);
    if(!ansi) { CloseClipboard(); return 0; }
    size_t size=GlobalSize(memory),length=0;
    while(length<size&&length<MAX_WORD&&ansi[length]) length++;
    WCHAR wide[MAX_WORD];
    int units=length<size&&length<MAX_WORD?MultiByteToWideChar(CP_ACP,0,ansi,(int)length,wide,MAX_WORD):0;
    int n=units>0?WideCharToMultiByte(CP_UTF8,0,wide,units,text,(int)capacity-1,NULL,NULL):0;
    if(n>0) text[n]=0;
    GlobalUnlock(memory); CloseClipboard(); return n>0;
}
#else
static int put(HWND hwnd,const char *text) {
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);
    if(count<1) return 0;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(size_t)count*sizeof(WCHAR)); if(!memory) return 0;
    WCHAR *wide=GlobalLock(memory); if(!wide) { GlobalFree(memory); return 0; }
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,count); GlobalUnlock(memory);
    if(!OpenClipboard(hwnd)) { GlobalFree(memory); return 0; }
    int ok=EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,memory)!=NULL;
    CloseClipboard(); if(!ok) GlobalFree(memory); return ok;
}
static int get(HWND hwnd,char *text,size_t capacity) {
    if(!OpenClipboard(hwnd)) return 0;
    HGLOBAL memory=GetClipboardData(CF_UNICODETEXT);
    if(!memory) { CloseClipboard(); return 0; }
    const WCHAR *wide=GlobalLock(memory); if(!wide) { CloseClipboard(); return 0; }
    size_t count=GlobalSize(memory)/sizeof(WCHAR),length=0;
    while(length<count&&wide[length]) length++;
    int n=length<count&&length<MAX_WORD?WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,(int)length,text,(int)capacity-1,NULL,NULL):0;
    if(n>=0&&(size_t)n<capacity) text[n]=0;
    GlobalUnlock(memory); CloseClipboard(); return n>0;
}
#endif
#else
static int put(void *unused,const char *text) { (void)unused; return tg_cocoa_clipboard_put(text); }
static int get(void *unused,char *text,size_t cap) { (void)unused; return tg_cocoa_clipboard_get(text,cap); }
#endif
#endif
int tg_clipboard_command(Tcl_Interp *i,size_t n,char **a) {
    if(n!=4||strcmp(a[0],"event")||strcmp(a[1],"generate")||(strcmp(a[3],"<<Copy>>")&&strcmp(a[3],"<<Cut>>")&&strcmp(a[3],"<<Paste>>"))) return -1;
#if defined(_WIN32)||defined(TKGPU_COCOA)
    tg_widget *w=tg_lookup(tg_interp_ui(i),tg_ui_find(tg_interp_ui(i),a[2]));
    tg_window *window=tg_window_for_widget(i,a[2]);
    if(!w||w->kind!=TG_ENTRY||!window) return tg_set_result(i,"unsupported clipboard target",TG_ERROR);
    char buffer[MAX_WORD]; int paste=!strcmp(a[3],"<<Paste>>"),cut=!strcmp(a[3],"<<Cut>>");
    if(w->state!=TG_STATE_NORMAL&&(paste||cut)) return tg_set_result(i,"",TG_OK);
    size_t first=byte_at(w->text,w->sel_first),last=byte_at(w->text,w->sel_last);
    if(!paste) {
        if(w->show[0]||last<=first) return tg_set_result(i,"",TG_OK);
        memcpy(buffer,w->text+first,last-first); buffer[last-first]=0;
        if(!put(window->native,buffer)) return tg_set_result(i,"clipboard is unavailable",TG_ERROR);
    } else {
        if(!get(window->native,buffer,sizeof buffer)) return tg_set_result(i,"",TG_OK);
        if(strlen(w->text)-(last-first)+strlen(buffer)>=sizeof w->text) return tg_set_result(i,"pasted text exceeds entry capacity",TG_ERROR);
    }
    if(cut||paste) {
        char result[MAX_WORD];
        if(last>first) {
            char *args[]={a[2],"delete","sel.first","sel.last"};
            if(tg_entry_command(w,4,args,result,sizeof result)!=TG_OK) return TG_ERROR;
        }
        if(paste) { char *args[]={a[2],"insert","insert",buffer}; if(tg_entry_command(w,4,args,result,sizeof result)!=TG_OK) return TG_ERROR; }
        if(w->textvariable[0]&&tg_var_store(i,w->textvariable,w->text)!=TG_OK) return TG_ERROR;
        if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
    }
    return tg_set_result(i,"",TG_OK);
#else
    return tg_set_result(i,"native clipboard is unavailable on this platform",TG_ERROR);
#endif
}
