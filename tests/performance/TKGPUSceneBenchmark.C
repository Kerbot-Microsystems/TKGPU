#define _POSIX_C_SOURCE 200809L
#include "TKGPUUi.H"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0+t.tv_nsec/1e6; }
int main(void) {
 tg_ui *u=tg_ui_create(); tg_canvas *c=tg_canvas_create(1280,720);
 for(int i=0;i<900;i++){char path[32]; snprintf(path,sizeof path,".w%d",i); tg_id id=tg_ui_add(u,path,TG_LABEL,"ABCD"); if(!id||tg_ui_geometry(u,id,TG_PLACE,(tg_rect){(i%30)*42,(i/30)*24,40,22})) return 1;}
 tg_ui_arrange(u,1280,720); double times[5];
 for(int i=0;i<5;i++){double t=now(); for(int k=0;k<25;k++) if(tg_ui_paint_parallel(u,c,4,NULL)) return 2; times[i]=(now()-t)/25;}
 for(int i=0;i<5;i++) for(int j=i+1;j<5;j++) if(times[i]>times[j]){double t=times[i];times[i]=times[j];times[j]=t;}
 unsigned long long hash=0; const uint32_t *p=tg_canvas_pixels(c); for(int i=0;i<1280*720;i++)hash=hash*33+p[i]; printf("median %.3f ms hash %llu\n",times[2],hash);tg_canvas_destroy(c);tg_ui_destroy(u);return 0;
}
