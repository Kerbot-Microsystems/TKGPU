#include "TKGPUWindow.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../core/TKGPUList.H"
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wchar.h>
#endif
static int err(Tcl_Interp *i,const char *s) { return tg_set_result(i,s,TG_ERROR); }
#ifdef _WIN32
static int button(const char *s,unsigned *flag) {
    if(!strcmp(s,"ok")) *flag=MB_OK;
    else if(!strcmp(s,"okcancel")) *flag=MB_OKCANCEL;
    else if(!strcmp(s,"yesno")) *flag=MB_YESNO;
    else if(!strcmp(s,"yesnocancel")) *flag=MB_YESNOCANCEL;
    else if(!strcmp(s,"retrycancel")) *flag=MB_RETRYCANCEL;
    else if(!strcmp(s,"abortretryignore")) *flag=MB_ABORTRETRYIGNORE;
    else return 0;
    return 1;
}
static int icon_flag(const char *s,unsigned *flag) {
    if(!strcmp(s,"info")) *flag=MB_ICONINFORMATION;
    else if(!strcmp(s,"warning")) *flag=MB_ICONWARNING;
    else if(!strcmp(s,"error")) *flag=MB_ICONERROR;
    else if(!strcmp(s,"question")) *flag=MB_ICONQUESTION;
    else return 0;
    return 1;
}
static int default_button(unsigned type,const char *s,unsigned *flag) {
    const char *names[3]={NULL,NULL,NULL};
    switch(type) {
    case MB_OK: names[0]="ok"; break;
    case MB_OKCANCEL: names[0]="ok"; names[1]="cancel"; break;
    case MB_YESNO: names[0]="yes"; names[1]="no"; break;
    case MB_YESNOCANCEL: names[0]="yes"; names[1]="no"; names[2]="cancel"; break;
    case MB_RETRYCANCEL: names[0]="retry"; names[1]="cancel"; break;
    case MB_ABORTRETRYIGNORE: names[0]="abort"; names[1]="retry"; names[2]="ignore"; break;
    }
    for(unsigned k=0;k<3;k++) if(names[k]&&!strcmp(names[k],s)) {
        *flag=k==0?MB_DEFBUTTON1:k==1?MB_DEFBUTTON2:MB_DEFBUTTON3; return 1;
    }
    return 0;
}
#ifndef TKGPU_LEGACY_WINDOWS
static int wide(const char *s,WCHAR *out,int cap) {
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out,cap)>0;
}
static int filter_append(WCHAR *out,size_t cap,size_t *used,const char *s) {
    if(*used>=cap-1||!wide(s,out+*used,(int)(cap-*used-1))) return 0;
    *used+=wcslen(out+*used)+1; return 1;
}
static int filters(const char *list,WCHAR *out,size_t cap) {
    size_t used=0;
    if(!*list) list="{All files *}";
    char entry[4096],label[1024],patterns[2048],item[512],joined[2048];
    int rc;
    while((rc=tg_list_next(&list,entry,sizeof entry))>0) {
        const char *p=entry;
        if(tg_list_next(&p,label,sizeof label)!=1||tg_list_next(&p,patterns,sizeof patterns)!=1) return 0;
        /* Optional 3rd macType element in Tk filetypes tuple is ignored on Windows. */
        while(tg_list_next(&p,item,sizeof item)==1) {}
        const char *q=patterns; size_t len=0; joined[0]=0;
        int pr;
        while((pr=tg_list_next(&q,item,sizeof item))>0) {
            const char *prefix=item[0]=='.'?"*":"";
            const char *pattern=!strcmp(item,"*")?"*.*":item;
            int n=snprintf(joined+len,sizeof joined-len,"%s%s%s",len?";":"",prefix,pattern);
            if(n<0||(size_t)n>=sizeof joined-len) return 0;
            len+=(size_t)n;
        }
        if(pr<0||!len||!filter_append(out,cap,&used,label)||!filter_append(out,cap,&used,joined)) return 0;
    }
    if(rc<0||!used||used>=cap) return 0;
    out[used]=0; return 1;
}
static int CALLBACK browse_dir_cb(HWND hwnd,UINT msg,LPARAM lp,LPARAM data) {
    (void)lp;
    if(msg==BFFM_INITIALIZED&&data) {
        SendMessageW(hwnd,BFFM_SETSELECTIONW,TRUE,data);
    }
    return 0;
}
static int dir_dialog(Tcl_Interp *i,HWND owner,size_t n,char **a) {
    const char *title="",*initialdir="";
    for(size_t k=1;k<n;k+=2) {
        if(!strcmp(a[k],"-title")) title=a[k+1];
        else if(!strcmp(a[k],"-initialdir")) initialdir=a[k+1];
        else if(!strcmp(a[k],"-parent")||!strcmp(a[k],"-mustexist")) continue;
        else return err(i,"unsupported directory dialog option");
    }
    WCHAR wt[512],init[4096];
    if(!wide(title,wt,512)||!wide(initialdir,init,4096)) return err(i,"invalid directory dialog options");
    for(WCHAR *p=init;*p;p++) if(*p==L'/') *p=L'\\';
    BROWSEINFOW bi={0};
    bi.hwndOwner=owner;
    bi.lpszTitle=*title?wt:NULL;
    bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE|BIF_USENEWUI;
    if(*initialdir) { bi.lpfn=browse_dir_cb; bi.lParam=(LPARAM)init; }
    PIDLIST_ABSOLUTE pidl=SHBrowseForFolderW(&bi);
    if(!pidl) return tg_set_result(i,"",TG_OK);
    WCHAR chosen[4096];
    BOOL ok=SHGetPathFromIDListW(pidl,chosen);
    CoTaskMemFree(pidl);
    if(!ok) return tg_set_result(i,"",TG_OK);
    char result[MAX_WORD];
    if(!WideCharToMultiByte(CP_UTF8,0,chosen,-1,result,sizeof result,NULL,NULL))
        return err(i,"selected directory exceeds preview limit");
    for(char *p=result;*p;p++) if(*p=='\\') *p='/';
    return tg_set_result(i,result,TG_OK);
}
static int color_dialog(Tcl_Interp *i,HWND owner,size_t n,char **a) {
    static COLORREF custom[16]={0};
    COLORREF init_color=RGB(255,255,255);
    for(size_t k=1;k<n;k+=2) {
        if(!strcmp(a[k],"-initialcolor")&&a[k+1][0]=='#'&&strlen(a[k+1])==7) {
            unsigned r=0,g=0,b=0;
            if(sscanf(a[k+1]+1,"%2x%2x%2x",&r,&g,&b)==3) init_color=RGB(r,g,b);
        } else if(!strcmp(a[k],"-title")||!strcmp(a[k],"-parent")) continue;
    }
    CHOOSECOLORW cc={0};
    cc.lStructSize=sizeof cc; cc.hwndOwner=owner; cc.lpCustColors=custom;
    cc.rgbResult=init_color; cc.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(!ChooseColorW(&cc)) return tg_set_result(i,"",TG_OK);
    char hex[16];
    snprintf(hex,sizeof hex,"#%02x%02x%02x",(unsigned)GetRValue(cc.rgbResult),(unsigned)GetGValue(cc.rgbResult),(unsigned)GetBValue(cc.rgbResult));
    return tg_set_result(i,hex,TG_OK);
}
static int file_dialog(Tcl_Interp *i,HWND owner,int save,size_t n,char **a) {
    const char *title="",*initialdir="",*initialfile="",*ext="",*types="";
    int multiple=0;
    for(size_t k=1;k<n;k+=2) {
        if(!strcmp(a[k],"-title")) title=a[k+1];
        else if(!strcmp(a[k],"-initialdir")) initialdir=a[k+1];
        else if(!strcmp(a[k],"-initialfile")) initialfile=a[k+1];
        else if(!strcmp(a[k],"-defaultextension")) ext=a[k+1];
        else if(!strcmp(a[k],"-filetypes")) types=a[k+1];
        else if(!strcmp(a[k],"-parent")||!strcmp(a[k],"-confirmoverwrite")||!strcmp(a[k],"-typevariable")) continue;
        else if(!save&&!strcmp(a[k],"-multiple")) {
            multiple=(!strcmp(a[k+1],"1")||!strcmp(a[k+1],"true")||!strcmp(a[k+1],"yes"));
        }
        else return err(i,"unsupported file dialog option");
    }
    WCHAR wt[512],dir[4096],file[8192],extension[128],filter[4096];
    if(*ext=='.') ext++;
    if(!wide(title,wt,512)||!wide(initialdir,dir,4096)||!wide(initialfile,file,8192)||
       !wide(ext,extension,128)||!filters(types,filter,4096)) return err(i,"invalid file dialog options");
    for(WCHAR *p=dir;*p;p++) if(*p==L'/') *p=L'\\';
    OPENFILENAMEW ofn={0};
    ofn.lStructSize=sizeof ofn; ofn.hwndOwner=owner; ofn.lpstrFile=file; ofn.nMaxFile=8192;
    ofn.lpstrFilter=filter; ofn.nFilterIndex=1; ofn.lpstrTitle=*title?wt:NULL;
    ofn.lpstrInitialDir=*initialdir?dir:NULL; ofn.lpstrDefExt=*ext?extension:NULL;
    ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|
        (save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST)|(multiple?OFN_ALLOWMULTISELECT:0);
    BOOL ok=save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn);
    if(!ok) return CommDlgExtendedError()?err(i,"native file dialog failed"):tg_set_result(i,"",TG_OK);
    if(multiple) {
        WCHAR *p=file;
        size_t dir_len=wcslen(p);
        WCHAR *next=p+dir_len+1;
        char list[MAX_WORD]="";
        if(!*next) {
            char single[MAX_WORD];
            if(!WideCharToMultiByte(CP_UTF8,0,file,-1,single,sizeof single,NULL,NULL))
                return err(i,"selected path exceeds preview limit");
            for(char *c=single;*c;c++) if(*c=='\\') *c='/';
            (void)tg_list_append(list,sizeof list,single);
            return tg_set_result(i,list,TG_OK);
        }
        char base[MAX_WORD];
        if(!WideCharToMultiByte(CP_UTF8,0,p,-1,base,sizeof base,NULL,NULL))
            return err(i,"selected path exceeds preview limit");
        for(char *c=base;*c;c++) if(*c=='\\') *c='/';
        while(*next) {
            char name[1024],full[MAX_WORD];
            if(!WideCharToMultiByte(CP_UTF8,0,next,-1,name,sizeof name,NULL,NULL)) break;
            snprintf(full,sizeof full,"%s/%s",base,name);
            if(!tg_list_append(list,sizeof list,full)) break;
            next+=wcslen(next)+1;
        }
        return tg_set_result(i,list,TG_OK);
    }
    char result[MAX_WORD];
    if(!WideCharToMultiByte(CP_UTF8,0,file,-1,result,sizeof result,NULL,NULL))
        return err(i,"selected path exceeds preview limit");
    for(char *p=result;*p;p++) if(*p=='\\') *p='/';
    return tg_set_result(i,result,TG_OK);
}
#endif
#endif
int tg_dialog_command(Tcl_Interp *i,size_t n,char **a) {
    int message=!strcmp(a[0],"tk_messageBox"),open=!strcmp(a[0],"tk_getOpenFile"),save=!strcmp(a[0],"tk_getSaveFile");
    if(!message&&!open&&!save&&strcmp(a[0],"tk_chooseDirectory")&&strcmp(a[0],"tk_chooseColor")) return -1;
    if(n%2==0) return err(i,"dialog options must be key/value pairs");
    tg_window *w=Tk_MainWindow(i); if(!w) return err(i,"dialog requires native root window");
#ifndef _WIN32
    return err(i,"native dialog backend unavailable on this platform");
#else
    /* Tk accepts any existing widget as parent; resolve its native toplevel. */
    for(size_t k=1;k<n;k+=2) if(!strcmp(a[k],"-parent")) {
        w=tg_window_for_widget(i,a[k+1]);
        if(!w) return err(i,"dialog parent does not exist");
    }
    HWND owner=(HWND)w->native;
    if(!owner) return err(i,"dialog owner unavailable");
    if(!message) {
#ifndef TKGPU_LEGACY_WINDOWS
        if(open||save) return file_dialog(i,owner,save,n,a);
        if(!strcmp(a[0],"tk_chooseDirectory")) return dir_dialog(i,owner,n,a);
        if(!strcmp(a[0],"tk_chooseColor")) return color_dialog(i,owner,n,a);
#endif
        return err(i,"native dialog type not yet implemented");
    }
    const char *title="",*body="",*detail="",*def=NULL;
    unsigned type=MB_OK,icon=MB_ICONINFORMATION,flags=0;
    for(size_t k=1;k<n;k+=2) {
        if(!strcmp(a[k],"-title")) title=a[k+1];
        else if(!strcmp(a[k],"-message")) body=a[k+1];
        else if(!strcmp(a[k],"-detail")) detail=a[k+1];
        else if(!strcmp(a[k],"-type")) { if(!button(a[k+1],&type)) return err(i,"invalid messageBox type"); }
        else if(!strcmp(a[k],"-icon")) { if(!icon_flag(a[k+1],&icon)) return err(i,"invalid messageBox icon"); }
        else if(!strcmp(a[k],"-default")) def=a[k+1];
        else if(!strcmp(a[k],"-parent")) continue;
        else return err(i,"unsupported messageBox option");
    }
    if(def&&!default_button(type,def,&flags)) return err(i,"invalid default button");
    char text[MAX_WORD];
    int len=snprintf(text,sizeof text,"%s%s%s",body,*detail?"\n\n":"",detail);
    if(len<0||(size_t)len>=sizeof text) return err(i,"message too long");
    int answer;
#ifdef TKGPU_LEGACY_WINDOWS
    answer=MessageBoxA(owner,text,title,type|icon|flags);
#else
    WCHAR wt[512],wb[MAX_WORD];
    if(!wide(title,wt,512)||!wide(text,wb,MAX_WORD)) return err(i,"invalid dialog text");
    answer=MessageBoxW(owner,wb,wt,type|icon|flags);
#endif
    const char *s=answer==IDOK?"ok":answer==IDCANCEL?"cancel":answer==IDYES?"yes":
        answer==IDNO?"no":answer==IDRETRY?"retry":answer==IDABORT?"abort":answer==IDIGNORE?"ignore":NULL;
    return s?tg_set_result(i,s,TG_OK):err(i,"native messageBox failed");
#endif
}
