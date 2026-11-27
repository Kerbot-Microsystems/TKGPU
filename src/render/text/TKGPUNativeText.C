#include "TKGPUText.H"
#include "TKGPURaster.H"
#include "TKGPUUtf8.H"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../../core/TKGPUList.H"
#define TEXT_PIXELS (4u*1024u*1024u)
#define TEXT_BYTES 4096u
#if defined(TKGPU_TEXT_PANGO) || defined(TKGPU_TEXT_UNISCRIBE)
static int valid(const char *text,int scale) {
    if(!text||scale<1||scale>16) return 0;
    size_t len=strlen(text),count;
    return len&&len<TEXT_BYTES&&tg_utf8_count((const unsigned char *)text,len,&count)==TG_OK;
}
static int dimensions(int w,int h) {
    return w>0&&h>0&&w<=16384&&h<=16384&&(size_t)w*h<=TEXT_PIXELS;
}
static void emit(tg_canvas *c,tg_rect clip,int x,int y,int w,int h,
                 const unsigned char *data,size_t stride,unsigned step,uint32_t rgb) {
    tg_rect q;
    if(!tg_rect_intersect(clip,(tg_rect){x,y,w,h},&q)) return;
    for(int row=q.y-y;row<q.y-y+q.h;row++) {
        const unsigned char *p=data+(size_t)row*stride;
        int col=q.x-x,end=col+q.w;
        while(col<end) {
            unsigned alpha=p[(size_t)col*step]; int first=col++;
            while(col<end&&p[(size_t)col*step]==alpha) col++;
            if(alpha) tg_canvas_fill_alpha(c,(tg_rect){x+first,y+row,col-first,1},rgb,alpha);
        }
    }
}
#endif
#if defined(TKGPU_TEXT_PANGO)
#include <pango/pangocairo.h>
static int native(const char *text,int scale,int *width,int *height,
                  tg_canvas *c,tg_rect clip,int x,int y,uint32_t rgb,
                  size_t *byte,int *pixel,int hit,size_t selection_end,const char *font_spec,tg_visual_cursor *cursor) {
    if(!valid(text,scale)) return TG_ERROR;
    int rc=TG_ERROR;
    PangoFontMap *map=pango_cairo_font_map_get_default();
    if(!map) return TG_ERROR;
    PangoContext *context=pango_font_map_create_context(map);
    if(!context) return TG_ERROR;
    PangoLayout *layout=pango_layout_new(context);
    g_object_unref(context);
    if(!layout) return TG_ERROR;
    PangoFontDescription *font=pango_font_description_from_string("sans");
    if(!font) { g_object_unref(layout); return TG_ERROR; }
    double pixels=12.0*scale;
    if(font_spec&&*font_spec) {
        const char *cur=font_spec; char family[128],value[32];
        if(tg_list_next(&cur,family,sizeof family)==1&&tg_list_next(&cur,value,sizeof value)==1) {
            char *end; double points=strtod(value,&end);
            if(*value&&!*end&&points!=0.0&&points>=-256.0&&points<=256.0) {
                double dpi=pango_cairo_context_get_resolution(pango_layout_get_context(layout));
                if(dpi<=0.0) dpi=96.0;
                pixels=(points<0?-points:points*dpi/72.0)*scale;
                pango_font_description_set_family(font,family);
                while(tg_list_next(&cur,value,sizeof value)==1) {
                    if(!strcmp(value,"bold")) pango_font_description_set_weight(font,PANGO_WEIGHT_BOLD);
                    if(!strcmp(value,"italic")) pango_font_description_set_style(font,PANGO_STYLE_ITALIC);
                }
            }
        }
    }
    pango_font_description_set_absolute_size(font,pixels*PANGO_SCALE);
    pango_layout_set_font_description(layout,font);
    pango_font_description_free(font);
    pango_layout_set_text(layout,text,-1);
    pango_layout_set_auto_dir(layout,TRUE);
    pango_layout_set_single_paragraph_mode(layout,TRUE);
    if(pango_layout_get_unknown_glyphs_count(layout)) goto done;
    PangoRectangle ink,logical;
    pango_layout_get_pixel_extents(layout,&ink,&logical);
    int left=ink.x<logical.x?ink.x:logical.x,top=ink.y<logical.y?ink.y:logical.y;
    int right=ink.x+ink.width>logical.x+logical.width?ink.x+ink.width:logical.x+logical.width;
    int bottom=ink.y+ink.height>logical.y+logical.height?ink.y+ink.height:logical.y+logical.height;
    int w=right-left,h=bottom-top;
    if(!dimensions(w,h)) goto done;
    if(c&&byte&&!pixel) {
        PangoLayoutLine *line=pango_layout_get_line_readonly(layout,0);
        int *ranges=NULL,nranges=0;
        if(!line) goto done;
        pango_layout_line_get_x_ranges(line,(int)*byte,(int)selection_end,&ranges,&nranges);
        for(int k=0;k<nranges;k++) {
            int a=PANGO_PIXELS(ranges[2*k])-left,b=PANGO_PIXELS(ranges[2*k+1])-left;
            if(b>a) tg_raster_fill(c,clip,(tg_rect){x+a,y,b-a,h},rgb);
        }
        g_free(ranges);
        *width=w; *height=h; rc=TG_OK; goto done;
    }
    if(byte&&pixel) {
        if(hit==2||hit==3) {
            int index=0,trailing=0,old=(int)*byte,old_trailing=0,weak=0;
            if(cursor&&cursor->valid&&cursor->offset==*byte&&cursor->index<=strlen(text)) {
                old=(int)cursor->index; old_trailing=cursor->trailing; weak=cursor->weak;
            }
            pango_layout_move_cursor_visually(layout,!weak,old,old_trailing,hit==2?-1:1,&index,&trailing);
            /* Retain the raw cluster position, not just its normalized byte.
               At the edge keep the exact previous affinity. */
            if(index>=0&&index!=G_MAXINT&&index<=(int)strlen(text)) {
                const char *at=text+index; int remain=trailing;
                while(remain-->0&&*at) at=g_utf8_next_char(at);
                *byte=(size_t)(at-text);
                if(cursor) *cursor=(tg_visual_cursor){*byte,(size_t)index,trailing,weak,1};
            }
        } else if(hit) {
            int index=0,trailing=0,px=*pixel;
            if(px< -32768) px=-32768;
            if(px>32768) px=32768;
            pango_layout_xy_to_index(layout,(px+left)*PANGO_SCALE,0,&index,&trailing);
            const char *at=text+index; int remain=trailing;
            while(remain-->0&&*at) at=g_utf8_next_char(at);
            *byte=(size_t)(at-text);
            if(cursor) {
                PangoRectangle strong,weak;
                pango_layout_get_cursor_pos(layout,(int)*byte,&strong,&weak);
                int s=PANGO_PIXELS(strong.x)-left,b=PANGO_PIXELS(weak.x)-left;
                *cursor=(tg_visual_cursor){*byte,(size_t)index,trailing,abs(px-b)<abs(px-s),1};
            }
        } else {
            PangoRectangle strong,weak;
            pango_layout_get_cursor_pos(layout,(int)*byte,&strong,&weak);
            int use_weak=cursor&&cursor->valid&&cursor->offset==*byte&&cursor->weak;
            *pixel=PANGO_PIXELS(use_weak?weak.x:strong.x)-left;
        }
    }
    if(c) {
        cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_A8,w,h);
        cairo_t *cr=cairo_create(surface);
        cairo_set_source_rgba(cr,1,1,1,1);
        cairo_move_to(cr,-left,-top);
        pango_cairo_show_layout(cr,layout);
        cairo_surface_flush(surface);
        if(cairo_status(cr)==CAIRO_STATUS_SUCCESS&&cairo_surface_status(surface)==CAIRO_STATUS_SUCCESS) {
            emit(c,clip,x,y,w,h,cairo_image_surface_get_data(surface),(size_t)cairo_image_surface_get_stride(surface),1,rgb);
            rc=TG_OK;
        }
        cairo_destroy(cr); cairo_surface_destroy(surface);
    } else rc=TG_OK;
    if(rc==TG_OK) { *width=w; *height=h; }
done:
    g_object_unref(layout); return rc;
}
const char *tg_text_native_backend(void) { return "pango-cairo"; }
#elif defined(TKGPU_TEXT_UNISCRIBE)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <usp10.h>

#ifdef TKGPU_WINDOWS_XP
/* Recent MinGW import libraries route several ScriptString entry points to
   GDI32. XP exports them from USP10, so resolve that system DLL explicitly. */
#define XP_SCRIPT_POINTER(name) static __typeof__(&name) xp_##name
XP_SCRIPT_POINTER(ScriptStringAnalyse);
XP_SCRIPT_POINTER(ScriptStringFree);
XP_SCRIPT_POINTER(ScriptStringOut);
XP_SCRIPT_POINTER(ScriptString_pLogAttr);
XP_SCRIPT_POINTER(ScriptString_pSize);
XP_SCRIPT_POINTER(ScriptStringCPtoX);
static volatile LONG xp_script_state;
static int xp_script_init(void) {
    if(InterlockedCompareExchange(&xp_script_state,1,0)==0) {
        WCHAR path[MAX_PATH];
        UINT n=GetSystemDirectoryW(path,MAX_PATH);
        HMODULE module=NULL;
        int ok=n>0&&n<MAX_PATH-12;
        if(ok) { lstrcatW(path,L"\\usp10.dll"); module=LoadLibraryW(path); ok=module!=NULL; }
#define XP_SCRIPT_LOAD(name) do { \
        xp_##name=(__typeof__(&name))GetProcAddress(module,#name); \
        if(!xp_##name) ok=0; \
    } while(0)
        if(module) {
            XP_SCRIPT_LOAD(ScriptStringAnalyse);
            XP_SCRIPT_LOAD(ScriptStringFree);
            XP_SCRIPT_LOAD(ScriptStringOut);
            XP_SCRIPT_LOAD(ScriptString_pLogAttr);
            XP_SCRIPT_LOAD(ScriptString_pSize);
            XP_SCRIPT_LOAD(ScriptStringCPtoX);
            if(!ok) FreeLibrary(module);
        }
        InterlockedExchange(&xp_script_state,ok?2:3);
    } else {
        while(InterlockedCompareExchange(&xp_script_state,1,1)==1) Sleep(0);
    }
    return InterlockedCompareExchange(&xp_script_state,2,2)==2;
}
#define ScriptStringAnalyse xp_ScriptStringAnalyse
#define ScriptStringFree xp_ScriptStringFree
#define ScriptStringOut xp_ScriptStringOut
#define ScriptString_pLogAttr xp_ScriptString_pLogAttr
#define ScriptString_pSize xp_ScriptString_pSize
#define ScriptStringCPtoX xp_ScriptStringCPtoX
#endif
enum { TEXT_CACHE_CAP=4096 };
typedef struct {
    uint64_t hash,stamp;
    int scale,w,h;
    char *text,*spec;
    unsigned char *mask;
} text_cache_entry;
#ifdef TKGPU_WINDOWS_XP
/* XP has Uniscribe, but SRW locks were introduced with Vista. */
static CRITICAL_SECTION g_text_cache_lock;
static volatile LONG g_text_cache_lock_state;
static void text_cache_lock(void) {
    if(InterlockedCompareExchange(&g_text_cache_lock_state,1,0)==0) {
        InitializeCriticalSection(&g_text_cache_lock);
        InterlockedExchange(&g_text_cache_lock_state,2);
    } else {
        while(InterlockedCompareExchange(&g_text_cache_lock_state,2,2)!=2) Sleep(0);
    }
    EnterCriticalSection(&g_text_cache_lock);
}
#define TEXT_CACHE_READ_LOCK() text_cache_lock()
#define TEXT_CACHE_WRITE_LOCK() text_cache_lock()
#define TEXT_CACHE_READ_UNLOCK() LeaveCriticalSection(&g_text_cache_lock)
#define TEXT_CACHE_WRITE_UNLOCK() LeaveCriticalSection(&g_text_cache_lock)
#else
static SRWLOCK g_text_cache_lock=SRWLOCK_INIT;
#define TEXT_CACHE_READ_LOCK() AcquireSRWLockShared(&g_text_cache_lock)
#define TEXT_CACHE_WRITE_LOCK() AcquireSRWLockExclusive(&g_text_cache_lock)
#define TEXT_CACHE_READ_UNLOCK() ReleaseSRWLockShared(&g_text_cache_lock)
#define TEXT_CACHE_WRITE_UNLOCK() ReleaseSRWLockExclusive(&g_text_cache_lock)
#endif
static text_cache_entry g_text_cache[TEXT_CACHE_CAP];
static uint64_t g_text_cache_clock=1;
static uint64_t text_cache_hash(const char *text,const char *spec,int scale) {
    uint64_t h=14695981039346656037ULL^(uint64_t)(unsigned)scale;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) { h^=*p; h*=1099511628211ULL; }
    h^=0xffu; h*=1099511628211ULL;
    if(spec) for(const unsigned char *p=(const unsigned char *)spec;*p;p++) { h^=*p; h*=1099511628211ULL; }
    return h?h:1;
}
static int text_cache_lookup(const char *text,const char *spec,int scale,
                             int *width,int *height,tg_canvas *c,tg_rect clip,int x,int y,uint32_t rgb) {
    uint64_t h=text_cache_hash(text,spec,scale);
    const char *sp=spec?spec:"";
    size_t idx=(size_t)(h&(TEXT_CACHE_CAP-1));
    TEXT_CACHE_READ_LOCK();
    for(size_t probe=0;probe<8;probe++) {
        text_cache_entry *e=&g_text_cache[(idx+probe)&(TEXT_CACHE_CAP-1)];
        if(e->hash==h&&e->scale==scale&&e->text&&!strcmp(e->text,text)&&!strcmp(e->spec,sp)) {
            if(c&&!e->mask) { TEXT_CACHE_READ_UNLOCK(); return 0; }
            if(c) emit(c,clip,x,y,e->w,e->h,e->mask,(size_t)e->w,1,rgb);
            *width=e->w; *height=e->h;
            TEXT_CACHE_READ_UNLOCK();
            return 1;
        }
    }
    TEXT_CACHE_READ_UNLOCK();
    return 0;
}
static void text_cache_store(const char *text,const char *spec,int scale,int w,int h,const unsigned char *mask) {
    if(w<=0||h<=0||(size_t)w*(size_t)h>262144u) return;
    size_t tlen=strlen(text),slen=spec?strlen(spec):0;
    if(tlen>512||slen>256) return;
    uint64_t hval=text_cache_hash(text,spec,scale);
    const char *sp=spec?spec:"";
    size_t idx=(size_t)(hval&(TEXT_CACHE_CAP-1));
    unsigned char *mcopy=NULL;
    if(mask) {
        mcopy=malloc((size_t)w*(size_t)h);
        if(!mcopy) return;
        memcpy(mcopy,mask,(size_t)w*(size_t)h);
    }
    char *tcopy=malloc(tlen+1),*scopy=malloc(slen+1);
    if(!tcopy||!scopy) { free(mcopy); free(tcopy); free(scopy); return; }
    memcpy(tcopy,text,tlen+1); memcpy(scopy,sp,slen+1);
    TEXT_CACHE_WRITE_LOCK();
    size_t victim=idx; uint64_t oldest=UINT64_MAX;
    for(size_t probe=0;probe<8;probe++) {
        size_t pos=(idx+probe)&(TEXT_CACHE_CAP-1);
        text_cache_entry *e=&g_text_cache[pos];
        if(e->hash==hval&&e->scale==scale&&e->text&&!strcmp(e->text,text)&&!strcmp(e->spec,sp)) {
            victim=pos; break;
        }
        if(!e->hash) { victim=pos; break; }
        if(e->stamp<oldest) { oldest=e->stamp; victim=pos; }
    }
    text_cache_entry *slot=&g_text_cache[victim];
    if(slot->hash==hval&&slot->scale==scale&&slot->text&&!strcmp(slot->text,text)&&!strcmp(slot->spec,sp)&&slot->mask&&!mcopy) {
        free(tcopy); free(scopy);
        slot->stamp=++g_text_cache_clock;
        TEXT_CACHE_WRITE_UNLOCK();
        return;
    }
    free(slot->text); free(slot->spec); free(slot->mask);
    slot->hash=hval; slot->stamp=++g_text_cache_clock;
    slot->scale=scale; slot->w=w; slot->h=h;
    slot->text=tcopy; slot->spec=scopy; slot->mask=mcopy;
    TEXT_CACHE_WRITE_UNLOCK();
}
/* Distinct visual positions for both sides of a logical bidi boundary. */
static int caret_stop(const WCHAR *wide,const SCRIPT_LOGATTR *attrs,int count,int cp) {
    return cp>=0&&cp<=count&&(!cp||cp==count||
        (!(wide[cp]>=0xdc00&&wide[cp]<=0xdfff)&&(!attrs||attrs[cp].fCharStop)));
}
static int caret_x(SCRIPT_STRING_ANALYSIS analysis,const WCHAR *wide,
                   const SCRIPT_LOGATTR *attrs,int count,int cp,int trailing,int *x) {
    if(!caret_stop(wide,attrs,count,cp)) return 0;
    if(cp==count) trailing=1;
    if(!cp) trailing=0;
    if(trailing) {
        int before=cp-1;
        while(before>0&&!caret_stop(wide,attrs,count,before)) before--;
        return SUCCEEDED(ScriptStringCPtoX(analysis,before,TRUE,x));
    }
    return SUCCEEDED(ScriptStringCPtoX(analysis,cp,FALSE,x));
}
static int native(const char *text,int scale,int *width,int *height,
                  tg_canvas *c,tg_rect clip,int x,int y,uint32_t rgb,
                  size_t *byte,int *pixel,int hit,size_t selection_end,const char *font_spec,tg_visual_cursor *cursor) {
    if(!valid(text,scale)) return TG_ERROR;
#ifdef TKGPU_WINDOWS_XP
    if(!xp_script_init()) return TG_ERROR;
#endif
    if(!byte&&!pixel&&text_cache_lookup(text,font_spec,scale,width,height,c,clip,x,y,rgb)) return TG_OK;
    WCHAR wide[TEXT_BYTES];
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,TEXT_BYTES);
    if(count<=1) return TG_ERROR;
    count--;
    HDC dc=CreateCompatibleDC(NULL);
    if(!dc) return TG_ERROR;
    int pixels=12*scale,weight=FW_NORMAL,italic=0;
    WCHAR family[128]=L"Segoe UI";
    if(font_spec&&*font_spec) {
        const char *cursor=font_spec; char name[128],value[32];
        if(tg_list_next(&cursor,name,sizeof name)==1&&tg_list_next(&cursor,value,sizeof value)==1) {
            char *end; double points=strtod(value,&end);
            if(*value&&!*end&&points!=0.0&&points>=-256.0&&points<=256.0) {
                if(points<0.0) pixels=(int)(-points+0.5);
                else pixels=(int)(points*GetDeviceCaps(dc,LOGPIXELSY)/72.0+0.5);
                if(pixels<1) pixels=1;
                if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,family,128)) { DeleteDC(dc); return TG_ERROR; }
                while(tg_list_next(&cursor,value,sizeof value)==1) { if(!strcmp(value,"bold")) weight=FW_BOLD; if(!strcmp(value,"italic")) italic=1; }
            }
        }
    }
    const int samples=2;
    HFONT font=CreateFontW(-pixels*samples,0,0,0,weight,italic,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,family);
    HGDIOBJ oldfont=NULL,oldbitmap=NULL;
    HBITMAP bitmap=NULL;
    SCRIPT_STRING_ANALYSIS analysis=NULL;
    int rc=TG_ERROR;
    if(!font) goto done;
    oldfont=SelectObject(dc,font);
    if(!oldfont||oldfont==HGDI_ERROR) goto done;
    DWORD flags=SSA_GLYPHS|SSA_FALLBACK|SSA_LINK;
    WORD types[TEXT_BYTES];
    if(GetStringTypeW(CT_CTYPE2,wide,count,types)) {
        for(int k=0;k<count;k++) {
            if(types[k]==C2_RIGHTTOLEFT) { flags|=SSA_RTL; break; }
            if(types[k]==C2_LEFTTORIGHT) break;
        }
    }
    if(ScriptStringAnalyse(dc,wide,count,0,-1,flags,0,NULL,NULL,NULL,NULL,NULL,&analysis)!=S_OK) goto done;
    const SIZE *size=ScriptString_pSize(analysis);
    if(!size||!dimensions(size->cx,size->cy)) goto done;
    int raw_w=size->cx,raw_h=size->cy;
    int w=(raw_w+samples-1)/samples,h=(raw_h+samples-1)/samples;
    if(c&&byte&&!pixel) {
        int first=*byte?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,(int)*byte,NULL,0):0;
        int last=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,(int)selection_end,NULL,0);
        if((*byte&&first<=0)||last<=first||last>count) goto done;
        /* A logical selection can consist of several disjoint visual runs. */
        for(int k=first;k<last;k++) {
            int x1=0,x2=0;
            if(FAILED(ScriptStringCPtoX(analysis,k,FALSE,&x1))||
               FAILED(ScriptStringCPtoX(analysis,k,TRUE,&x2))) goto done;
            int lo=(x1<x2?x1:x2)/samples,hi=((x1>x2?x1:x2)+samples-1)/samples;
            if(hi>lo) tg_raster_fill(c,clip,(tg_rect){x+lo,y,hi-lo,h},rgb);
        }
        *width=w; *height=h; rc=TG_OK; goto done;
    }
    if(byte&&pixel) {
        const SCRIPT_LOGATTR *attrs=ScriptString_pLogAttr(analysis);
        int cp=0,side=0,pos=0;
        if(hit!=1) {
            cp=*byte?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,(int)*byte,NULL,0):0;
            if((*byte&&cp<=0)||cp>count) goto done;
            while(cp>0&&!caret_stop(wide,attrs,count,cp)) cp--;
            side=cp==count;
            if(cursor&&cursor->valid&&cursor->offset==*byte&&cursor->index==(size_t)cp)
                side=cursor->trailing!=0;
            if(!caret_x(analysis,wide,attrs,count,cp,side,&pos)) goto done;
        }
        if(hit) {
            if(hit==1) { int px=*pixel; if(px<0) px=0; if(px>w) px=w; pos=px*samples; }
            int best=cp,best_side=side,distance=INT_MAX;
            for(int k=0;k<=count;k++) {
                if(!caret_stop(wide,attrs,count,k)) continue;
                for(int edge=0;edge<2;edge++) {
                    if((k==0&&edge)||(k==count&&!edge)) continue;
                    int x;
                    if(!caret_x(analysis,wide,attrs,count,k,edge,&x)) continue;
                    int delta=hit==2?pos-x:hit==3?x-pos:abs(x-pos);
                    if(hit!=1&&delta<=0) continue;
                    if(delta<distance||(delta==distance&&edge==side&&best_side!=side)) {
                        best=k; best_side=edge; distance=delta;
                    }
                }
            }
            if(hit==1&&distance==INT_MAX) goto done;
            int bytes=best?WideCharToMultiByte(CP_UTF8,0,wide,best,NULL,0,NULL,NULL):0;
            if(best&&bytes<=0) goto done;
            *byte=(size_t)bytes;
            if(cursor) *cursor=(tg_visual_cursor){*byte,(size_t)best,best_side,best_side,1};
        } else *pixel=(pos+samples/2)/samples;
    }
    if(c) {
        BITMAPINFO bi={0};
        bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth=raw_w; bi.bmiHeader.biHeight=-raw_h;
        bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
        void *bits=NULL;
        bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,NULL,0);
        if(!bitmap||!bits) goto done;
        oldbitmap=SelectObject(dc,bitmap);
        if(!oldbitmap||oldbitmap==HGDI_ERROR) goto done;
        memset(bits,0,(size_t)raw_w*raw_h*4);
        SetTextColor(dc,RGB(255,255,255)); SetBkColor(dc,RGB(0,0,0)); SetBkMode(dc,TRANSPARENT);
        SetTextAlign(dc,TA_LEFT|TA_TOP|TA_NOUPDATECP);
        if(ScriptStringOut(analysis,0,0,0,NULL,0,0,FALSE)!=S_OK) goto done;
        GdiFlush();
        unsigned char *mask=malloc((size_t)w*h); if(!mask) goto done;
        for(int row=0;row<h;row++) for(int col=0;col<w;col++) {
            unsigned sum=0,count_samples=0;
            for(int dy=0;dy<samples&&row*samples+dy<raw_h;dy++) for(int dx=0;dx<samples&&col*samples+dx<raw_w;dx++) {
                const unsigned char *pixel=(const unsigned char *)bits+((size_t)(row*samples+dy)*raw_w+col*samples+dx)*4;
                sum+=(pixel[0]+pixel[1]+pixel[2]+1)/3; count_samples++;
            }
            mask[(size_t)row*w+col]=(unsigned char)((sum+count_samples/2)/count_samples);
        }
        emit(c,clip,x,y,w,h,mask,(size_t)w,1,rgb);
        if(!byte&&!pixel) text_cache_store(text,font_spec,scale,w,h,mask);
        free(mask);
    } else if(!byte&&!pixel) {
        text_cache_store(text,font_spec,scale,w,h,NULL);
    }
    *width=w; *height=h; rc=TG_OK;
done:
    if(analysis) ScriptStringFree(&analysis);
    if(oldbitmap&&oldbitmap!=HGDI_ERROR) SelectObject(dc,oldbitmap);
    if(bitmap) DeleteObject(bitmap);
    if(oldfont&&oldfont!=HGDI_ERROR) SelectObject(dc,oldfont);
    if(font) DeleteObject(font);
    DeleteDC(dc); return rc;
}
const char *tg_text_native_backend(void) { return "uniscribe"; }
#else
#include "TKGPULegacyUnicode.H"
static int native(const char *text,int scale,int *width,int *height,
                  tg_canvas *c,tg_rect clip,int x,int y,uint32_t rgb,
                  size_t *byte,int *pixel,int hit,size_t selection_end,const char *font_spec,tg_visual_cursor *cursor) {
    (void)font_spec; if(cursor&&hit) cursor->valid=0;
    if(hit==2||hit==3) return TG_ERROR; /* Legacy logical navigation remains available. */
    return tg_legacy_unicode(getenv("TKGPU_LEGACY_FONT_PACK"),text,scale,width,height,
                             c,clip,x,y,rgb,byte,pixel,hit,selection_end);
}
const char *tg_text_native_backend(void) {
    const char *path=getenv("TKGPU_LEGACY_FONT_PACK");
    return path&&*path?"bitmap-unicode":"bitmap";
}
#endif
int tg_text_native_measure(const char *text,int scale,int *w,int *h) {
    if(!w||!h) return TG_ERROR;
    return native(text,scale,w,h,NULL,(tg_rect){0,0,0,0},0,0,0,NULL,NULL,0,0,NULL,NULL);
}
int tg_text_native_draw(tg_canvas *c,tg_rect clip,int x,int y,const char *text,int scale,uint32_t rgb) {
    int w,h;
    if(!c||x< -65536||x>65536||y< -65536||y>65536) return TG_ERROR;
    return native(text,scale,&w,&h,c,clip,x,y,rgb,NULL,NULL,0,0,NULL,NULL);
}
int tg_text_native_enabled(void) {
    const char *s=getenv("TKGPU_NATIVE_TEXT");
    return s&&strcmp(s,"1")==0;
}

/* Byte offsets are UTF-8 boundaries, never visual character indices. */
int tg_text_native_caret(const char *text,int scale,size_t byte,int *pixel) {
    if(!text||!pixel||scale<1||scale>16||byte>strlen(text)||
       ((unsigned char)text[byte]&0xc0)==0x80) return TG_ERROR;
    if(!*text) { *pixel=0; return TG_OK; }
    int w,h;
    return native(text,scale,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,&byte,pixel,0,0,NULL,NULL);
}
int tg_text_native_hit(const char *text,int scale,int pixel,size_t *byte) {
    if(!text||!byte||scale<1||scale>16) return TG_ERROR;
    if(!*text) { *byte=0; return TG_OK; }
    int w,h;
    return native(text,scale,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,byte,&pixel,1,0,NULL,NULL);
}

int tg_text_native_selection(tg_canvas *c,tg_rect clip,int x,int y,const char *text,
                             int scale,size_t first,size_t last,uint32_t rgb) {
    if(!c||!text||first>last||last>strlen(text)||scale<1||scale>16||
       x< -65536||x>65536||y< -65536||y>65536||
       ((unsigned char)text[first]&0xc0)==0x80||
       ((unsigned char)text[last]&0xc0)==0x80) return TG_ERROR;
    if(first==last) return TG_OK;
    int w,h;
    return native(text,scale,&w,&h,c,clip,x,y,rgb,&first,NULL,0,last,NULL,NULL);
}

int tg_text_font_measure(const char *text,const char *font,int *width,int *height) {
    if(!text||!width||!height) return TG_ERROR;
    if(!*text) { *width=0; *height=12; return TG_OK; }
    return native(text,1,width,height,NULL,(tg_rect){0,0,0,0},0,0,0,NULL,NULL,0,0,font,NULL);
}
int tg_text_font_draw(tg_canvas *c,tg_rect clip,int x,int y,const char *text,const char *font,uint32_t rgb) {
    int width,height;
    return native(text,1,&width,&height,c,clip,x,y,rgb,NULL,NULL,0,0,font,NULL);
}

int tg_text_font_caret(const char *text,const char *font,size_t byte,int *pixel) {
    if(!text||!pixel||byte>strlen(text)||((unsigned char)text[byte]&0xc0)==0x80) return TG_ERROR;
    if(!*text) { *pixel=0; return TG_OK; } int w,h;
    return native(text,1,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,&byte,pixel,0,0,font,NULL);
}
int tg_text_font_hit(const char *text,const char *font,int pixel,size_t *byte) {
    if(!text||!byte) return TG_ERROR;
    if(!*text) { *byte=0; return TG_OK; } int w,h;
    return native(text,1,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,byte,&pixel,1,0,font,NULL);
}
int tg_text_font_selection(tg_canvas *c,tg_rect clip,int x,int y,const char *text,const char *font,size_t first,size_t last,uint32_t rgb) {
    if(!c||!text||first>last||last>strlen(text)||
       ((unsigned char)text[first]&0xc0)==0x80||((unsigned char)text[last]&0xc0)==0x80) return TG_ERROR;
    if(first==last) return TG_OK; int w,h;
    return native(text,1,&w,&h,c,clip,x,y,rgb,&first,NULL,0,last,font,NULL);
}

/* Visual caret step in a shaped run; public Host ABI stays unchanged. */
int tg_text_font_move(const char *text,const char *font,size_t *byte,int direction) {
    if(!text||!byte||(direction!=-1&&direction!=1)||*byte>strlen(text)||
       ((unsigned char)text[*byte]&0xc0)==0x80) return TG_ERROR;
    if(!*text) return TG_OK;
    int w,h,pixel=0;
    return native(text,1,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,byte,&pixel,direction<0?2:3,0,font,NULL);
}

int tg_text_font_visual_hit(const char *text,const char *font,int pixel,size_t *byte,tg_visual_cursor *cursor) {
    if(!text||!byte||!cursor) return TG_ERROR;
    cursor->valid=0;
    if(!*text) { *byte=0; return TG_OK; }
    int w,h;
    return native(text,1,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,byte,&pixel,1,0,font,cursor);
}
int tg_text_font_visual_move(const char *text,const char *font,size_t *byte,int direction,tg_visual_cursor *cursor) {
    if(!text||!byte||!cursor||(direction!=-1&&direction!=1)||*byte>strlen(text)||
       ((unsigned char)text[*byte]&0xc0)==0x80) return TG_ERROR;
    if(!*text) { cursor->valid=0; return TG_OK; }
    int w,h,pixel=0;
    return native(text,1,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,byte,&pixel,direction<0?2:3,0,font,cursor);
}
int tg_text_font_visual_caret(const char *text,const char *font,size_t byte,int *pixel,const tg_visual_cursor *cursor) {
    if(!text||!pixel||byte>strlen(text)||((unsigned char)text[byte]&0xc0)==0x80) return TG_ERROR;
    if(!*text) { *pixel=0; return TG_OK; }
    tg_visual_cursor state={0}; if(cursor) state=*cursor;
    int w,h;
    return native(text,1,&w,&h,NULL,(tg_rect){0,0,0,0},0,0,0,&byte,pixel,0,0,font,&state);
}
