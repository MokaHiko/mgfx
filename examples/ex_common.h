#ifndef MGFX_EXAMPLES_COMMON_H
#define MGFX_EXAMPLES_COMMON_H

#include <mgfx/mgfx.h>
#include <cglm/cglm.h>

// TODO: REMOVE WHEN API AGNOSTIC.
#include "../../mgfx/src/renderer_vk.h"

#define APP_WIDTH 1280
#define APP_HEIGHT 720

typedef enum mgfx_camera_type {
    mgfx_camera_type_perspective = 0,
    mgfx_camera_type_orthographic = 1,
} mgfx_camera_type;

typedef struct camera {
    mat4 view;
    mat4 proj;
    mat4 view_proj;

    mat4 inverse_view;

    vec3 position;

    vec3 forward;
    vec3 right;
    vec3 up;

    float yaw;
    float pitch;

    mgfx_camera_type type;
} camera;

void camera_create(mgfx_camera_type type, camera* cam);
void camera_update(camera* cam);

extern camera g_example_camera;
extern texture_vk s_default_white;
extern texture_vk s_default_black;
extern texture_vk s_default_normal_map;

extern double MGFX_TIME;
extern double MGFX_TIME_DELTA_TIME;

int load_shader_from_path(const char *file_path, shader_vk *shader);

void load_texture_2d_from_path(const char* path, VkFormat format, texture_vk* texture);

int mgfx_example_app();

// Input
typedef enum mgfx_input_axis {
    MGFX_INPUT_AXIS_MOUSE_X,
    MGFX_INPUT_AXIS_MOUSE_Y,
} mgfx_input_axis;

MX_API mx_bool mgfx_get_key(int key_code);
MX_API float mgfx_get_axis(mgfx_input_axis axis);

void log_mat4(const mat4 mat);

extern void mgfx_example_init();

extern void mgfx_example_update();

extern void mgfx_example_shutdown();

#endif
