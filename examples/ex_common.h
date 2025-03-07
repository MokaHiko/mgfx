#ifndef MGFX_EXAMPLES_COMMON_H
#define MGFX_EXAMPLES_COMMON_H

#include <mgfx/mgfx.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/cglm.h>

#define APP_WIDTH 1280
#define APP_HEIGHT 720

typedef struct camera {
    vec3 position;

    vec3 forward;
    vec3 right;
    vec3 up;

    mat4 view;
    mat4 proj;
    mat4 view_proj;

    mat4 inverse_view;
} camera;

extern camera g_example_camera;

extern double MGFX_TIME;
extern double MGFX_TIME_DELTA_TIME;

// TODO: REMOVE WHEN API AGNOSTIC.
#include "../../mgfx/src/renderer_vk.h"
int load_shader_from_path(const char *file_path, shader_vk *shader);

void load_image_2d_from_path(const char* path, texture_vk* texture);

int mgfx_example_app();

MX_API mx_bool mgfx_get_key(int key_code);

extern void mgfx_example_init();
extern void mgfx_example_update();
extern void mgfx_example_shutdown();

#endif
