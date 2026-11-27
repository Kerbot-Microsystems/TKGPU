#ifndef TKGPU_TITLEBAR_H
#define TKGPU_TITLEBAR_H
#include "TKGPUUi.H"
enum { TG_TITLEBAR_SOURCE_LIMIT=64 };
typedef struct { tg_id sources[TG_TITLEBAR_SOURCE_LIMIT]; unsigned count; int enabled; } tg_titlebar;
int tg_titlebar_attach(tg_titlebar *,const tg_ui *,const tg_id *,size_t);
int tg_titlebar_hit(const tg_titlebar *,const tg_ui *,int,int);
#endif
