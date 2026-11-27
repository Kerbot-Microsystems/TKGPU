#include "TKGPUCompat.H"
#include "../core/TKGPUCoreInternal.H"
#include "../widgets/TKGPUWidgetInternal.H"
#include "../widgets/image/TKGPUImageRegistry.H"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* Command registration and result ownership live in their dedicated modules. */
void *Tk_FindPhoto(Tcl_Interp *i,const char *name) {
    if(!i) return NULL;
    tg_image_slot *slot=(tg_image_slot *)tg_image_find(tg_interp_ui(i),name);
    if(slot) slot->owner=i;
    return slot;
}
int Tk_PhotoGetImage(void *handle,Tk_PhotoImageBlock *block) {
    tg_image_slot *slot=handle;
    if(!slot||!slot->photo||!block) return 0;
    block->width=tg_photo_width(slot->photo); block->height=tg_photo_height(slot->photo);
    block->pitch=block->width*4; block->pixelSize=4; block->pixelPtr=tg_photo_pixels(slot->photo);
    for(int k=0;k<4;k++) block->offset[k]=k;
    return 1;
}
int Tk_PhotoPutBlock(Tcl_Interp *i,void *handle,Tk_PhotoImageBlock *block,int x,int y,int width,int height,int mode) {
    tg_image_slot *slot=handle;
    if(!i||!slot||slot->owner!=i||!slot->photo||!block||!block->pixelPtr||width<0||height<0||width>block->width||height>block->height||x<0||y<0||x>4096||y>4096||width>4096-x||height>4096-y) return TG_ERROR;
    tg_ui *u=tg_interp_ui(i);
    int old_w=tg_photo_width(slot->photo),old_h=tg_photo_height(slot->photo);
    int new_w=old_w>x+width?old_w:x+width,new_h=old_h>y+height?old_h:y+height;
    size_t before=(size_t)old_w*old_h*4,after=(size_t)new_w*new_h*4;
    if(after-before>(size_t)128*1024*1024-u->images->bytes) return TG_ERROR;
    if(new_w!=old_w||new_h!=old_h) {
        if(tg_photo_resize(slot->photo,new_w,new_h)!=TG_OK) return TG_ERROR;
        u->images->bytes+=after-before;
    }
    int rc=tg_photo_put_block(slot->photo,block->pixelPtr,block->pitch,block->pixelSize,block->offset,x,y,width,height,mode);
    if(rc==TG_OK&&i->tk_invalidate) i->tk_invalidate(i->tk_host);
    return rc;
}
