#ifndef MGFX_EXAMPLES_COMMON_H
#define MGFX_EXAMPLES_COMMON_H

#include <mgfx/mgfx.h>

#define APP_WIDTH 1080
#define APP_HEIGHT 720

// TODO: REMOVE WHEN API AGNOSTIC.
#include "../../mgfx/src/renderer_vk.h"
int load_shader_from_path(const char* file_path, shader_vk* shader);

int mgfx_example_app();

extern void mgfx_example_init();
extern void mgfx_example_update();
extern void mgfx_example_shutdown();

#endif
