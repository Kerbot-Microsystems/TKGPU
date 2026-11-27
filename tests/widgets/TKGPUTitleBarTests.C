#include "TKGPUTitleBar.H"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    tg_ui *u=tg_ui_create(); CHECK(u);
    tg_id bar=tg_ui_add(u,".bar",TG_FRAME,NULL);
    tg_id title=tg_ui_add(u,".bar.title",TG_LABEL,"Kerbot");
    tg_id button=tg_ui_add(u,".bar.close",TG_BUTTON,"X");
    tg_id entry=tg_ui_add(u,".bar.search",TG_ENTRY,NULL);
    CHECK(bar&&title&&button&&entry);
    CHECK(tg_ui_geometry(u,bar,TG_PLACE,(tg_rect){0,0,300,32})==TG_OK);
    CHECK(tg_ui_geometry(u,title,TG_PLACE,(tg_rect){0,0,80,32})==TG_OK);
    CHECK(tg_ui_geometry(u,button,TG_PLACE,(tg_rect){250,0,50,32})==TG_OK);
    CHECK(tg_ui_geometry(u,entry,TG_PLACE,(tg_rect){100,0,100,32})==TG_OK);
    CHECK(tg_ui_arrange(u,400,200)==TG_OK);
    tg_titlebar b={0}; tg_id ids[]={bar,title};
    CHECK(tg_titlebar_attach(&b,u,ids,2)==TG_OK);
    CHECK(tg_titlebar_hit(&b,u,10,10)); CHECK(tg_titlebar_hit(&b,u,90,10));
    CHECK(!tg_titlebar_hit(&b,u,270,10)); CHECK(!tg_titlebar_hit(&b,u,150,10)); CHECK(!tg_titlebar_hit(&b,u,10,100));
    CHECK(tg_titlebar_attach(&b,u,&button,1)==TG_ERROR);
    CHECK(tg_ui_remove(u,title)==TG_OK); CHECK(tg_ui_arrange(u,400,200)==TG_OK);
    b.enabled=0; CHECK(!tg_titlebar_hit(&b,u,90,10));
    tg_ui_destroy(u); return 0;
}
