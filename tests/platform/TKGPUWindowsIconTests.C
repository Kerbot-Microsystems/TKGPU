/* Verify framework defaults from an EXE without its own icon resource. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "TKGPUCompat.H"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static HWND window_id(Tcl_Interp *i,const char *path) {
    char script[128];
    snprintf(script,sizeof script,"winfo id %s",path);
    if(Tcl_Eval(i,script)) return NULL;
    return (HWND)(UINT_PTR)strtoull(Tcl_GetStringResult(i),NULL,0);
}
int main(int argc,char **argv) {
    CHECK(argc==4);
    SetEnvironmentVariableA("TKGPU_PROBE","1");
    SetEnvironmentVariableA("KERBOT_TKGPU_WINDOW_ICON",NULL);
    HMODULE library=LoadLibraryA(argv[1]); CHECK(library);
    HICON small=(HICON)LoadImageA(library,MAKEINTRESOURCEA(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);
    HICON big=(HICON)LoadImageA(library,MAKEINTRESOURCEA(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);
    CHECK(small&&big);
    Tcl_Interp *i=Tcl_CreateInterp(); CHECK(i);
    CHECK(Tcl_Init(i)==0&&Tk_Init(i)==0);
    CHECK(Tcl_Eval(i,"toplevel .child; update")==0);
    HWND main=window_id(i,"."),child=window_id(i,".child"); CHECK(main&&child);
    CHECK((HICON)SendMessageA(main,WM_GETICON,ICON_SMALL,0)==small);
    CHECK((HICON)SendMessageA(main,WM_GETICON,ICON_BIG,0)==big);
    CHECK((HICON)SendMessageA(child,WM_GETICON,ICON_SMALL,0)==small);
    CHECK((HICON)SendMessageA(child,WM_GETICON,ICON_BIG,0)==big);
    char script[4096]; snprintf(script,sizeof script,"wm iconbitmap . {%s}",argv[2]);
    CHECK(Tcl_Eval(i,script)==0);
    CHECK(SendMessageA(main,WM_GETICON,ICON_BIG,0)!=0);
    CHECK((HICON)SendMessageA(main,WM_GETICON,ICON_BIG,0)!=big);
    HMODULE app=LoadLibraryExA(argv[3],NULL,LOAD_LIBRARY_AS_DATAFILE); CHECK(app);
    CHECK(FindResourceA(app,MAKEINTRESOURCEA(1),RT_GROUP_ICON)!=NULL);
    FreeLibrary(app);
    CHECK(Tcl_Eval(i,"destroy .; update")==0);
    Tcl_DeleteInterp(i); FreeLibrary(library);
    puts("PASS: TKGPU window defaults, custom override and embedded EXE icon");
    return 0;
}
