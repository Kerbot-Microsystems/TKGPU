#ifndef TKGPU_BASE_DIALOG_H
#define TKGPU_BASE_DIALOG_H
#include <stdint.h>
/* Inline value object: no Python references and no additional allocation. */
typedef struct {
    int enabled,visible,minimized,dragging;
    int width,height,restore_height,title_height,minimum_y;
    int mouse_x,mouse_y,start_x,start_y;
    int resize_x,resize_y,min_width,min_height,max_width,max_height;
    int resize_edges,start_w,start_h;
    uint32_t title,shadow;
} tg_base_dialog;
#endif
