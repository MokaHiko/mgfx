#ifndef MXGUI_H_
#define MXGUI_H_

#include <mx/mx.h>

typedef MX_API struct mxgui_init_info {
  uint32_t width, height;
} mxgui_init_info;

MX_API void mxgui_init(mxgui_init_info setings);

MX_API void mxgui_render();

void mxgui_shutdown();

#endif
