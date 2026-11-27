#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include "TKGPUD3D11Present.H"
#include <d3d11.h>
#include <dxgi.h>
#include <stdlib.h>
#include <string.h>
typedef HRESULT (WINAPI *create_device_swap)(IDXGIAdapter *,D3D_DRIVER_TYPE,HMODULE,UINT,
    const D3D_FEATURE_LEVEL *,UINT,UINT,const DXGI_SWAP_CHAIN_DESC *,IDXGISwapChain **,
    ID3D11Device **,D3D_FEATURE_LEVEL *,ID3D11DeviceContext **);
typedef struct { float left,top,right,bottom; uint32_t rgba; } instance;
typedef struct { UINT index; ID3D11ShaderResourceView *view; } image_draw;
enum { IMAGE_LIMIT=128,CACHE_LIMIT=128,LAYER_LIMIT=512,LAYER_DEPTH=8 };
typedef struct { UINT index; tg_rect rect; unsigned depth,begin; } layer_draw;
typedef struct { ID3D11Texture2D *texture; ID3D11ShaderResourceView *view; int width,height; } layer_texture;
#define CACHE_BYTES ((size_t)64*1024*1024)
typedef struct {
    uint32_t id; uint64_t revision,last;
    size_t offset,stride,bytes; int width,height;
    ID3D11ShaderResourceView *view;
} texture_entry;
typedef HRESULT (WINAPI *compile_shader)(const void *,SIZE_T,const char *,const void *,void *,
    const char *,const char *,UINT,UINT,ID3D10Blob **,ID3D10Blob **);
typedef struct {
    HMODULE dll;
    tg_window *owner;
    ID3D11RenderTargetView *target;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps,*image_ps;
    ID3D11SamplerState *sampler;
    image_draw images[IMAGE_LIMIT];
    unsigned image_count; size_t image_bytes;
    layer_draw layers[LAYER_LIMIT]; layer_texture scratch[LAYER_DEPTH];
    unsigned layer_count,layer_depth; size_t layer_bytes;
    texture_entry cache[CACHE_LIMIT]; size_t cache_bytes;
    uint64_t cache_clock;
    ID3D11InputLayout *layout;
    ID3D11BlendState *blend;
    ID3D11RasterizerState *raster;
    ID3D11Buffer *vertices;
    instance *batch;
    size_t count,capacity,buffer_capacity;
    IDXGISwapChain *swap;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    int width,height;
} presenter;
/* Avoid a static dxguid/d3d11 dependency: legacy software hosts never load it. */
static const IID texture2d_iid={0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static void clear_images(presenter *p) {
    for(unsigned k=0;k<p->image_count;k++) ID3D11ShaderResourceView_Release(p->images[k].view);
    p->image_count=0; p->image_bytes=0;
}
static void evict(presenter *p,unsigned k) {
    texture_entry *e=&p->cache[k];
    if(e->view) { ID3D11ShaderResourceView_Release(e->view); p->cache_bytes-=e->bytes; memset(e,0,sizeof *e); }
}
static unsigned oldest(presenter *p) {
    unsigned at=CACHE_LIMIT;
    for(unsigned k=0;k<CACHE_LIMIT;k++) if(p->cache[k].view&&(at==CACHE_LIMIT||p->cache[k].last<p->cache[at].last)) at=k;
    return at;
}
static unsigned cache_slot(presenter *p) {
    for(unsigned k=0;k<CACHE_LIMIT;k++) if(!p->cache[k].view) return k;
    return oldest(p);
}
static void release_layer(presenter *p,unsigned at) {
    layer_texture *s=&p->scratch[at];
    if(s->view) ID3D11ShaderResourceView_Release(s->view);
    if(s->texture) ID3D11Texture2D_Release(s->texture);
    p->layer_bytes-=(size_t)s->width*s->height*4;
    memset(s,0,sizeof *s);
}
void tg_d3d11_release(tg_window *w) {
    presenter *p=w->gpu_present;
    w->gpu_present=NULL; w->gpu_present_active=0; w->gpu_raster_active=0;
    if(!p) return;
    if(p->context) { ID3D11DeviceContext_ClearState(p->context); ID3D11DeviceContext_Flush(p->context); }
    if(p->target) ID3D11RenderTargetView_Release(p->target);
    if(p->vertices) ID3D11Buffer_Release(p->vertices);
    if(p->layout) ID3D11InputLayout_Release(p->layout);
    if(p->vs) ID3D11VertexShader_Release(p->vs);
    if(p->ps) ID3D11PixelShader_Release(p->ps);
    if(p->image_ps) ID3D11PixelShader_Release(p->image_ps);
    if(p->sampler) ID3D11SamplerState_Release(p->sampler);
    clear_images(p);
    for(unsigned k=0;k<LAYER_DEPTH;k++) release_layer(p,k);
    for(unsigned k=0;k<CACHE_LIMIT;k++) evict(p,k);
    if(p->blend) ID3D11BlendState_Release(p->blend);
    if(p->raster) ID3D11RasterizerState_Release(p->raster);
    free(p->batch);
    if(p->swap) IDXGISwapChain_Release(p->swap);
    if(p->context) ID3D11DeviceContext_Release(p->context);
    if(p->device) ID3D11Device_Release(p->device);
    if(p->dll) FreeLibrary(p->dll);
    free(p);
}
static presenter *create(tg_window *w,HWND hwnd,int width,int height) {
    presenter *p=calloc(1,sizeof *p); if(!p) return NULL;
    w->gpu_present=p; p->owner=w;
    /* System32 only; do not load a DLL from the working directory. */
    WCHAR path[MAX_PATH];
    UINT n=GetSystemDirectoryW(path,MAX_PATH);
    const WCHAR tail[]=L"\\d3d11.dll";
    if(!n||n>=MAX_PATH||n+sizeof tail/sizeof tail[0]>MAX_PATH) goto fail;
    for(size_t k=0;k<sizeof tail/sizeof tail[0];k++) path[n+k]=tail[k];
    p->dll=LoadLibraryW(path); if(!p->dll) goto fail;
    create_device_swap fn=(create_device_swap)GetProcAddress(p->dll,"D3D11CreateDeviceAndSwapChain");
    if(!fn) goto fail;
    DXGI_SWAP_CHAIN_DESC d={0};
    d.BufferDesc.Width=(UINT)width; d.BufferDesc.Height=(UINT)height;
    d.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count=1; d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount=2; d.OutputWindow=hwnd; d.Windowed=TRUE; d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr=fn(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        NULL,0,D3D11_SDK_VERSION,&d,&p->swap,&p->device,NULL,&p->context);
    if(FAILED(hr)||!p->swap||!p->device||!p->context) goto fail;
    p->width=width; p->height=height; return p;
fail:
    tg_d3d11_release(w); return NULL;
}
int tg_d3d11_present(tg_window *w,HWND hwnd,int width,int height) {
    w->gpu_raster_active=0;
    if(width<=0||height<=0||width>16384||height>16384||
       width!=tg_canvas_width(w->canvas)||height!=tg_canvas_height(w->canvas)) {
        /* Never mix GDI and a live swap chain on a stale-size frame. */
        tg_d3d11_release(w); return 0;
    }
    presenter *p=w->gpu_present;
    if(!p) { p=create(w,hwnd,width,height); if(!p) return -1; }
    if(p->width!=width||p->height!=height) {
        ID3D11DeviceContext_ClearState(p->context);
        if(p->target) { ID3D11RenderTargetView_Release(p->target); p->target=NULL; }
        ID3D11DeviceContext_Flush(p->context);
        if(FAILED(IDXGISwapChain_ResizeBuffers(p->swap,0,(UINT)width,(UINT)height,DXGI_FORMAT_UNKNOWN,0))) goto fail;
        p->width=width; p->height=height;
    }
    ID3D11Texture2D *back=NULL;
    HRESULT hr=IDXGISwapChain_GetBuffer(p->swap,0,&texture2d_iid,(void **)&back);
    if(FAILED(hr)||!back) goto fail;
    /* The CPU canvas uses 0x00RRGGBB: little-endian Windows stores B,G,R,0.
       An opaque window swap chain ignores the alpha byte. */
    ID3D11DeviceContext_UpdateSubresource(p->context,(ID3D11Resource *)back,0,NULL,
        tg_canvas_pixels(w->canvas),(UINT)width*4u,0);
    ID3D11Texture2D_Release(back);
    hr=IDXGISwapChain_Present(p->swap,1,0);
    if(hr==DXGI_STATUS_OCCLUDED) { tg_d3d11_release(w); return 0; }
    if(FAILED(hr)) goto fail;
    w->gpu_present_active=1; return 1;
fail:
    tg_d3d11_release(w); return -1;
}

/* Load the system compiler only when the raster path is explicitly enabled. */
static HMODULE compiler(void) {
    static const WCHAR names[][24]={L"\\d3dcompiler_47.dll",L"\\d3dcompiler_43.dll"};
    WCHAR path[MAX_PATH]; UINT n=GetSystemDirectoryW(path,MAX_PATH);
    if(!n||n>=MAX_PATH) return NULL;
    for(unsigned k=0;k<2;k++) {
        size_t z=0; while(names[k][z]) z++;
        if(n+z>=MAX_PATH) continue;
        for(size_t j=0;j<=z;j++) path[n+j]=names[k][j];
        HMODULE dll=LoadLibraryW(path); if(dll) return dll;
    }
    return NULL;
}
static int pipeline(presenter *p) {
    if(p->vs&&p->ps&&p->image_ps&&p->sampler&&p->layout&&p->blend&&p->raster) return 1;
    static const char source[]=
        "struct V { float4 rect:RECT; float4 color:COLOR; };"
        "struct P { float4 pos:SV_POSITION; float4 color:COLOR; float2 uv:TEXCOORD0; };"
        "P vs(V i,uint id:SV_VertexID) { P o; float2 uv=float2(id&1,id>>1); o.pos=float4(lerp(i.rect.xy,i.rect.zw,uv),0,1); o.color=i.color; o.uv=uv; return o; }"
        "float4 ps(P i):SV_TARGET { return i.color; }"
        "Texture2D tex:register(t0); SamplerState samp:register(s0);"
        "float4 image_ps(P i):SV_TARGET { float4 v=tex.Sample(samp,i.uv); if(i.color.r<0.5) v.a=i.color.a; return v; }";
    HMODULE dll=compiler(); if(!dll) return 0;
    compile_shader compile=(compile_shader)GetProcAddress(dll,"D3DCompile");
    ID3D10Blob *vs=NULL,*ps=NULL,*image=NULL;
    int ok=0;
    if(!compile||FAILED(compile(source,sizeof source-1,NULL,NULL,NULL,"vs","vs_4_0",0,0,&vs,NULL))||!vs||
       FAILED(compile(source,sizeof source-1,NULL,NULL,NULL,"ps","ps_4_0",0,0,&ps,NULL))||!ps||
       FAILED(compile(source,sizeof source-1,NULL,NULL,NULL,"image_ps","ps_4_0",0,0,&image,NULL))||!image) goto done;
    void *v=vs->lpVtbl->GetBufferPointer(vs),*s=ps->lpVtbl->GetBufferPointer(ps);
    SIZE_T vn=vs->lpVtbl->GetBufferSize(vs),sn=ps->lpVtbl->GetBufferSize(ps);
    if(FAILED(ID3D11Device_CreateVertexShader(p->device,v,vn,NULL,&p->vs))||
       FAILED(ID3D11Device_CreatePixelShader(p->device,s,sn,NULL,&p->ps))||
       FAILED(ID3D11Device_CreatePixelShader(p->device,image->lpVtbl->GetBufferPointer(image),
                    image->lpVtbl->GetBufferSize(image),NULL,&p->image_ps))) goto done;
    D3D11_SAMPLER_DESC sd={0};
    sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT; sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc=D3D11_COMPARISON_NEVER; sd.MaxAnisotropy=1; sd.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(ID3D11Device_CreateSamplerState(p->device,&sd,&p->sampler))) goto done;
    D3D11_INPUT_ELEMENT_DESC input[]={
        {"RECT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
        {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,16,D3D11_INPUT_PER_INSTANCE_DATA,1}
    };
    if(FAILED(ID3D11Device_CreateInputLayout(p->device,input,2,v,vn,&p->layout))) goto done;
    D3D11_BLEND_DESC bd={0};
    bd.RenderTarget[0].BlendEnable=TRUE;
    bd.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_RASTERIZER_DESC rd={0};
    rd.FillMode=D3D11_FILL_SOLID; rd.CullMode=D3D11_CULL_NONE; rd.DepthClipEnable=TRUE;
    if(FAILED(ID3D11Device_CreateBlendState(p->device,&bd,&p->blend))||
       FAILED(ID3D11Device_CreateRasterizerState(p->device,&rd,&p->raster))) goto done;
    ok=1;
done:
    if(vs) vs->lpVtbl->Release(vs);
    if(ps) ps->lpVtbl->Release(ps);
    if(image) image->lpVtbl->Release(image);
    FreeLibrary(dll); return ok;
}
static int record(void *ctx,tg_rect r,uint32_t rgb,unsigned alpha) {
    presenter *p=ctx;
    enum { LIMIT=262144 }; /* Bounded host/GPU instance buffers. */
    if(p->count==LIMIT) return TG_ERROR;
    if(p->count==p->capacity) {
        size_t cap=p->capacity?p->capacity*2:4096;
        if(cap>LIMIT) cap=LIMIT;
        instance *next=realloc(p->batch,cap*sizeof *next);
        if(!next) return TG_ERROR;
        p->batch=next; p->capacity=cap;
    }
    float l=2.0f*r.x/p->width-1.0f,rr=2.0f*(r.x+r.w)/p->width-1.0f;
    float t=1.0f-2.0f*r.y/p->height,b=1.0f-2.0f*(r.y+r.h)/p->height;
    uint32_t rgba=((rgb>>16)&255u)|(rgb&0xff00u)|((rgb&255u)<<16)|(alpha<<24);
    p->batch[p->count++]=(instance){l,t,rr,b,rgba}; return TG_OK;
}
static int record_image(void *ctx,tg_rect r,const unsigned char *source,size_t stride,size_t length,
                        uint32_t id,uint64_t revision,size_t source_offset) {
    presenter *p=ctx;
    size_t bytes=(size_t)r.w*r.h*4;
    if(p->image_count==IMAGE_LIMIT||bytes>(size_t)64*1024*1024-p->image_bytes||
       stride>UINT32_MAX||stride<(size_t)r.w*4||
       (size_t)(r.h-1)*stride+(size_t)r.w*4>length) return TG_ERROR;
    ID3D11ShaderResourceView *view=NULL;
    if(id&&revision) {
        for(unsigned k=0;k<CACHE_LIMIT;k++) {
            texture_entry *e=&p->cache[k];
            if(e->view&&e->id==id&&e->revision!=revision) evict(p,k);
            else if(e->view&&e->id==id&&e->revision==revision&&e->offset==source_offset&&
                    e->stride==stride&&e->width==r.w&&e->height==r.h) {
                view=e->view; ID3D11ShaderResourceView_AddRef(view);
                e->last=++p->cache_clock; p->owner->gpu_texture_hits++; break;
            }
        }
    }
    if(!view) {
    D3D11_TEXTURE2D_DESC desc={0};
    desc.Width=(UINT)r.w; desc.Height=(UINT)r.h; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data={0}; data.pSysMem=source; data.SysMemPitch=(UINT)stride;
    ID3D11Texture2D *tex=NULL;
    if(FAILED(ID3D11Device_CreateTexture2D(p->device,&desc,&data,&tex))||!tex) return TG_ERROR;
    HRESULT hr=ID3D11Device_CreateShaderResourceView(p->device,(ID3D11Resource *)tex,NULL,&view);
    ID3D11Texture2D_Release(tex);
    if(FAILED(hr)||!view) return TG_ERROR;
    p->owner->gpu_texture_uploads++; p->owner->gpu_upload_bytes+=bytes;
    if(id&&revision&&bytes<=CACHE_BYTES) {
        while(p->cache_bytes>CACHE_BYTES-bytes) evict(p,oldest(p));
        unsigned at=cache_slot(p); evict(p,at);
        ID3D11ShaderResourceView_AddRef(view);
        p->cache[at]=(texture_entry){id,revision,++p->cache_clock,source_offset,stride,bytes,r.w,r.h,view};
        p->cache_bytes+=bytes;
    }
    }
    UINT index=(UINT)p->count;
    if(record(ctx,r,0xffffff,255)!=TG_OK) { ID3D11ShaderResourceView_Release(view); return TG_ERROR; }
    p->images[p->image_count++]=(image_draw){index,view}; p->image_bytes+=bytes;
    return TG_OK;
}
static int record_layer(void *ctx,tg_rect r,unsigned opacity,int begin) {
    presenter *p=ctx;
    if(p->layer_count==LAYER_LIMIT||!opacity||opacity>=255||r.w<=0||r.h<=0) return TG_ERROR;
    if(begin) {
        if(p->layer_depth==LAYER_DEPTH) return TG_ERROR;
        p->layers[p->layer_count++]=(layer_draw){(UINT)p->count,r,p->layer_depth++,1};
    } else {
        if(!p->layer_depth) return TG_ERROR;
        UINT at=(UINT)p->count;
        if(record(ctx,r,0,255-opacity)!=TG_OK) return TG_ERROR;
        p->layers[p->layer_count++]=(layer_draw){at,r,--p->layer_depth,0};
    }
    return TG_OK;
}
static int layer_copy(presenter *p,const layer_draw *draw) {
    layer_texture *s=&p->scratch[draw->depth];
    tg_rect r=draw->rect;
    if(!s->texture||s->width!=r.w||s->height!=r.h) {
        size_t bytes=(size_t)r.w*r.h*4;
        release_layer(p,draw->depth);
        /* Bound retained intermediate textures. Higher levels are not
           active at this point, so they may be reclaimed safely. */
        for(unsigned k=draw->depth+1;k<LAYER_DEPTH&&bytes>CACHE_BYTES-p->layer_bytes;k++) release_layer(p,k);
        if(bytes>CACHE_BYTES-p->layer_bytes) return 0;
        D3D11_TEXTURE2D_DESC desc={0};
        desc.Width=(UINT)r.w; desc.Height=(UINT)r.h; desc.MipLevels=1; desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(ID3D11Device_CreateTexture2D(p->device,&desc,NULL,&s->texture))) return 0;
        if(FAILED(ID3D11Device_CreateShaderResourceView(p->device,(ID3D11Resource *)s->texture,NULL,&s->view))) {
            release_layer(p,draw->depth); return 0;
        }
        s->width=r.w; s->height=r.h; p->layer_bytes+=bytes;
    }
    ID3D11Resource *back=NULL;
    ID3D11RenderTargetView_GetResource(p->target,&back);
    if(!back) return 0;
    ID3D11DeviceContext_OMSetRenderTargets(p->context,0,NULL,NULL);
    D3D11_BOX box={(UINT)r.x,(UINT)r.y,0,(UINT)(r.x+r.w),(UINT)(r.y+r.h),1};
    ID3D11DeviceContext_CopySubresourceRegion(p->context,(ID3D11Resource *)s->texture,0,0,0,0,back,0,&box);
    ID3D11Resource_Release(back);
    ID3D11DeviceContext_OMSetRenderTargets(p->context,1,&p->target,NULL);
    return 1;
}
static void draw_texture(presenter *p,UINT index,ID3D11ShaderResourceView *view) {
    ID3D11DeviceContext_PSSetShader(p->context,p->image_ps,NULL,0);
    ID3D11DeviceContext_PSSetShaderResources(p->context,0,1,&view);
    ID3D11DeviceContext_DrawInstanced(p->context,4,1,0,index);
    ID3D11ShaderResourceView *empty=NULL;
    ID3D11DeviceContext_PSSetShaderResources(p->context,0,1,&empty);
    ID3D11DeviceContext_PSSetShader(p->context,p->ps,NULL,0);
}
static int upload_vertices(presenter *p) {
    if(!p->count) return 1;
    if(p->buffer_capacity<p->count) {
        D3D11_BUFFER_DESC d={0};
        d.ByteWidth=(UINT)(p->capacity*sizeof(instance)); d.Usage=D3D11_USAGE_DYNAMIC;
        d.BindFlags=D3D11_BIND_VERTEX_BUFFER; d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        ID3D11Buffer *next=NULL;
        if(FAILED(ID3D11Device_CreateBuffer(p->device,&d,NULL,&next))||!next) return 0;
        if(p->vertices) ID3D11Buffer_Release(p->vertices);
        p->vertices=next; p->buffer_capacity=p->capacity;
    }
    D3D11_MAPPED_SUBRESOURCE map;
    if(FAILED(ID3D11DeviceContext_Map(p->context,(ID3D11Resource *)p->vertices,0,D3D11_MAP_WRITE_DISCARD,0,&map))) return 0;
    memcpy(map.pData,p->batch,p->count*sizeof(instance));
    ID3D11DeviceContext_Unmap(p->context,(ID3D11Resource *)p->vertices,0);
    p->owner->gpu_upload_bytes+=p->count*sizeof(instance); return 1;
}
int tg_d3d11_raster(tg_window *w,HWND hwnd,int width,int height) {
    w->gpu_raster_active=0;
    if(width<=0||height<=0||width>16384||height>16384) return 0;
    tg_ui *u=tg_interp_ui(w->interp);
    /* The interpreter scene is shared by all native toplevels. Always select
       this window's root before recording a GPU frame. */
    if(!tg_window_select(w,width,height)) return 0;
    presenter *p=w->gpu_present;
    if(!p) { p=create(w,hwnd,width,height); if(!p) return -1; }
    if(p->width!=width||p->height!=height) {
        ID3D11DeviceContext_ClearState(p->context);
        if(p->target) { ID3D11RenderTargetView_Release(p->target); p->target=NULL; }
        if(FAILED(IDXGISwapChain_ResizeBuffers(p->swap,0,(UINT)width,(UINT)height,DXGI_FORMAT_UNKNOWN,0))) goto fail;
        p->width=width; p->height=height;
    }
    p->count=0; p->layer_count=p->layer_depth=0; clear_images(p);
    if(tg_ui_record_composite(u,width,height,record,record_image,record_layer,p)!=TG_OK||p->layer_depth) {
        clear_images(p); return 0;
    }
    if(!pipeline(p)||!upload_vertices(p)) goto fail;
    HRESULT hr;
    if(!p->target) {
        ID3D11Texture2D *back=NULL;
        hr=IDXGISwapChain_GetBuffer(p->swap,0,&texture2d_iid,(void **)&back);
        if(FAILED(hr)||!back) goto fail;
        hr=ID3D11Device_CreateRenderTargetView(p->device,(ID3D11Resource *)back,NULL,&p->target);
        ID3D11Texture2D_Release(back);
        if(FAILED(hr)||!p->target) goto fail;
    }
    ID3D11RenderTargetView *rt=p->target;
    ID3D11DeviceContext_OMSetRenderTargets(p->context,1,&rt,NULL);
    const float clear[4]={0,0,0,1};
    ID3D11DeviceContext_ClearRenderTargetView(p->context,rt,clear);
    D3D11_VIEWPORT viewport={0,0,(float)width,(float)height,0,1};
    ID3D11DeviceContext_RSSetViewports(p->context,1,&viewport);
    ID3D11DeviceContext_RSSetState(p->context,p->raster);
    ID3D11DeviceContext_OMSetBlendState(p->context,p->blend,NULL,0xffffffffu);
    UINT stride=sizeof(instance),offset=0;
    ID3D11DeviceContext_IASetInputLayout(p->context,p->layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(p->context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_IASetVertexBuffers(p->context,0,1,&p->vertices,&stride,&offset);
    ID3D11DeviceContext_VSSetShader(p->context,p->vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(p->context,p->ps,NULL,0);
    UINT first=0;
    ID3D11DeviceContext_PSSetSamplers(p->context,0,1,&p->sampler);
    unsigned image=0,layer=0;
    while(image<p->image_count||layer<p->layer_count) {
        int is_layer=layer<p->layer_count&&(image==p->image_count||p->layers[layer].index<=p->images[image].index);
        UINT at=is_layer?p->layers[layer].index:p->images[image].index;
        if(at>first) ID3D11DeviceContext_DrawInstanced(p->context,4,at-first,0,first);
        first=at;
        if(is_layer) {
            layer_draw *draw=&p->layers[layer++];
            if(draw->begin) { if(!layer_copy(p,draw)) goto fail; }
            else {
                draw_texture(p,at,p->scratch[draw->depth].view); first=at+1;
                w->gpu_layer_passes++;
            }
        } else {
            draw_texture(p,at,p->images[image++].view); first=at+1;
        }
    }
    if(p->count>first) ID3D11DeviceContext_DrawInstanced(p->context,4,(UINT)p->count-first,0,first);
    clear_images(p);
    ID3D11DeviceContext_OMSetRenderTargets(p->context,0,NULL,NULL);
    hr=IDXGISwapChain_Present(p->swap,1,0);
    if(hr==DXGI_STATUS_OCCLUDED) { tg_d3d11_release(w); return 0; }
    if(FAILED(hr)) goto fail;
    w->gpu_frames++; w->gpu_instances+=p->count;
    w->gpu_present_active=1; w->gpu_raster_active=1; w->dirty=0; return 1;
fail:
    tg_d3d11_release(w); return -1;
}
