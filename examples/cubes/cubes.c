#include "ex_common.h"

#include <math.h>
#include <mx/mx_log.h>
#include <mx/mx_math.h>
#include <mx/mx_math_mtx.h>

typedef struct my_vertex {
    float position[3];
    float uv_x;
    float normal[3];
    float uv_y;
    float color[4];
} my_vertex;

static my_vertex k_vertices[] = {
    // Front face
    {{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},  // Top-right
    {{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Back face
    {{0.5f, -0.5f, -0.5f},
     0.0f,
     {0.0f, 0.0f, -1.0f},
     0.0f,
     {1.0f, 0.0f, 1.0f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, -0.5f},
     1.0f,
     {0.0f, 0.0f, -1.0f},
     0.0f,
     {0.0f, 1.0f, 1.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}, // Top-right
    {{0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {0.5f, 0.5f, 0.5f, 1.0f}},  // Top-left

    // Left face
    {{-0.5f, -0.5f, -0.5f},
     0.0f,
     {-1.0f, 0.0f, 0.0f},
     0.0f,
     {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, 0.5f},
     1.0f,
     {-1.0f, 0.0f, 0.0f},
     0.0f,
     {0.5f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {0.0f, 0.5f, 1.0f, 1.0f}},  // Top-right
    {{-0.5f, 0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.5f, 1.0f}}, // Top-left

    // Right face
    {{0.5f, -0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.0f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f},
     1.0f,
     {1.0f, 0.0f, 0.0f},
     0.0f,
     {0.5f, 0.5f, 1.0f, 1.0f}},                                                      // Bottom right
    {{0.5f, 0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {0.5f, 0.0f, 1.0f, 1.0f}}, // Top-right
    {{0.5f, 0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // Top-left

    // Top face
    {{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.5f, 1.0f}},  // Bottom-right
    {{0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {0.0f, 1.0f, 1.0f, 1.0f}}, // Top-right
    {{-0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Bottom face
    {{-0.5f, -0.5f, -0.5f},
     0.0f,
     {0.0f, -1.0f, 0.0f},
     0.0f,
     {1.0f, 0.5f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f},
     1.0f,
     {0.0f, -1.0f, 0.0f},
     0.0f,
     {0.5f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {1.0f, 0.0f, 0.5f, 1.0f}},  // Top-right
    {{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {0.5f, 0.5f, 1.0f, 1.0f}}, // Top-left
};
const size_t k_cube_vertex_count = sizeof(k_vertices) / sizeof(my_vertex);

static uint32_t k_indices[] = {
    0,  1,  2,  0,  2,  3,

    4,  5,  6,  4,  6,  7,

    8,  9,  10, 8,  10, 11,

    12, 13, 14, 12, 14, 15,

    16, 17, 18, 16, 18, 19,

    20, 21, 22, 20, 22, 23,
};
const size_t k_cube_index_count = sizeof(k_indices) / sizeof(uint32_t);

mgfx_sh cube_sh, fsh;
mgfx_ph gfxph;

mgfx_vbh vbh;
mgfx_ibh ibh;

void mgfx_example_init() {
    cube_sh = mgfx_shader_create("/Users/christianmarkg.solon/dev/mx_app/mgfx/unlit.vert.spv");
    fsh = mgfx_shader_create("/Users/christianmarkg.solon/dev/mx_app/mgfx/unlit.frag.spv");

    gfxph = mgfx_program_create_graphics(cube_sh, fsh);

    vbh = mgfx_vertex_buffer_create(k_vertices, sizeof(k_vertices));
    ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));
}

void mgfx_example_update() {
    // Profiler
    static float last_value = 0.0f;
    float cur_value = MGFX_TIME_DELTA_TIME;
    float epsilon = 0.005f;

    if (fabsf(cur_value - last_value) > epsilon) {
        last_value = cur_value;
    }
    mgfx_debug_draw_text(0, APP_HEIGHT * 0.95f, "dt: %.2f ms", last_value * 1000.0f);

    for (int x = -2; x < 3; x++) {
        mgfx_bind_vertex_buffer(vbh);
        mgfx_bind_index_buffer(ibh);

        mx_mat4 rot = mx_mat4_rotate_euler(MGFX_TIME, (mx_vec3){MGFX_TIME, MGFX_TIME, MGFX_TIME});
        mx_mat4 translate = mx_translate((mx_vec3){x * 2.0f, 0.0f, 0.0f});
        mgfx_set_transform(mx_mat4_mul(translate, rot).val);

        mx_mat4 proj = mx_perspective(MX_DEG_TO_RAD(60.0), 16.0 / 9.0, 0.1, 1000.0f);
        mgfx_set_proj(proj.val);

        mx_mat4 view = mx_look_at((mx_vec3){.z = 5.0f}, (mx_vec3){0}, MX_VEC3_UP);
        mgfx_set_view(view.val);

        mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, gfxph);
    }
}

void mgfx_example_shutdown() {
    mgfx_buffer_destroy(vbh.idx);
    mgfx_buffer_destroy(ibh.idx);

    mgfx_program_destroy(gfxph);
    mgfx_shader_destroy(cube_sh);
    mgfx_shader_destroy(fsh);
}

int main() { mgfx_example_app(); }
