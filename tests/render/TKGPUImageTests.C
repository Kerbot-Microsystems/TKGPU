#include "TKGPURaster.H"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    CHECK(tg_pixel_blend(0x123456,0xffffff00)==0x123456);
    CHECK(tg_pixel_blend(0,0xff8000ff)==0xff8000);
    CHECK(tg_pixel_blend(0xffffff,0x00000080)==0x7f7f7f);
    tg_canvas *c=tg_canvas_create(4,4); CHECK(c);
    tg_canvas_clear(c,0x0000ff);
    const unsigned char rgba[]={255,0,0,255, 0,255,0,128, 0,0,0,0,0,0,0,0,
        255,255,255,0, 255,255,255,255};
    tg_rect q={0,0,4,4},damage;
    CHECK(tg_canvas_damage(c,&damage));
    CHECK(tg_raster_blit_rgba(c,q,1,1,2,2,rgba,16,sizeof rgba)==TG_OK);
    const uint32_t *p=tg_canvas_pixels(c);
    CHECK(p[5]==0xff0000&&p[6]==0x00807f&&p[9]==0x0000ff&&p[10]==0xffffff);
    CHECK(tg_canvas_damage(c,&damage)&&damage.x==1&&damage.y==1&&damage.w==2&&damage.h==2);
    CHECK(tg_raster_blit_rgba(c,q,1,1,2,2,rgba,16,sizeof rgba-1)==TG_ERROR);
    CHECK(tg_raster_blit_rgba(c,q,1,1,2,2,rgba,SIZE_MAX,sizeof rgba)==TG_ERROR);
    CHECK(!tg_canvas_damage(c,&damage));
    tg_canvas_clear(c,0);
    CHECK(tg_raster_blit_rgba(c,q,-1,-1,2,2,rgba,16,sizeof rgba)==TG_OK);
    CHECK(p[0]==0xffffff&&p[1]==0&&p[4]==0);
    CHECK(tg_raster_blit_rgba(c,q,INT_MAX,INT_MAX,2,2,rgba,16,sizeof rgba)==TG_OK);
    tg_canvas_destroy(c); puts("RGBA blending, clipping and bounds passed"); return 0;
}
