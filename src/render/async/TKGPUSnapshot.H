#ifndef TKGPU_SNAPSHOT_H
#define TKGPU_SNAPSHOT_H
#include "TKGPUUi.H"
/* Owner-only capture of render state. No input events are copied. */
tg_ui *tg_snapshot_create(const tg_ui *u);
#endif
