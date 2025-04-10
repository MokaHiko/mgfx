#include <ex_common.h>
#include <gltf_loading/gltf_loader.h>

#include <ex_common.h>

#include <GLFW/glfw3.h>
#include <mx/mx_file.h>
#include <mx/mx_log.h>
#include <mx/mx_memory.h>

#include <string.h>

typedef struct vertex {
    float position[3];
    float uv_x;
    float normal[3];
    float uv_y;
    float color[4];
} vertex;

static vertex k_vertices[] = {
    {{-1.0f, -1.0f, 1.0f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}}, // Bottom-left
    {{1.0f, -1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    {{1.0f, 1.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},   // Top-right
    {{-1.0f, 1.0f, 1.0f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}},  // Top-left
};
const size_t k_quad_vertex_count = sizeof(k_vertices) / sizeof(vertex);

static uint32_t k_indices[] = {3, 2, 0, 2, 1, 0};
const size_t k_quad_index_count = sizeof(k_indices) / sizeof(uint32_t);

typedef struct directional_light {
    // TODO: make 4th number distance for dir light
    float direction[4];
    float color[4];

    float light_space_matrix[16];
} directional_light;

typedef struct point_light {
    float position[4];

    float color[3];
    float intensity;
} point_light;

directional_light dir_light = {.color = {1.0f, 1.0f, 1.0f, 1.0f}, .direction = {1.0f, -1.0f, 0.0f, 1.0f}};
mgfx_ubh dir_lights_buffer;
mgfx_dh u_dir_light;

//mgfx_ubh point_lights_buffer;

mgfx_imgh shadow_pass_depth_attachment;
mgfx_fbh shadow_pass_fbh;

mgfx_sh shadow_pass_vs;
mgfx_ph shadow_pass_program;

mgfx_th shadow_fba_texture;
mgfx_dh u_shadow_map;

mgfx_imgh mesh_pass_color_attachment;
mgfx_imgh mesh_pass_depth_attachment;
mgfx_fbh mesh_pass_fbh;

mgfx_sh mesh_pass_vs;
mgfx_sh mesh_pass_fs;
mgfx_ph mesh_pass_program;

mgfx_sh quad_vsh, quad_fsh;
mgfx_ph blit_program;

mgfx_vbh quad_vbh;
mgfx_ibh quad_ibh;

mgfx_th color_fba_texture;
mgfx_dh u_color_fba;

mgfx_scene gltf;
mgfx_scene cube;
void mgfx_example_init() {
    // Common
    dir_lights_buffer = mgfx_uniform_buffer_create(&dir_light, sizeof(directional_light));
    u_dir_light = mgfx_descriptor_create("u_dir_light", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_dir_light, dir_lights_buffer);

    // Directional light shadow pass
    struct mgfx_image_info shadow_pass_depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    shadow_pass_depth_attachment = mgfx_image_create(&shadow_pass_depth_attachment_info,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    shadow_pass_fbh = mgfx_framebuffer_create(NULL, 0, shadow_pass_depth_attachment);
    mgfx_set_view_target(0, shadow_pass_fbh);

    shadow_pass_vs = mgfx_shader_create("assets/shaders/shadows.vert.glsl.spv");
    shadow_pass_program = mgfx_program_create_graphics(shadow_pass_vs, (mgfx_sh){.idx = 0});

    // Mesh pass
    struct mgfx_image_info color_attachment_info = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mesh_pass_color_attachment = mgfx_image_create(
        &color_attachment_info,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    struct mgfx_image_info depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mesh_pass_depth_attachment = mgfx_image_create(&depth_attachment_info, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    mesh_pass_fbh = mgfx_framebuffer_create(&mesh_pass_color_attachment, 1, mesh_pass_depth_attachment);
    mgfx_set_view_target(1, mesh_pass_fbh);

    mesh_pass_vs = mgfx_shader_create("assets/shaders/shadows_lit.vert.glsl.spv");
    mesh_pass_fs = mgfx_shader_create("assets/shaders/shadows_lit.frag.glsl.spv");
    mesh_pass_program = mgfx_program_create_graphics(mesh_pass_vs, mesh_pass_fs);

    u_shadow_map = mgfx_descriptor_create("u_shadow_map", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    shadow_fba_texture = mgfx_texture_create_from_image(shadow_pass_depth_attachment, VK_FILTER_LINEAR);
    mgfx_set_texture(u_shadow_map, shadow_fba_texture);

    mgfx_vertex_layout vl;

    vertex_layout_begin(&vl);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
    vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TANGENT, sizeof(float) * 4);
    vertex_layout_end(&vl);

    gltf.vl = &vl;

    cube.vl = &vl;
    load_scene_from_path("/Users/christianmarkg.solon/Dev/mx_app/mgfx/examples/assets/models/cube.gltf", gltf_loader_flag_default, &cube);
    LOAD_GLTF_MODEL("Sponza", gltf_loader_flag_default, &gltf);

    // Blit pass
    quad_vsh = mgfx_shader_create("assets/shaders/blit.vert.glsl.spv");
    quad_fsh = mgfx_shader_create("assets/shaders/sprites.frag.glsl.spv");
    blit_program = mgfx_program_create_graphics(quad_vsh, quad_fsh);

    quad_vbh = mgfx_vertex_buffer_create(k_vertices, sizeof(k_vertices));
    quad_ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));

    u_color_fba = mgfx_descriptor_create("u_diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    color_fba_texture = mgfx_texture_create_from_image(mesh_pass_color_attachment, VK_FILTER_LINEAR);
    mgfx_set_texture(u_color_fba, color_fba_texture);
}

void mgfx_example_update() {
    static float angle = 0;

    if (mgfx_get_key(GLFW_KEY_E)) {
        angle += 3.1415925f * MGFX_TIME_DELTA_TIME;
    }

    if (mgfx_get_key(GLFW_KEY_R)) {
        angle -= 3.1415925f * MGFX_TIME_DELTA_TIME;
    }

    // Apply accumulated rotation
    mat4 rotate_matrix = GLM_MAT4_IDENTITY;
    glm_rotate(rotate_matrix, angle, (vec3){0, 0, 1});

    // Transform the light direction
    glm_mat4_mulv3(rotate_matrix, (vec3){1, 0, 0}, 1.0f, dir_light.direction);
    mgfx_buffer_update(dir_lights_buffer.idx, &dir_light, sizeof(dir_light), 0);

    // Draw shadow pass
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

            float near_plane = 0.1f, far_plane = 50.0f;

            mat4 light_projection;
            glm_ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane, light_projection);
            light_projection[1][1] *= -1;

            mat4 light_view;

            vec3 neg_dir;
            glm_vec3_negate_to(dir_light.direction, neg_dir);
            glm_vec3_scale(neg_dir, 10.0f, neg_dir);

            vec3 light_pos; 
            glm_vec3_sub(GLM_VEC3_ZERO, neg_dir, light_pos);

            glm_lookat(light_pos, GLM_VEC3_ZERO, (vec3){0.0f, 1.0f, 0.0f}, light_view);
            //glm_lookat((vec3){-20.0f, 20.0f, -10.0f}, dir_light.direction, (vec3){0.0f, 1.0f, 0.0f}, light_view);

            mat4 light_space_matrix;
            glm_mat4_mul(light_projection, light_view, light_space_matrix);
            memcpy(dir_light.light_space_matrix, light_space_matrix, sizeof(float) * 16);

            mgfx_set_proj(light_projection[0]);
            mgfx_set_view(light_view[0]);

            mgfx_set_transform(model[0]);

            mgfx_bind_vertex_buffer(node_primitive->vbh);
            mgfx_bind_index_buffer(node_primitive->ibh);

            mgfx_submit(0, shadow_pass_program);
        }
    }

    // Draw mesh pass
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
            y_rotation += MGFX_TIME_DELTA_TIME * 3.141593 * 0.01f;
        } else if (mgfx_get_key(GLFW_KEY_LEFT)) {
            y_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 * 0.01f;
        }
        glm_rotate(model, y_rotation, up);

        static vec3 right = {1.0, 0.0, 0.0};
        static float z_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_UP)) {
            z_rotation += MGFX_TIME_DELTA_TIME * 3.141593 * 0.01f;
        } else if (mgfx_get_key(GLFW_KEY_DOWN)) {
            z_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 * 0.01f;
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

            mgfx_bind_descriptor(0, u_dir_light);
            mgfx_bind_descriptor(0, u_shadow_map);

            mgfx_bind_descriptor(1, node_primitive->material->u_properties_buffer);
            mgfx_bind_descriptor(1, node_primitive->material->u_albedo_texture);
            mgfx_bind_descriptor(1, node_primitive->material->u_metallic_roughness_texture);
            mgfx_bind_descriptor(1, node_primitive->material->u_normal_texture);
            mgfx_bind_descriptor(1, node_primitive->material->u_occlusion_texture);
            mgfx_bind_descriptor(1, node_primitive->material->u_emissive_texture);

            mgfx_submit(1, mesh_pass_program);
        }
    }

    // Blit to backbuffer
    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);
    mgfx_bind_descriptor(0, u_color_fba);

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, blit_program);
}

void mgfx_example_shutdown() {
    scene_destroy(&cube);
    scene_destroy(&gltf);

    // Blit pass resources
    mgfx_texture_destroy(color_fba_texture, MX_FALSE);
    mgfx_descriptor_destroy(u_color_fba);

    mgfx_buffer_destroy(quad_vbh.idx);
    mgfx_buffer_destroy(quad_ibh.idx);

    mgfx_program_destroy(blit_program);
    mgfx_shader_destroy(quad_vsh);
    mgfx_shader_destroy(quad_fsh);

    // Mesh pass resources 
    mgfx_program_destroy(mesh_pass_program);
    mgfx_shader_destroy(mesh_pass_fs);
    mgfx_shader_destroy(mesh_pass_vs);

    mgfx_image_destroy(mesh_pass_depth_attachment);
    mgfx_image_destroy(mesh_pass_color_attachment);
    mgfx_framebuffer_destroy(mesh_pass_fbh);

    // Shadow map resources
    mgfx_texture_destroy(shadow_fba_texture, MX_FALSE);

    mgfx_shader_destroy(shadow_pass_vs);
    mgfx_program_destroy(shadow_pass_program);

    mgfx_descriptor_destroy(u_shadow_map);

    mgfx_image_destroy(shadow_pass_depth_attachment);
    mgfx_framebuffer_destroy(shadow_pass_fbh);

    // Common
    mgfx_descriptor_destroy(u_dir_light);
    mgfx_buffer_destroy(dir_lights_buffer.idx);
}

int main(int argc, char** argv) { mgfx_example_app(); }
