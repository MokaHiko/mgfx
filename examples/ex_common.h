#ifndef MGFX_EXAMPLES_COMMON_H
#define MGFX_EXAMPLES_COMMON_H

#include <mgfx/mgfx.h>

// TODO: REMOVE WHEN API AGNOSTIC.
#include "../../mgfx/src/renderer_vk.h"
int load_shader_from_path(const char* file_path, ShaderVk* shader);

int mgfx_example_app();

extern void mgfx_example_init();
extern void mgfx_example_update();
extern void mgfx_example_shutdown();

#endif
