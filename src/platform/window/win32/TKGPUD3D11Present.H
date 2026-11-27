#ifndef TKGPU_D3D11_PRESENT_H
#define TKGPU_D3D11_PRESENT_H
#include "../../../compat/tk/TKGPUWindow.H"
#include <windows.h>
/* 1: hardware frame presented, 0: not ready, -1: device/backend failure. */
int tg_d3d11_present(tg_window *,HWND,int,int);
int tg_d3d11_raster(tg_window *,HWND,int,int);
void tg_d3d11_release(tg_window *);
#endif
