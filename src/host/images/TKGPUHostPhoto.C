#include "TKGPUHost.H"
#include "../../core/TKGPUCoreInternal.H"
#include "../../widgets/image/TKGPUImageRegistry.H"
#include <string.h>

static const tg_image_slot *photo(TKGPUHost *h,const char *name,int w,int height,const void *data,size_t length) {
    if(!TKGPU_HostIsOwner(h)||h->disposing||!name||!data||
       w<=0||height<=0||w>4096||height>4096||length!=(size_t)w*(size_t)height*4)
        return NULL;
    const tg_image_slot *slot=tg_image_find(tg_interp_ui(h),name);
    if(!slot||tg_photo_width(slot->photo)!=w||tg_photo_height(slot->photo)!=height)
        return NULL;
    return slot;
}
int TKGPU_HostPhotoWrite(TKGPUHost *h,const char *name,int w,int height,const void *data,size_t length) {
    const tg_image_slot *slot=photo(h,name,w,height,data,length);
    if(!slot) return TKGPU_HOST_ERROR;
    const int offsets[4]={0,1,2,3};
    int rc=tg_photo_put_block(slot->photo,data,w*4,4,offsets,0,0,w,height,1);
    if(rc==TG_OK&&h->tk_invalidate) h->tk_invalidate(h->tk_host);
    return rc;
}
int TKGPU_HostPhotoRead(TKGPUHost *h,const char *name,int w,int height,void *data,size_t length) {
    const tg_image_slot *slot=photo(h,name,w,height,data,length);
    if(!slot) return TKGPU_HOST_ERROR;
    memcpy(data,tg_photo_pixels(slot->photo),length);
    return TKGPU_HOST_OK;
}
