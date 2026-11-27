#include "../../src/render/text/TKGPULegacyUnicode.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); remove(path); return 1; } } while(0)
static void glyph(FILE *f,unsigned cp,int width) {
    unsigned char r[37]={0}; r[0]=(unsigned char)cp; r[1]=(unsigned char)(cp>>8);
    r[2]=(unsigned char)(cp>>16); r[3]=(unsigned char)(cp>>24); r[4]=(unsigned char)width;
    r[5]=0x80; fwrite(r,1,sizeof r,f);
}
int main(void) {
    const char *path="tkgpu-test-unicode.tgf";
    FILE *f=fopen(path,"wb"); CHECK(f);
    const unsigned char header[12]={'T','G','F','O','N','T','1',0,5,0,0,0};
    fwrite(header,1,12,f);
    glyph(f,0x0628,8); glyph(f,0x4e2d,16); glyph(f,0xac00,16); glyph(f,0xfe90,8); glyph(f,0xfe91,8);
    fclose(f);
    tg_canvas *c=tg_canvas_create(64,32); CHECK(c);
    tg_rect clip={0,0,64,32}; int w,h,p; size_t byte=0;
    CHECK(tg_legacy_unicode(path,"\xe4\xb8\xad\xea\xb0\x80",1,&w,&h,c,clip,0,0,0xffffff,NULL,NULL,0,0)==TG_OK&&w==32&&h==16);
    const uint32_t *pixels=tg_canvas_pixels(c); CHECK(pixels[0]&&pixels[16]);
    CHECK(tg_legacy_unicode(path,"\xd8\xa8\xd8\xa8",1,&w,&h,NULL,clip,0,0,0,&byte,&p,0,0)==TG_OK&&p==16);
    p=0; CHECK(tg_legacy_unicode(path,"\xd8\xa8\xd8\xa8",1,&w,&h,NULL,clip,0,0,0,&byte,&p,1,0)==TG_OK&&byte==4);
    CHECK(tg_legacy_unicode(path,"?",1,&w,&h,NULL,clip,0,0,0,NULL,NULL,0,0)==TG_ERROR);
    tg_canvas_destroy(c); remove(path); return 0;
}
