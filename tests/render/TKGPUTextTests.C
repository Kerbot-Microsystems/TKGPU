#include "TKGPUText.H"
#include <stdio.h>
#include <string.h>
#include "../../src/render/text/TKGPUFallbackFont.H"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    int w,h;
    CHECK(tg_text_measure("A1",2,&w,&h)==TG_OK&&w==22&&h==14);
    CHECK(tg_text_measure("\xd0\x90",1,&w,&h)==TG_OK&&w==5&&h==7);
    CHECK(tg_text_measure("",1,&w,&h)==TG_OK&&w==0);
    CHECK(tg_text_measure("A",0,&w,&h)==TG_ERROR);
    tg_canvas *c=tg_canvas_create(12,12); CHECK(c);
    tg_text_draw(c,(tg_rect){2,0,4,12},1,1,"A",1,0xffffff);
    const uint32_t *p=tg_canvas_pixels(c);
    CHECK(p[1*12+2]==0xffffff&&p[1*12+1]==0&&p[4*12+5]==0xffffff);
    CHECK(p[2*12+1]==0&&p[2*12+5]==0xffffff);
    tg_canvas_clear(c,0);
    tg_text_draw(c,(tg_rect){0,0,12,12},0,0,"\xd0\x96\xd1\x97",1,0xffffff);
    CHECK(p[0]==0xffffff&&p[2]==0xffffff&&p[4]==0xffffff); /* ZHE */
    CHECK(p[7]==0xffffff&&p[9]==0xffffff&&p[8]==0); /* YI diaeresis */
    CHECK(tg_text_measure("\xd2\x90\xd1\x94\xd1\x96\xd1\x97",1,&w,&h)==TG_OK&&w==23&&h==7);
    /* Legacy accented letters are actual glyphs, not question marks or ASCII folds. */
    CHECK(memcmp(tg_fallback_glyph(0x00e9),tg_fallback_glyph('?'),7)!=0);
    CHECK(memcmp(tg_fallback_glyph(0x00e9),tg_fallback_glyph('E'),7)!=0);
    CHECK(memcmp(tg_fallback_glyph(0x00e9),tg_fallback_glyph(0x00c9),7)==0);
    CHECK(memcmp(tg_fallback_glyph(0x0142),tg_fallback_glyph('L'),7)!=0);
    CHECK(memcmp(tg_fallback_glyph(0x017c),tg_fallback_glyph(0x017a),7)!=0);
    tg_canvas_destroy(c); puts("fallback text checks passed"); return 0;
}
