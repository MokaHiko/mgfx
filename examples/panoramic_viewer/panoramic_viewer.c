#include "ex_common.h"

#include <GLFW/glfw3.h>

#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_log.h>
#include <mx/mx_math.h>
#include <mx/mx_math_mtx.h>
#include <mx/mx_memory.h>

#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include <third_party/jsmn/jsmn.h>

#include <string.h>

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

struct panoramic_data {
    float a;
} panoramic_data;

mgfx_sh panoramic_vsh, panoramic_fsh;
mgfx_ph panoramic_ph;

mgfx_vbh vbh;
mgfx_ibh ibh;

mgfx_ubh panoramic_data_buffer;
mgfx_dh u_panoramic_data;

mgfx_th start_tex;
mgfx_dh u_start_tex;

mgfx_th end_tex;
mgfx_dh u_end_tex;

void mgfx_example_init() {
    panoramic_vsh = mgfx_shader_create(MGFX_ASSET_PATH "shaders/panoramic_viewer.vert.glsl.spv");
    panoramic_fsh = mgfx_shader_create(MGFX_ASSET_PATH "shaders/panoramic_viewer.frag.glsl.spv");
    panoramic_ph = mgfx_program_create_graphics(panoramic_vsh, panoramic_fsh);

    vbh = mgfx_vertex_buffer_create(k_vertices, sizeof(k_vertices));
    ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));

    panoramic_data_buffer = mgfx_uniform_buffer_create(&panoramic_data, sizeof(panoramic_data));
    u_panoramic_data = mgfx_descriptor_create("panoramic_data", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_panoramic_data, panoramic_data_buffer);

    start_tex =
        load_texture_2d_from_path(MGFX_ASSET_PATH "textures/5.jpg", VK_FORMAT_R8G8B8A8_SRGB);
    u_start_tex = mgfx_descriptor_create("start_tex", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    mgfx_set_texture(u_start_tex, start_tex);

    end_tex = load_texture_2d_from_path(MGFX_ASSET_PATH "textures/4.jpg", VK_FORMAT_R8G8B8A8_SRGB);
    u_end_tex = mgfx_descriptor_create("end_tex", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    mgfx_set_texture(u_end_tex, end_tex);

    // Extract features
    char json[MX_MB] = {0};
    const char* feature_path =
        "/Users/christianmarkg.solon/Dev/mx_app/mgfx/examples/panoramic_viewer/54.json";
    size_t file_size = 0;

    int file_read_ret = mx_read_file(feature_path, &file_size, NULL);
    MX_ASSERT(file_read_ret == MX_SUCCESS, "Failed to feature features json!");
    MX_ASSERT(file_size < MX_MB, "Feature json larger than buffer %d");
    mx_read_file(feature_path, &file_size, json);
    json[file_size] = '\0';

    jsmn_parser p;
    jsmntok_t t[MX_KB] = {0};
    jsmn_init(&p);

    int r = jsmn_parse(&p, json, file_size, t, MX_KB);
    MX_ASSERT(r >= 0, "Failed to prase feature json %d", r);
    MX_ASSERT(r <= MX_KB, "Feature json larger than buffer");

    for (int i = 1; i < r;) {
        if (t[i].type == JSMN_STRING && t[i + 1].type == JSMN_PRIMITIVE) {
            float x1 = strtof(&json[t[i + 1].start], NULL);
            float y1 = strtof(&json[t[i + 3].start], NULL);

            float x2 = strtof(&json[t[i + 5].start], NULL);
            float y2 = strtof(&json[t[i + 7].start], NULL);

            MX_LOG_INFO("x1: %.2f y1: %.2f | x2: %.2f y2: %.2f", x1, y1, x2, y2);
            i += 8;
            continue;
        }

        i++;
    };

    exit(0);
}

void mgfx_example_update() {
    // Profiler
    static float last_value = 0.0f;
    float cur_value = MGFX_TIME_DELTA_TIME;
    float epsilon = 0.005f;

    if (fabsf(cur_value - last_value) > epsilon) {
        last_value = cur_value;
    }

    if (mgfx_get_key(GLFW_KEY_W)) {
        panoramic_data.a += MGFX_TIME_DELTA_TIME * 1.0f;
    }

    if (mgfx_get_key(GLFW_KEY_S)) {
        panoramic_data.a -= MGFX_TIME_DELTA_TIME * 1.0f;
    }
    panoramic_data.a = mx_clamp(panoramic_data.a, 0.0f, 1.0f);

    mgfx_buffer_update(panoramic_data_buffer.idx, &panoramic_data, sizeof(panoramic_data), 0);

    mgfx_debug_draw_text(0, APP_HEIGHT * 0.95f, "dt: %.2f ms", last_value * 1000.0f);
    mgfx_debug_draw_text(0, APP_HEIGHT * 0.90f, "a: %.2f", panoramic_data.a);

    mgfx_bind_vertex_buffer(vbh);
    mgfx_bind_index_buffer(ibh);

    mgfx_bind_descriptor(0, u_panoramic_data);
    mgfx_bind_descriptor(0, u_start_tex);
    mgfx_bind_descriptor(0, u_end_tex);

    mx_mat4 rot = mx_mat4_rotate_euler(MX_DEG_TO_RAD(180), (mx_vec3){0.0, 0.0f, 1.0f});
    mx_mat4 translate = mx_translate((mx_vec3){0.0f, 0.0f, 0.0f});
    mgfx_set_transform(mx_mat4_mul(translate, rot).val);

    /*mx_mat4 view = mx_look_at((mx_vec3){.z = 2.0f}, (mx_vec3){0}, MX_VEC3_UP);*/
    /*mgfx_set_view(view.val);*/
    /*mx_mat4 proj = mx_perspective(MX_DEG_TO_RAD(60.0), 16.0 / 9.0, 0.1, 1000.0f);*/
    /*mgfx_set_proj(proj.val);*/
    mgfx_set_view(g_example_camera.view.val);
    mgfx_set_proj(g_example_camera.proj.val);

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, panoramic_ph);
}

void mgfx_example_shutdown() {
    mgfx_texture_destroy(start_tex, MX_TRUE);
    mgfx_descriptor_destroy(u_start_tex);

    mgfx_texture_destroy(end_tex, MX_TRUE);
    mgfx_descriptor_destroy(u_end_tex);

    mgfx_buffer_destroy(panoramic_data_buffer.idx);
    mgfx_descriptor_destroy(u_panoramic_data);

    mgfx_buffer_destroy(vbh.idx);
    mgfx_buffer_destroy(ibh.idx);

    mgfx_program_destroy(panoramic_ph);
    mgfx_shader_destroy(panoramic_vsh);
    mgfx_shader_destroy(panoramic_fsh);
}

int main() { mgfx_example_app(); }
