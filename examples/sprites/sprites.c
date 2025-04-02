#include "ex_common.h"
#include <vulkan/vulkan_core.h>

int width = 720;
int height = 480;

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
};
const size_t k_quad_vertex_count = sizeof(k_vertices) / sizeof(Vertex);
static uint32_t k_indices[] = {0, 1, 2, 0, 2, 3};

const size_t k_quad_index_count = sizeof(k_indices) / sizeof(uint32_t);

mgfx_sh vsh, fsh;
mgfx_ph gfxph;

mgfx_vbh vbh;
mgfx_ibh ibh;

mgfx_th sprite;
mgfx_dh u_diffuse;

void mgfx_example_init() {
    vsh = mgfx_shader_create("assets/shaders/sprites.vert.glsl.spv");
    fsh = mgfx_shader_create("assets/shaders/sprites.frag.glsl.spv");
    gfxph = mgfx_program_create_graphics(vsh, fsh);

    vbh = mgfx_vertex_buffer_create(k_vertices, sizeof(k_vertices));
    ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));

    u_diffuse = mgfx_descriptor_create("u_diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    mgfx_image_info img_info = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,

        .width = 1,
        .height = 1,

        .layers = 1,

        .cube_map = MX_FALSE,
    };

    sprite = mgfx_texture_create(&img_info, VK_FILTER_NEAREST, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    mgfx_set_texture(u_diffuse, sprite);
}

// TODO: Remove
void mgfx_example_updates(const draw_ctx* ctx) {}

void mgfx_example_update() {
    for (int x = -2; x < 3; x++) {
        mgfx_bind_descriptor(0, u_diffuse);

        mgfx_bind_vertex_buffer(vbh);
        mgfx_bind_index_buffer(ibh);

        mat4 model;
        vec3 position = {x * 2.0f, 0.0f, -5.0f};
        glm_mat4_identity(model);

        glm_translate(model, position);

        mgfx_set_transform(model[0]);

        mat4 proj;
        glm_perspective(glm_rad(60.0), 16.0 / 9.0, 0.1f, 10000.0f, proj);
        proj[1][1] *= -1;
        mgfx_set_proj(proj[0]);

        mat4 view;
        glm_mat4_identity(view);
        mgfx_set_view(view[0]);

        mgfx_submit(0, gfxph);
    }
}

void mgfx_example_shutdown() {
    mgfx_texture_destroy(sprite, MX_TRUE);

    mgfx_buffer_destroy(vbh.idx);
    mgfx_buffer_destroy(ibh.idx);

    mgfx_program_destroy(gfxph);
    mgfx_shader_destroy(vsh);
    mgfx_shader_destroy(fsh);
}

int main() { mgfx_example_app(); }
