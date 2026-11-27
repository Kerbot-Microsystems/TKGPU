#include "TKGPUWindow.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../core/TKGPUList.H"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "TKGPUText.H"
#include "../../widgets/TKGPUWidgetInternal.H"
#if defined(_WIN32) && !defined(TKGPU_LEGACY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
typedef struct {
    char *result;
    size_t cap;
    char last[LF_FACESIZE*4];
    int failed;
} families;
static int to_utf8(const WCHAR *s,char *out,int cap) {
    return WideCharToMultiByte(CP_UTF8,0,s,-1,out,cap,NULL,NULL)>0;
}
static int to_wide(const char *s,WCHAR *out,int cap) {
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out,cap)>0;
}
static int CALLBACK enum_font(const LOGFONTW *font,const TEXTMETRICW *metrics,DWORD type,LPARAM context) {
    (void)metrics; (void)type;
    families *f=(families *)context; char name[LF_FACESIZE*4];
    if(font->lfFaceName[0]==L'@') return 1;
    if(!to_utf8(font->lfFaceName,name,sizeof name)) { f->failed=1; return 0; }
    if(!strcmp(name,f->last)) return 1;
    /* Deduplicate without fixed per-family slots. */
    const char *p=f->result; char current[LF_FACESIZE*4]; int rc;
    while((rc=tg_list_next(&p,current,sizeof current))>0) if(!strcmp(current,name)) return 1;
    if(rc<0) { f->failed=1; return 0; }
    if(!tg_list_append(f->result,f->cap,name)) return 0;
    strcpy(f->last,name); return 1;
}
static int stock_description(const char *spec,LOGFONTW *lf) {
    int stock=DEFAULT_GUI_FONT;
    if(!strcmp(spec,"TkFixedFont")) stock=SYSTEM_FIXED_FONT;
    else if(strcmp(spec,"TkDefaultFont")&&strcmp(spec,"TkTextFont")&&
        strcmp(spec,"TkMenuFont")&&strcmp(spec,"TkHeadingFont")&&
        strcmp(spec,"TkCaptionFont")&&strcmp(spec,"TkSmallCaptionFont")&&
        strcmp(spec,"TkIconFont")&&strcmp(spec,"TkTooltipFont")) return 0;
    if(GetObjectW(GetStockObject(stock),sizeof *lf,lf)!=sizeof *lf) return 0;
    if(!strcmp(spec,"TkHeadingFont")) lf->lfWeight=FW_BOLD;
    return 1;
}
static HFONT resolve_font(HDC dc,const char *spec) {
    LOGFONTW lf={0};
    if(stock_description(spec,&lf)) return CreateFontIndirectW(&lf);
    const char *p=spec; char family[LF_FACESIZE*4],size[32],style[64];
    if(tg_list_next(&p,family,sizeof family)!=1||
       tg_list_next(&p,size,sizeof size)!=1||!to_wide(family,lf.lfFaceName,LF_FACESIZE)) return NULL;
    char *end; errno=0; long points=strtol(size,&end,10);
    if(errno||end==size||*end||!points||points>256||points< -256) return NULL;
    lf.lfHeight=points<0?(LONG)points:-MulDiv((int)points,GetDeviceCaps(dc,LOGPIXELSY),72);
    lf.lfWeight=FW_NORMAL; lf.lfCharSet=DEFAULT_CHARSET;
    int rc;
    while((rc=tg_list_next(&p,style,sizeof style))>0) {
        if(!strcmp(style,"bold")) lf.lfWeight=FW_BOLD;
        else if(!strcmp(style,"normal")) lf.lfWeight=FW_NORMAL;
        else if(!strcmp(style,"italic")) lf.lfItalic=TRUE;
        else if(!strcmp(style,"roman")) lf.lfItalic=FALSE;
        else if(!strcmp(style,"underline")) lf.lfUnderline=TRUE;
        else if(!strcmp(style,"overstrike")) lf.lfStrikeOut=TRUE;
        else return NULL;
    }
    if(rc<0) return NULL;
    return CreateFontIndirectW(&lf);
}
static int format_metric(Tcl_Interp *i,const TEXTMETRICW *m,const char *key) {
    int value;
    if(!strcmp(key,"-ascent")) value=m->tmAscent;
    else if(!strcmp(key,"-descent")) value=m->tmDescent;
    else if(!strcmp(key,"-linespace")) value=m->tmHeight+m->tmExternalLeading;
    else if(!strcmp(key,"-fixed")) value=!(m->tmPitchAndFamily&TMPF_FIXED_PITCH);
    else return tg_set_result(i,"unknown font metric",TG_ERROR);
    char b[32]; snprintf(b,sizeof b,"%d",value); return tg_set_result(i,b,TG_OK);
}
static int native_font(Tcl_Interp *i,tg_window *w,size_t n,char **a) {
    if(n<2) return tg_set_result(i,"missing font subcommand",TG_ERROR);
    HWND hwnd=(HWND)w->native;
    HDC dc=GetDC(hwnd);
    if(!dc) return tg_set_result(i,"cannot obtain native font context",TG_ERROR);
    int rc=TG_ERROR;
    if(!strcmp(a[1],"families")&&n==2) {
        size_t cap=65536;
        char *buf=calloc(1,cap);
        if(!buf) { ReleaseDC(hwnd,dc); return tg_set_result(i,"cannot allocate font families buffer",TG_ERROR); }
        families f={buf,cap,"",0}; LOGFONTW query={0}; query.lfCharSet=DEFAULT_CHARSET;
        EnumFontFamiliesExW(dc,&query,enum_font,(LPARAM)&f,0);
        rc=f.failed||!buf[0]?tg_set_result(i,"font families enumeration failed",TG_ERROR):
            tg_set_result(i,buf,TG_OK);
        free(buf);
        ReleaseDC(hwnd,dc); return rc;
    }
    if(n<3||(strcmp(a[1],"measure")&&strcmp(a[1],"metrics")&&strcmp(a[1],"actual"))) {
        ReleaseDC(hwnd,dc); return tg_set_result(i,"unsupported font subcommand",TG_ERROR);
    }
    HFONT font=resolve_font(dc,a[2]);
    if(!font) { ReleaseDC(hwnd,dc); return tg_set_result(i,"unsupported font description",TG_ERROR); }
    HGDIOBJ old=SelectObject(dc,font);
    if(!old||old==HGDI_ERROR) { DeleteObject(font); ReleaseDC(hwnd,dc); return tg_set_result(i,"cannot select font",TG_ERROR); }
    TEXTMETRICW m;
    rc=tg_set_result(i,"unsupported font query",TG_ERROR);
    if(!strcmp(a[1],"measure")&&n==4) {
        WCHAR wide[MAX_WORD]; SIZE extent;
        if(to_wide(a[3],wide,MAX_WORD)&&GetTextExtentPoint32W(dc,wide,(int)wcslen(wide),&extent)) {
            char b[32]; snprintf(b,sizeof b,"%ld",(long)extent.cx); rc=tg_set_result(i,b,TG_OK);
        }
    } else if(GetTextMetricsW(dc,&m)) {
        if(!strcmp(a[1],"metrics")&&n==4) rc=format_metric(i,&m,a[3]);
        else if(!strcmp(a[1],"metrics")&&n==3) {
            char b[128]; snprintf(b,sizeof b,"-ascent %ld -descent %ld -linespace %ld -fixed %d",
                (long)m.tmAscent,(long)m.tmDescent,(long)(m.tmHeight+m.tmExternalLeading),!(m.tmPitchAndFamily&TMPF_FIXED_PITCH));
            rc=tg_set_result(i,b,TG_OK);
        } else if(!strcmp(a[1],"actual")&&n==4) {
            if(!strcmp(a[3],"-family")) {
                WCHAR face[LF_FACESIZE]; char b[LF_FACESIZE*4];
                if(GetTextFaceW(dc,LF_FACESIZE,face)&&to_utf8(face,b,sizeof b)) rc=tg_set_result(i,b,TG_OK);
            } else if(!strcmp(a[3],"-weight")) rc=tg_set_result(i,m.tmWeight>=FW_BOLD?"bold":"normal",TG_OK);
            else if(!strcmp(a[3],"-slant")) rc=tg_set_result(i,m.tmItalic?"italic":"roman",TG_OK);
            else if(!strcmp(a[3],"-underline")) rc=tg_set_result(i,m.tmUnderlined?"1":"0",TG_OK);
            else if(!strcmp(a[3],"-overstrike")) rc=tg_set_result(i,m.tmStruckOut?"1":"0",TG_OK);
            else if(!strcmp(a[3],"-size")) {
                char b[32]; snprintf(b,sizeof b,"%d",MulDiv(m.tmHeight-m.tmInternalLeading,72,GetDeviceCaps(dc,LOGPIXELSY)));
                rc=tg_set_result(i,b,TG_OK);
            }
        }
    }
    SelectObject(dc,old); DeleteObject(font); ReleaseDC(hwnd,dc); return rc;
}
#endif
enum { FONT_LIMIT=256 };
typedef struct {
    char name[128],family[128];
    int size,bold,italic,underline,overstrike,stock;
} named_font;
struct tg_named_fonts { size_t count; unsigned next; named_font items[FONT_LIMIT]; };
static const char *stock_names[]={"TkDefaultFont","TkTextFont","TkFixedFont","TkMenuFont","TkHeadingFont","TkCaptionFont","TkSmallCaptionFont","TkIconFont","TkTooltipFont"};
static named_font defaults(void) {
    named_font f={0}; f.size=10;
#if defined(_WIN32) && !defined(TKGPU_LEGACY_WINDOWS)
    strcpy(f.family,"Segoe UI");
#else
    strcpy(f.family,"TKGPU Bitmap");
#endif
    return f;
}
static struct tg_named_fonts *registry(tg_window *w) {
    if(!w->fonts) {
        w->fonts=calloc(1,sizeof *w->fonts); if(!w->fonts) return NULL;
        for(size_t k=0;k<sizeof stock_names/sizeof stock_names[0];k++) {
            named_font f=defaults(); f.stock=1;
            strcpy(f.name,stock_names[k]); f.bold=k==4;
#if defined(_WIN32) && !defined(TKGPU_LEGACY_WINDOWS)
            if(k==2) strcpy(f.family,"Consolas");
#endif
            w->fonts->items[w->fonts->count++]=f;
        }
    }
    return w->fonts;
}
static named_font *find_font(struct tg_named_fonts *r,const char *name) {
    for(size_t k=0;k<r->count;k++) if(!strcmp(r->items[k].name,name)) return &r->items[k];
    return NULL;
}
static int integer(const char *s,int *value) {
    char *end; errno=0; long n=strtol(s,&end,10);
    if(errno||end==s||*end||!n||n< -256||n>256) return 0;
    *value=(int)n; return 1;
}
static int boolean(const char *s,int *value) {
    if(!strcmp(s,"1")||!strcmp(s,"true")||!strcmp(s,"yes")||!strcmp(s,"on")) *value=1;
    else if(!strcmp(s,"0")||!strcmp(s,"false")||!strcmp(s,"no")||!strcmp(s,"off")) *value=0;
    else return 0;
    return 1;
}
static int font_option(named_font *f,const char *key,const char *value) {
    if(!strcmp(key,"-family")) {
        if(!*value||strlen(value)>=sizeof f->family) return 0;
        strcpy(f->family,value); return 1;
    }
    if(!strcmp(key,"-size")) return integer(value,&f->size);
    if(!strcmp(key,"-weight")) {
        if(strcmp(value,"bold")&&strcmp(value,"normal")) return 0;
        f->bold=!strcmp(value,"bold"); return 1;
    }
    if(!strcmp(key,"-slant")) {
        if(strcmp(value,"italic")&&strcmp(value,"roman")) return 0;
        f->italic=!strcmp(value,"italic"); return 1;
    }
    if(!strcmp(key,"-underline")) return boolean(value,&f->underline);
    if(!strcmp(key,"-overstrike")) return boolean(value,&f->overstrike);
    return 0;
}
static const char *font_value(const named_font *f,const char *key,char b[32]) {
    if(!strcmp(key,"-family")) return f->family;
    if(!strcmp(key,"-size")) { snprintf(b,32,"%d",f->size); return b; }
    if(!strcmp(key,"-weight")) return f->bold?"bold":"normal";
    if(!strcmp(key,"-slant")) return f->italic?"italic":"roman";
    if(!strcmp(key,"-underline")) return f->underline?"1":"0";
    if(!strcmp(key,"-overstrike")) return f->overstrike?"1":"0";
    return NULL;
}
static int font_error(Tcl_Interp *i) { return tg_set_result(i,"invalid or unsupported named font operation",TG_ERROR); }
static int font_values(Tcl_Interp *i,const named_font *f,const char *key) {
    static const char *keys[]={"-family","-size","-weight","-slant","-underline","-overstrike"};
    char word[32],b[512]="";
    if(key) { const char *v=font_value(f,key,word); return v?tg_set_result(i,v,TG_OK):font_error(i); }
    for(unsigned k=0;k<sizeof keys/sizeof keys[0];k++)
        if(!tg_list_append(b,sizeof b,keys[k])||!tg_list_append(b,sizeof b,font_value(f,keys[k],word))) return font_error(i);
    return tg_set_result(i,b,TG_OK);
}
static int font_spec(const named_font *f,char *b,size_t cap) {
    char size[32]; snprintf(size,sizeof size,"%d",f->size); b[0]=0;
    return tg_list_append(b,cap,f->family)&&tg_list_append(b,cap,size)&&
        (!f->bold||tg_list_append(b,cap,"bold"))&&(!f->italic||tg_list_append(b,cap,"italic"))&&
        (!f->underline||tg_list_append(b,cap,"underline"))&&(!f->overstrike||tg_list_append(b,cap,"overstrike"));
}
static void publish_fonts(Tcl_Interp *i,struct tg_named_fonts *fonts) {
    tg_ui *u=tg_interp_ui(i); u->font_count=(unsigned)fonts->count;
    for(unsigned k=0;k<u->font_count;k++) {
        strcpy(u->fonts[k].name,fonts->items[k].name);
        if(!font_spec(&fonts->items[k],u->fonts[k].spec,sizeof u->fonts[k].spec)) u->fonts[k].spec[0]=0;
    }
    for(size_t k=0;k<u->count;k++) {
        tg_widget *w=&u->items[k]; const char *spec=tg_ui_font_spec(u,w->font);
        if(strlen(spec)<sizeof w->font_spec) strcpy(w->font_spec,spec);
    }
}
static int parse_spec(const char *s,named_font *f) {
    char word[128]; int rc;
    if(tg_list_next(&s,word,sizeof word)!=1||!font_option(f,"-family",word)) return 0;
    rc=tg_list_next(&s,word,sizeof word);
    if(rc==0) return 1;
    if(rc!=1||!integer(word,&f->size)) return 0;
    while((rc=tg_list_next(&s,word,sizeof word))>0) {
        if(!strcmp(word,"bold")) f->bold=1;
        else if(!strcmp(word,"normal")) f->bold=0;
        else if(!strcmp(word,"italic")) f->italic=1;
        else if(!strcmp(word,"roman")) f->italic=0;
        else if(!strcmp(word,"underline")) f->underline=1;
        else if(!strcmp(word,"overstrike")) f->overstrike=1;
        else return 0;
    }
    return rc==0;
}
int tg_font_command(Tcl_Interp *i,size_t n,char **a) {
    if(strcmp(a[0],"font")) return -1;
    tg_window *w=Tk_MainWindow(i);
    if(!w||n<2) return font_error(i);
    struct tg_named_fonts *r=registry(w);
    if(!r) return font_error(i);
    if(!strcmp(a[1],"names")&&n==2) {
        char b[MAX_WORD]="";
        for(size_t k=0;k<r->count;k++) if(!tg_list_append(b,sizeof b,r->items[k].name)) return font_error(i);
        return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[1],"create")) {
        named_font f=defaults(); size_t k=2;
        if(r->count==FONT_LIMIT) return font_error(i);
        if(n>2&&a[2][0]!='-') {
            if(!*a[2]||strlen(a[2])>=sizeof f.name||find_font(r,a[2])) return font_error(i);
            strcpy(f.name,a[2]); k++;
        } else {
            do {
                if(r->next==UINT32_MAX) return font_error(i);
                snprintf(f.name,sizeof f.name,"font%u",++r->next);
            } while(find_font(r,f.name));
        }
        if((n-k)%2) return font_error(i);
        for(;k<n;k+=2) if(!font_option(&f,a[k],a[k+1])) return font_error(i);
        r->items[r->count++]=f; publish_fonts(i,r); return tg_set_result(i,f.name,TG_OK);
    }
    if(!strcmp(a[1],"delete")) {
        if(n<3) return font_error(i);
        for(size_t k=2;k<n;k++) {
            named_font *f=find_font(r,a[k]); if(!f||f->stock) return font_error(i);
        }
        for(size_t k=2;k<n;k++) {
            named_font *f=find_font(r,a[k]); if(!f) continue;
            size_t pos=(size_t)(f-r->items);
            memmove(f,f+1,(r->count-pos-1)*sizeof *f); r->count--;
        }
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"configure")||!strcmp(a[1],"config")) {
        if(n<3) return font_error(i);
        named_font *f=find_font(r,a[2]); if(!f) return font_error(i);
        if(n<=4) return font_values(i,f,n==4?a[3]:NULL);
        if((n-3)%2) return font_error(i);
        named_font copy=*f;
        for(size_t k=3;k<n;k+=2) if(!font_option(&copy,a[k],a[k+1])) return font_error(i);
        *f=copy; publish_fonts(i,r); if(i->tk_invalidate) i->tk_invalidate(i->tk_host);
        return tg_set_result(i,"",TG_OK);
    }
    if(!strcmp(a[1],"families")) {
#if defined(_WIN32) && !defined(TKGPU_LEGACY_WINDOWS)
        return native_font(i,w,n,a);
#else
        return n==2?tg_set_result(i,"{TKGPU Bitmap}",TG_OK):font_error(i);
#endif
    }
    if(n<3) return font_error(i);
    named_font f=defaults(),*named=find_font(r,a[2]);
    if(named) f=*named;
    else if(!parse_spec(a[2],&f)) return font_error(i);
    /* Tkinter optional displayof argument: the single root is the only native
       display context supported; validate the requested widget exists. */
    char *args[6]; size_t used=0;
    for(size_t k=0;k<n;k++) {
        if(k>=3&&!strcmp(a[k],"-displayof")) {
            if(k+1>=n||!tg_ui_find(tg_interp_ui(i),a[++k])) return font_error(i);
            continue;
        }
        if(used==6) return font_error(i);
        args[used++]=a[k];
    }
    n=used;
    if(!strcmp(a[1],"actual")&&(n==3||n==4)) {
#if defined(_WIN32) && !defined(TKGPU_LEGACY_WINDOWS)
        if(n==4) {
            char spec[256]; if(!font_spec(&f,spec,sizeof spec)) return font_error(i);
            args[2]=spec; return native_font(i,w,n,args);
        }
#endif
        return font_values(i,&f,n==4?args[3]:NULL);
    }
#if defined(_WIN32) && !defined(TKGPU_LEGACY_WINDOWS)
    char spec[256]; if(!font_spec(&f,spec,sizeof spec)) return font_error(i);
    args[2]=spec; return native_font(i,w,n,args);
#else
    int scale=((f.size<0?-f.size:f.size)+4)/8; if(scale<1) scale=1; if(scale>16) scale=16;
    char b[128];
    if(!strcmp(a[1],"measure")&&n==4) {
        int width,height;
        if(tg_text_measure(args[3],scale,&width,&height)!=TG_OK) return font_error(i);
        snprintf(b,sizeof b,"%d",width); return tg_set_result(i,b,TG_OK);
    }
    if(!strcmp(a[1],"metrics")&&(n==3||n==4)) {
        int v;
        if(n==3) snprintf(b,sizeof b,"-ascent %d -descent %d -linespace %d -fixed 1",7*scale,scale,8*scale);
        else {
            if(!strcmp(args[3],"-ascent")) v=7*scale;
            else if(!strcmp(args[3],"-descent")) v=scale;
            else if(!strcmp(args[3],"-linespace")) v=8*scale;
            else if(!strcmp(args[3],"-fixed")) v=1;
            else return font_error(i);
            snprintf(b,sizeof b,"%d",v);
        }
        return tg_set_result(i,b,TG_OK);
    }
    return font_error(i);
#endif
}
