#ifndef MGFX_EXAMPLES_COMMON_H_
#define MGFX_EXAMPLES_COMMON_H_

#include <mgfx/mgfx.h>
#include <mx/mx_math_types.h>
#include <mx/mx_math_mtx.h>

#define APP_WIDTH  2560
#define APP_HEIGHT 1440

typedef enum mgfx_camera_type {
    mgfx_camera_type_perspective = 0,
    mgfx_camera_type_orthographic = 1,
} mgfx_camera_type;

typedef struct camera {
    mx_mat4 view;
    mx_mat4 proj;
    mx_mat4 view_proj;

    mx_mat4 inverse_view;

    mx_vec3 position;

    mx_vec3 forward;
    mx_vec3 right;
    mx_vec3 up;

    float yaw;
    float pitch;

    mgfx_camera_type type;
} camera;

void camera_create(mgfx_camera_type type, camera* cam);
void camera_update(camera* cam);

extern camera g_example_camera;
extern mgfx_th s_default_white;
extern mgfx_th s_default_black;
extern mgfx_th s_default_normal_map;

extern double MGFX_TIME;
extern double MGFX_TIME_DELTA_TIME;

mgfx_th load_texture_2d_from_path(const char* path, mgfx_format format);

int mgfx_example_app();

// Input
typedef enum mgfx_input_axis {
    MGFX_INPUT_AXIS_MOUSE_X,
    MGFX_INPUT_AXIS_MOUSE_Y,
} mgfx_input_axis;

MX_API mx_bool mgfx_get_key(int key_code);
MX_API float mgfx_get_axis(mgfx_input_axis axis);

extern void mgfx_example_init();
extern void mgfx_example_update();
extern void mgfx_example_shutdown();

// TODO: Candidate functions
MX_API void mgfx_gizmo_draw_cube(const float* view,
                                 const float* proj,
                                 const mx_vec3 position,
                                 const mx_quat rotation,
                                 const mx_vec3 scale);

#endif
