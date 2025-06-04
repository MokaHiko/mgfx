#ifndef MX_RENDERER_H_
#define MX_RENDERER_H_

// TODO: Remove
#include <ex_common.h>

#include <mgfx/mgfx.h>
#include <mx/mx_math_types.h>

typedef enum mesh_flags {
    MGFX_MESH_FLAG_RECIEVE_NONE = 0,
    MGFX_MESH_FLAG_RECIEVE_SHADOWS = 1 << 0,
    MGFX_MESH_FLAG_CAST_SHADOWS = 1 << 1,
    MGFX_MESH_FLAG_STATIC = 1 << 2,
} mesh_flags;

enum frame_targets {
    FORWARD_FRAME_TARGET,
    SHADOW_FRAME_TARGET,
    MESH_FRAME_TARGET,
    BLUR_0_FRAME_TARGET, // Horizontal
    BLUR_1_FRAME_TARGET, // Vertical and out
};

typedef struct directional_light {
    struct {
        float light_space_matrix[16];
        mx_vec3 direction;
        float distance;
        mx_vec4 intensity;
    } data;

    camera camera;
} directional_light;

typedef struct point_light {
    float position[4];

    mx_vec4 intensity;
} point_light;

enum { SCENE_MAX_POINT_LIGHTS = 32 };
typedef struct scene_data {
    mx_vec3 camera_position;
    uint32_t point_light_count;
} scene_data;

void renderer_init();
void renderer_draw();
void renderer_shutdown();

#endif
