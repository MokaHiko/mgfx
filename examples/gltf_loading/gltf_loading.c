#include "gltf_loader.h"

#include <ex_common.h>

#include <GLFW/glfw3.h>

typedef struct Vertex {
    float position[3];
    float uv_x;
    float normal[3];
    float uv_y;
    float color[4];
} Vertex;

static const Vertex k_quad_vertices[] = {
    {{-1.0f, -1.0f, 1.0f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{1.0f, -1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-1.0f, 1.0f, 1.0f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}},
};
static const size_t k_quad_vertex_count = sizeof(k_quad_vertices) / sizeof(Vertex);

static const uint32_t k_indices[] = {3, 2, 0, 2, 1, 0};
static const size_t k_quad_index_count = sizeof(k_indices) / sizeof(uint32_t);

mgfx_imgh color_fba;
mgfx_imgh depth_fba;
mgfx_fbh fbh;

mgfx_sh fp_vs;
mgfx_sh fp_fs;
mgfx_ph fp_program;

mgfx_sh quad_vsh, quad_fsh;
mgfx_ph blit_program;

mgfx_vbh quad_vbh;
mgfx_ibh quad_ibh;

mgfx_th color_fba_texture;
mgfx_dh u_color_fba;

mgfx_scene gltf;

void mgfx_example_init() {
    struct mgfx_image_info color_attachment_info = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    color_fba = mgfx_image_create(&color_attachment_info,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT);

    struct mgfx_image_info depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    depth_fba = mgfx_image_create(&depth_attachment_info, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    fbh = mgfx_framebuffer_create(&color_fba, 1, depth_fba);
    mgfx_set_view_target(0, fbh);

    fp_vs = mgfx_shader_create("assets/shaders/lit.vert.glsl.spv");
    fp_fs = mgfx_shader_create("assets/shaders/lit.frag.glsl.spv");
    fp_program = mgfx_program_create_graphics(fp_vs, fp_fs);

    mgfx_vertex_layout vl;

    vertex_layout_begin(&vl);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TANGENT, sizeof(float) * 4);
    vertex_layout_end(&vl);

    gltf.vl = &vl;

    LOAD_GLTF_MODEL("DamagedHelmet", gltf_loader_flag_default, &gltf);

    quad_vsh = mgfx_shader_create("assets/shaders/blit.vert.glsl.spv");
    quad_fsh = mgfx_shader_create("assets/shaders/sprites.frag.glsl.spv");
    blit_program = mgfx_program_create_graphics(quad_vsh, quad_fsh);

    quad_vbh = mgfx_vertex_buffer_create(k_quad_vertices, sizeof(k_quad_vertices));
    quad_ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));

    u_color_fba = mgfx_descriptor_create("u_diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    color_fba_texture = mgfx_texture_create_from_image(color_fba, VK_FILTER_NEAREST);
    mgfx_set_texture(u_color_fba, color_fba_texture);
}

void mgfx_example_update() {
    for (uint32_t n = 0; n < gltf.node_count; n++) {
        if (gltf.nodes[n].mesh == NULL) {
            continue;
        }

        assert(gltf.nodes[n].mesh->primitive_count > 0);

        mat4 model = GLM_MAT4_IDENTITY;
        glm_mat4_mul(gltf.nodes[n].matrix, model, model);

        vec3 position = {0.0f, 0.0f, 0.0f};
        glm_translate(model, position);

        static vec3 up = {0.0, 1.0, 0.0};
        static float y_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_RIGHT)) {
            y_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if (mgfx_get_key(GLFW_KEY_LEFT)) {
            y_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(model, y_rotation, up);

        static vec3 right = {1.0, 0.0, 0.0};
        static float z_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_UP)) {
            z_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if (mgfx_get_key(GLFW_KEY_DOWN)) {
            z_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(model, z_rotation, right);

        vec3 uniform_scale = {1.0, 1.0, 1.0};
        glm_vec3_scale(uniform_scale, 1.0, uniform_scale);
        glm_scale(model, uniform_scale);

        for (uint32_t p = 0; p < gltf.nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &gltf.nodes[n].mesh->primitives[p];

            if (node_primitive->index_count <= 0) {
                continue;
            }

            mgfx_set_proj(g_example_camera.proj[0]);
            mgfx_set_view(g_example_camera.view[0]);
            mgfx_set_transform(model[0]);

            mgfx_bind_vertex_buffer(node_primitive->vbh);
            mgfx_bind_index_buffer(node_primitive->ibh);

            mgfx_bind_descriptor(0, node_primitive->material->u_properties_buffer);
            mgfx_bind_descriptor(0, node_primitive->material->u_albedo_texture);
            mgfx_bind_descriptor(0, node_primitive->material->u_metallic_roughness_texture);
            mgfx_bind_descriptor(0, node_primitive->material->u_normal_texture);
            mgfx_bind_descriptor(0, node_primitive->material->u_occlusion_texture);
            mgfx_bind_descriptor(0, node_primitive->material->u_emissive_texture);

            mgfx_submit(0, fp_program);
        }
    }

    // Blit to backbuffer
    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);
    mgfx_bind_descriptor(0, u_color_fba);

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, blit_program);
}

void mgfx_example_shutdown() {
    mgfx_image_destroy(depth_fba);

    mgfx_texture_destroy(color_fba_texture, MX_FALSE);
    mgfx_image_destroy(color_fba);

    scene_destroy(&gltf);

    mgfx_program_destroy(fp_program);
    mgfx_shader_destroy(fp_fs);
    mgfx_shader_destroy(fp_vs);

    mgfx_buffer_destroy(quad_vbh.idx);
    mgfx_buffer_destroy(quad_ibh.idx);

    mgfx_program_destroy(blit_program);
    mgfx_shader_destroy(quad_vsh);
    mgfx_shader_destroy(quad_fsh);

    mgfx_framebuffer_destroy(fbh);
}

int main(int argc, char** argv) { mgfx_example_app(); }
