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
    struct {
        // TODO: make 4th number distance for dir light
        float direction[4];
        float color[4];

        float light_space_matrix[16];
    } data;

    camera camera;
} directional_light;

typedef struct point_light {
    float position[4];

    float color[3];
    float intensity;
} point_light;

directional_light dir_light = {.data = {.direction = {1.0f, -1.0f, 0.0f, 1.0f},
                                        .color = {1.0f, 1.0f, 1.0f, 1.0f},
                                        .light_space_matrix = {0.0f}}};
mgfx_ubh dir_lights_buffer;
mgfx_dh u_dir_light;

// mgfx_ubh point_lights_buffer;

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

mgfx_sh vsh, quad_fsh;
mgfx_ph blit_program;

mgfx_vbh quad_vbh;
mgfx_ibh quad_ibh;

mgfx_th color_fba_texture;
mgfx_dh u_color_fba;

mgfx_scene sponza;
mgfx_scene helmet;
void mgfx_example_init() {
    // Common
    camera_create(mgfx_camera_type_orthographic, &dir_light.camera);

    dir_lights_buffer = mgfx_uniform_buffer_create(&dir_light.data, sizeof(dir_light.data));
    u_dir_light = mgfx_descriptor_create("u_dir_light", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_dir_light, dir_lights_buffer);

    // Directional light shadow pass
    struct mgfx_image_info shadow_pass_depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    shadow_pass_depth_attachment =
        mgfx_image_create(&shadow_pass_depth_attachment_info,
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

    mesh_pass_depth_attachment =
        mgfx_image_create(&depth_attachment_info, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

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

    helmet.vl = &vl;
    LOAD_GLTF_MODEL("DamagedHelmet", gltf_loader_flag_default, &helmet);

    sponza.vl = &vl;
    LOAD_GLTF_MODEL("Sponza", gltf_loader_flag_default, &sponza);

    // Blit pass
    vsh = mgfx_shader_create("assets/shaders/blit.vert.glsl.spv");
    quad_fsh = mgfx_shader_create("assets/shaders/sprites.frag.glsl.spv");
    blit_program = mgfx_program_create_graphics(vsh, quad_fsh);

    quad_vbh = mgfx_vertex_buffer_create(k_vertices, sizeof(k_vertices));
    quad_ibh = mgfx_index_buffer_create(k_indices, sizeof(k_indices));

    u_color_fba = mgfx_descriptor_create("u_diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    color_fba_texture = mgfx_texture_create_from_image(mesh_pass_color_attachment, VK_FILTER_LINEAR);
    mgfx_set_texture(u_color_fba, color_fba_texture);
}

void draw_scene(
    mgfx_scene* scene, uint32_t target, mgfx_ph ph, mat4 parent_transform, mx_bool is_shadow_pass) {
    for (uint32_t n = 0; n < scene->node_count; n++) {
        if (scene->nodes[n].mesh == NULL) {
            continue;
        }

        assert(scene->nodes[n].mesh->primitive_count > 0);

        mat4 model;
        glm_mat4_identity(model);
        glm_mat4_mul(scene->nodes[n].matrix, model, model);

        for (uint32_t p = 0; p < scene->nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &scene->nodes[n].mesh->primitives[p];

            if (node_primitive->index_count <= 0) {
                continue;
            }

            mat4 transform;
            glm_mat4_identity(transform);
            if (parent_transform != NULL) {
                glm_mat4_mul(parent_transform, model, transform);
                mgfx_set_transform(transform[0]);
            } else {
                mgfx_set_transform(model[0]);
            }

            mgfx_bind_vertex_buffer(node_primitive->vbh);
            mgfx_bind_index_buffer(node_primitive->ibh);

            if (!is_shadow_pass) {
                mgfx_bind_descriptor(0, u_dir_light);
                mgfx_bind_descriptor(0, u_shadow_map);

                mgfx_bind_descriptor(1, node_primitive->material->u_properties_buffer);
                mgfx_bind_descriptor(1, node_primitive->material->u_albedo_texture);
                mgfx_bind_descriptor(1, node_primitive->material->u_metallic_roughness_texture);
                mgfx_bind_descriptor(1, node_primitive->material->u_normal_texture);
                mgfx_bind_descriptor(1, node_primitive->material->u_occlusion_texture);
                mgfx_bind_descriptor(1, node_primitive->material->u_emissive_texture);
            }

            mgfx_submit(target, ph);
        }
    }
}

void mgfx_example_update() {
    static float angle = 0;

    if (mgfx_get_key(GLFW_KEY_E)) {
        angle += 3.1415925f * MGFX_TIME_DELTA_TIME;
        MX_LOG_INFO("light pos: %.2f %.2f %.2f", dir_light.camera.position[0], dir_light.camera.position[1], dir_light.camera.position[2]);
        MX_LOG_INFO("light dir: %.2f %.2f %.2f", dir_light.data.direction[0], dir_light.data.direction[1], dir_light.data.direction[2]);
    }

    if (mgfx_get_key(GLFW_KEY_R)) {
        angle -= 3.1415925f * MGFX_TIME_DELTA_TIME;
        MX_LOG_INFO("light pos: %.2f %.2f %.2f", dir_light.camera.position[0], dir_light.camera.position[1], dir_light.camera.position[2]);
        MX_LOG_INFO("light dir: %.2f %.2f %.2f", dir_light.data.direction[0], dir_light.data.direction[1], dir_light.data.direction[2]);
    }

    // Apply accumulated rotation
    mat4 rotation_matrix;
    glm_mat4_identity(rotation_matrix);

    glm_rotate(rotation_matrix, angle, (vec3){0, 0, 1});
    vec3 start_dir = {1.0f, -1.0f, 0.0f};
    glm_mat4_mulv3(rotation_matrix, start_dir, 1.0f, dir_light.data.direction);

    static float distance = 10.0f;
    if(mgfx_get_key(GLFW_KEY_UP)) {
        distance += 5.0f * MGFX_TIME_DELTA_TIME;
        MX_LOG_INFO("Distance: %.2f", distance);
    } else if(mgfx_get_key(GLFW_KEY_DOWN)) {
        distance -= 5.0f * MGFX_TIME_DELTA_TIME;
        MX_LOG_INFO("Distance: %.2f", distance);
    }

    static float screen_s = 10.0f;
    if(mgfx_get_key(GLFW_KEY_PERIOD)) {
        screen_s += 5.0f * MGFX_TIME_DELTA_TIME;
        glm_ortho(-screen_s, screen_s, -screen_s, screen_s, 0.1f, 1000.0f, dir_light.camera.proj);
        dir_light.camera.proj[1][1] *= -1;

        camera_update(&dir_light.camera);
        MX_LOG_INFO("ScreenS: %.2f", screen_s);
    } else if (mgfx_get_key(GLFW_KEY_COMMA)) {
        screen_s -= 5.0f * MGFX_TIME_DELTA_TIME;
        glm_ortho(-screen_s, screen_s, -screen_s, screen_s, 0.1f, 1000.0f, dir_light.camera.proj);
        dir_light.camera.proj[1][1] *= -1;

        camera_update(&dir_light.camera);
        MX_LOG_INFO("ScreenS: %.2f", screen_s);
    }

    // Transform the light direction
    glm_normalize(dir_light.data.direction);
    mgfx_buffer_update(dir_lights_buffer.idx, &dir_light.data, sizeof(dir_light.data), 0);

    // Update object transforms
    mat4 sponza_transform;
    glm_mat4_identity(sponza_transform);
    glm_translate(sponza_transform, (vec3){0.0f, 0.0f, 0.0f});
    glm_scale(sponza_transform, (vec3){1.0f, 1.0f, 1.0f});

    mat4 helmet_transform;
    glm_mat4_identity(helmet_transform);
    glm_translate(helmet_transform, (vec3){0.0f, 1.5f, 1.0f});
    glm_scale(helmet_transform, (vec3){0.5f, 0.5f, 0.5f});

    // Draw shadow pass
    vec3 neg_dir = { 0.0f , 0.0f, 0.0f};
    glm_vec3_negate_to(dir_light.data.direction, neg_dir);

    vec3 light_pos = { 0.0f, 0.0f, 0.0f };
    glm_vec3_sub(neg_dir, GLM_VEC3_ZERO, light_pos);
    glm_vec3_scale(neg_dir, distance, light_pos);

    dir_light.camera.position[0] = light_pos[0];
    dir_light.camera.position[1] = light_pos[1];
    dir_light.camera.position[2] = light_pos[2];
    memcpy(dir_light.camera.forward, dir_light.data.direction, sizeof(float) * 3);
    camera_update(&dir_light.camera);

    mat4 light_space_matrix;
    glm_mat4_identity(light_space_matrix);

    glm_mat4_mul(dir_light.camera.proj, dir_light.camera.view, light_space_matrix);
    memcpy(dir_light.data.light_space_matrix, light_space_matrix[0], sizeof(float) * 16);

    mgfx_set_proj(dir_light.camera.proj[0]);
    mgfx_set_view(dir_light.camera.view[0]);

    draw_scene(&sponza, 0, shadow_pass_program, sponza_transform, MX_TRUE);
    draw_scene(&helmet, 0, shadow_pass_program, helmet_transform, MX_TRUE);

    // Draw mesh pass
    mgfx_set_proj(g_example_camera.proj[0]);
    mgfx_set_view(g_example_camera.view[0]);

    draw_scene(&sponza, 1, mesh_pass_program, sponza_transform, MX_FALSE);
    draw_scene(&helmet, 1, mesh_pass_program, helmet_transform, MX_FALSE);

    // Blit to backbuffer
    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);

    if (mgfx_get_key(GLFW_KEY_L)) {
        mgfx_bind_descriptor(0, u_shadow_map);
    } else {
        mgfx_bind_descriptor(0, u_color_fba);
    }

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, blit_program);

    // TODO: Fix descriptor draw gizmo
    // mgfx_gizmo_draw_cube(g_example_camera.view[0], g_example_camera.proj[0], light_pos, NULL, NULL);
}

void mgfx_example_shutdown() {
    scene_destroy(&helmet);
    scene_destroy(&sponza);

    // Blit pass resources
    mgfx_texture_destroy(color_fba_texture, MX_FALSE);
    mgfx_descriptor_destroy(u_color_fba);

    mgfx_buffer_destroy(quad_vbh.idx);
    mgfx_buffer_destroy(quad_ibh.idx);

    mgfx_program_destroy(blit_program);
    mgfx_shader_destroy(vsh);
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
