#include "ex_common.h"

typedef struct Vertex {
    float position[3];
    float uv_x;
    float normal[3];
    float uv_y;
    float color[4];
} Vertex;

static Vertex k_vertices[] = {
    // Front face
    {{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    {{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},   // Top-right
    {{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}},  // Top-left

    // Back face
    {{0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {1.0f, 0.0f, 1.0f, 1.0f}},  // Bottom-left
    {{-0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {0.0f, 1.0f, 1.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // Top-right
    {{0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {0.5f, 0.5f, 0.5f, 1.0f}},   // Top-left

    // Left face
    {{-0.5f, -0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    {{-0.5f, 0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {0.0f, 0.5f, 1.0f, 1.0f}},   // Top-right
    {{-0.5f, 0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.5f, 1.0f}},  // Top-left

    // Right face
    {{0.5f, -0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.0f, 1.0f}},  // Bottom-left
    {{0.5f, -0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {0.5f, 0.5f, 1.0f, 1.0f}}, // Bottom right
    {{0.5f, 0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {0.5f, 0.0f, 1.0f, 1.0f}},  // Top-right
    {{0.5f, 0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},   // Top-left

    // Top face
    {{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}},  // Bottom-left
    {{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.5f, 1.0f}},   // Bottom-right
    {{0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {0.0f, 1.0f, 1.0f, 1.0f}},  // Top-right
    {{-0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Bottom face
    {{-0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    {{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {1.0f, 0.0f, 0.5f, 1.0f}},   // Top-right
    {{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {0.5f, 0.5f, 1.0f, 1.0f}},  // Top-left
};
const size_t k_cube_vertex_count = sizeof(k_vertices) / sizeof(Vertex);

static uint32_t k_indices[] = {
    // Front face
    0, 1, 2, 0, 2, 3,
    // Back face
    4, 5, 6, 4, 6, 7,
    // Left face
    8, 9, 10, 8, 10, 11,
    // Right face
    12, 13, 14, 12, 14, 15,
    // Top face
    16, 17, 18, 16, 18, 19,
    // Bottom face
    20, 21, 22, 20, 22, 23};
const size_t k_cube_index_count = sizeof(k_indices) / sizeof(uint32_t);

mgfx_sh quad_vsh, fsh;
mgfx_ph gfxph;

mgfx_vbh vbh;
mgfx_ibh ibh;

void mgfx_example_init() {
    quad_vsh = mgfx_shader_create("assets/shaders/unlit.vert.glsl.spv");
    fsh = mgfx_shader_create("assets/shaders/unlit.frag.glsl.spv");
    gfxph = mgfx_program_create_graphics(quad_vsh, fsh);

    vbh = mgfx_vertex_buffer_create(k_vertices, sizeof(k_vertices));
    ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));
}

void mgfx_example_update() {
    for (int x = -2; x < 3; x++) {
        mgfx_bind_vertex_buffer(vbh);
        mgfx_bind_index_buffer(ibh);

        mat4 model;
        glm_mat4_identity(model);

        vec3 position = {x * 2.0f, 0.0f, -5.0f};

        glm_translate(model, position);

        vec3 axis = {1.0, 1.0, 1.0};
        glm_rotate(model, MGFX_TIME * 3.141593, axis);

        mgfx_set_transform(model[0]);

        mat4 proj;
        glm_perspective(glm_rad(60.0), 16.0 / 9.0, 0.1f, 10000.0f, proj);
        proj[1][1] *= -1;
        mgfx_set_proj(proj[0]);

        mat4 view;
        glm_mat4_identity(view);
        mgfx_set_view(view[0]);

        mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, gfxph);
    }

    // Profiler
    static float last_value = 0.0f;
    float cur_value = MGFX_TIME_DELTA_TIME;
    float epsilon = 0.01f;

    if (fabsf(cur_value - last_value) > epsilon) {
        last_value = cur_value;
    }

    mgfx_debug_draw_text(0, 0, "delta time: %.2f s", last_value * 1000.0f);

    //mgfx_gizmo_draw_cube(g_example_camera.view[0], g_example_camera.proj[0], (mx_vec3){0.0f, 0.0f, 0.0f} , NULL, NULL);
}

void mgfx_example_shutdown() {
    mgfx_buffer_destroy(vbh.idx);
    mgfx_buffer_destroy(ibh.idx);

    mgfx_program_destroy(gfxph);
    mgfx_shader_destroy(quad_vsh);
    mgfx_shader_destroy(fsh);
}

int main() { mgfx_example_app(); }
