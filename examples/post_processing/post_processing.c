#include "mgfx/defines.h"
#include "mgfx/mgfx.h"

#include <ex_common.h>
#include <gltf_loading/gltf_loader.h>

#include <ex_common.h>

#include <GLFW/glfw3.h>
#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_log.h>
#include <mx/mx_memory.h>

#include <mx/mx_math.h>
#include <mx/mx_math_mtx.h>

#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

enum frame_target {
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

scene_data scene = {0};
mgfx_ubh scene_data_buffer;
mgfx_dh u_scene_data;

directional_light sun_light = {.data = {.direction = {3.0f, -8.0f, 3.0f},
                                        .distance = 10.0f,
                                        .intensity = {1, 1, 1, 1.0f},
                                        .light_space_matrix = {0.0f}}};
mgfx_ubh sun_light_buffer;
mgfx_dh u_sun_light;

point_light point_lights[SCENE_MAX_POINT_LIGHTS] = {0};
mgfx_sbh point_lights_buffer;
mgfx_dh u_point_lights;

mgfx_imgh shadow_pass_dattachment;
mgfx_fbh shadow_pass_fbh;

mgfx_sh shadow_pass_vs;
mgfx_ph shadow_pass_program;

mgfx_th shadow_fba_texture;
mgfx_dh u_shadow_map;

mgfx_imgh mesh_pass_color_attachment;
mgfx_imgh mesh_pass_depth_attachment;

// ----- Post Processing -----
mgfx_imgh mesh_pass_bright_attachment;
mgfx_th mesh_pass_bright_attachment_th;

enum { BLUR_PASS_COUNT = 10 };
mgfx_imgh blur_pingpong_imgh[2];

mgfx_th blur_pingpong_th[2];
mgfx_dh u_blur_pingpong_dh[2];

mgfx_fbh blur_pingpong_fbh[2];

typedef struct blur_settings {
    float weights[4];
    float base;
    uint32_t horizontal;
    uint32_t padding[2];
} blur_settings;

mgfx_ubh horiz_blur_settings_buffer;
mgfx_dh u_horiz_blur_settings;

mgfx_ubh vert_blur_settings_buffer;
mgfx_dh u_vert_blur_settings;

mgfx_dh u_brightness;

mgfx_sh blur_pass_fs;
mgfx_ph blur_pass_program;
// ----- Post Processing -----

mgfx_fbh mesh_pass_fbh;

mgfx_sh mesh_pass_vs;
mgfx_sh mesh_pass_fs;
mgfx_ph mesh_pass_program;

mgfx_sh blit_vs, blit_fsh;
mgfx_ph blit_program;

mgfx_vbh quad_vbh;
mgfx_ibh quad_ibh;

mgfx_th mesh_pass_cattachment_th;
mgfx_dh u_mesh_pass_cattachment;

mgfx_scene sponza;
mgfx_scene tree;
void mgfx_example_init() {
    // Common
    camera_create(mgfx_camera_type_orthographic, &sun_light.camera);

    sun_light_buffer = mgfx_uniform_buffer_create(&sun_light.data, sizeof(sun_light.data));
    u_sun_light = mgfx_descriptor_create("sun_light", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_sun_light, sun_light_buffer);

    point_lights[0] = (point_light){
        .intensity = {10.0f, 0.0f, 0.0f, 10.0f},
        .position = {0.0f, 1.5f, 0.0f},
    };

    point_lights_buffer = mgfx_storage_buffer_create(point_lights, sizeof(point_lights));
    u_point_lights = mgfx_descriptor_create("point_lights", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    mgfx_set_buffer(u_point_lights, (mgfx_ubh){.idx = point_lights_buffer.idx});

    scene.point_light_count = 1;

    scene_data_buffer = mgfx_uniform_buffer_create(&scene, sizeof(scene_data));
    u_scene_data =
        mgfx_descriptor_create("scene_data", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_scene_data, scene_data_buffer);

    // Directional light shadow pass
    struct mgfx_image_info shadow_pass_depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH * 2,
        .height = APP_HEIGHT * 2,
        .layers = 1,
    };

    shadow_pass_dattachment = mgfx_image_create(
        &shadow_pass_depth_attachment_info,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    shadow_pass_fbh = mgfx_framebuffer_create(NULL, 0, shadow_pass_dattachment);
    mgfx_set_view_target(SHADOW_FRAME_TARGET, shadow_pass_fbh);

    shadow_pass_vs = mgfx_shader_create("shadow.vert.spv");

    shadow_pass_program =
        mgfx_program_create_graphics(shadow_pass_vs,
                                     (mgfx_sh){.idx = 0},
                                     {
                                         .name = {"shadow_pass"},
                                         .polygon_mode = MGFX_FILL,
                                         .primitive_topology = MGFX_TRIANGLE_LIST,
                                         .cull_mode = MGFX_CULL_FRONT,
                                     });

    // HDR Mesh pass
    struct mgfx_image_info hdr_color_info = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mesh_pass_color_attachment = mgfx_image_create(
        &hdr_color_info,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    // ----- Post Processing -----
    mesh_pass_bright_attachment = mgfx_image_create(
        &hdr_color_info,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);
    mesh_pass_bright_attachment_th =
        mgfx_texture_create_from_image(mesh_pass_bright_attachment, VK_FILTER_LINEAR);
    // ----- Post Processing -----

    struct mgfx_image_info depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mesh_pass_depth_attachment =
        mgfx_image_create(&depth_attachment_info,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    mgfx_imgh mesh_pass_color_attachments[2] = {mesh_pass_color_attachment,
                                                mesh_pass_bright_attachment};

    mesh_pass_fbh = mgfx_framebuffer_create(mesh_pass_color_attachments,
                                            2,
                                            mesh_pass_depth_attachment);
    mgfx_set_view_target(MESH_FRAME_TARGET, mesh_pass_fbh);
    mgfx_set_view_clear(MESH_FRAME_TARGET, (float[4]){0.0f, 0.0f, 0.0f, 1.0f});

    mesh_pass_vs = mgfx_shader_create("lit.vert.spv");
    mesh_pass_fs = mgfx_shader_create("lit.frag.spv");
    mesh_pass_program = mgfx_program_create_graphics(mesh_pass_vs,
                                                     mesh_pass_fs,
                                                     &(mgfx_graphics_create_info){
                                                         .name = "lit_mesh_pass",
                                                         .cull_mode = MGFX_CULL_BACK,
                                                     });

    u_shadow_map =
        mgfx_descriptor_create("shadow_map", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    shadow_fba_texture =
        mgfx_texture_create_from_image(shadow_pass_dattachment, VK_FILTER_LINEAR);
    mgfx_set_texture(u_shadow_map, shadow_fba_texture);

    mgfx_vertex_layout vl;
    mgfx_vertex_layout_begin(&vl);
    mgfx_vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
    mgfx_vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
    mgfx_vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD0, sizeof(float) * 2);
    mgfx_vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
    mgfx_vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TANGENT, sizeof(float) * 4);
    mgfx_vertex_layout_end(&vl);

    tree.vl = &vl;
    LOAD_GLTF_MODEL("DamagedHelmet", gltf_loader_flag_default, &tree);

    sponza.vl = &vl;
    LOAD_GLTF_MODEL("Sponza", gltf_loader_flag_default, &sponza);

    // Blit pass
    blit_vs = mgfx_shader_create("blit.vert.spv");
    blit_fsh = mgfx_shader_create("blit.frag.spv");
    blit_program =
        mgfx_program_create_graphics(blit_vs,
                                     blit_fsh,
                                     &(mgfx_graphics_create_info){.name = "blit_pass"});

    quad_vbh = mgfx_vertex_buffer_create(MGFX_FS_QUAD_VERTICES,
                                         sizeof(MGFX_FS_QUAD_VERTICES),
                                         &MGFX_PNTU32F_LAYOUT);
    quad_ibh =
        mgfx_index_buffer_create(MGFX_FS_QUAD_INDICES, sizeof(MGFX_FS_QUAD_INDICES));

    u_mesh_pass_cattachment =
        mgfx_descriptor_create("diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    mesh_pass_cattachment_th =
        mgfx_texture_create_from_image(mesh_pass_color_attachment, VK_FILTER_LINEAR);
    mgfx_set_texture(u_mesh_pass_cattachment, mesh_pass_cattachment_th);

    // ----- Post Processing -----
    for (int i = 0; i < 2; i++) {
        blur_pingpong_imgh[i] = mgfx_image_create(
            &hdr_color_info,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT);

        blur_pingpong_th[i] =
            mgfx_texture_create_from_image(blur_pingpong_imgh[i], VK_FILTER_LINEAR);
        blur_pingpong_fbh[i] =
            mgfx_framebuffer_create(&blur_pingpong_imgh[i], 1, (mgfx_imgh){.idx = 0});

        if (i == 0) {
            u_blur_pingpong_dh[i] =
                mgfx_descriptor_create("ping_pong_0",
                                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        } else {
            u_blur_pingpong_dh[i] =
                mgfx_descriptor_create("ping_pong_1",
                                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }

        mgfx_set_texture(u_blur_pingpong_dh[i], blur_pingpong_th[i]);
    }

    mgfx_set_view_clear(BLUR_0_FRAME_TARGET, (float[4]){0.0f, 0.0f, 0.0f, 1.0f});
    for (int i = 0; i < BLUR_PASS_COUNT; i++) {
        mgfx_set_view_target(BLUR_0_FRAME_TARGET + i, blur_pingpong_fbh[i % 2]);
    }

    blur_pass_fs = mgfx_shader_create("blur.frag.spv");
    blur_pass_program =
        mgfx_program_create_graphics(blit_vs,
                                     blur_pass_fs,
                                     &(mgfx_graphics_create_info){.name = "blur_pass"});

    blur_settings horiz = {
        .weights = {0.1945946, 0.1216216, 0.054054, 0.016216},
        .base = 0.227027,
        .horizontal = MX_TRUE,
    };

    blur_settings vert = {
        .weights = {0.1945946, 0.1216216, 0.054054, 0.016216},
        .base = 0.227027,
        .horizontal = MX_FALSE,
    };

    horiz_blur_settings_buffer =
        mgfx_uniform_buffer_create(&horiz, sizeof(blur_settings));
    vert_blur_settings_buffer = mgfx_uniform_buffer_create(&vert, sizeof(blur_settings));

    u_horiz_blur_settings =
        mgfx_descriptor_create("horiz_blur", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_horiz_blur_settings, horiz_blur_settings_buffer);

    u_vert_blur_settings =
        mgfx_descriptor_create("vert_blur", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_vert_blur_settings, vert_blur_settings_buffer);

    u_brightness =
        mgfx_descriptor_create("brightness", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    mgfx_set_texture(u_brightness, mesh_pass_bright_attachment_th);
    // ----- Post Processing -----
}

void draw_scene(mgfx_scene* scene,
                uint32_t target,
                mgfx_ph ph,
                mx_mat4 parent_transform,
                mx_bool is_shadow_pass) {
    for (uint32_t n = 0; n < scene->node_count; n++) {
        if (scene->nodes[n].mesh == NULL) {
            continue;
        }

        MX_ASSERT(scene->nodes[n].mesh->primitive_count > 0);

        for (uint32_t p = 0; p < scene->nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &scene->nodes[n].mesh->primitives[p];

            if (node_primitive->index_count <= 0) {
                continue;
            }

            mgfx_set_transform(mx_mat4_mul(parent_transform, scene->nodes[n].matrix).val);

            mgfx_bind_vertex_buffer(node_primitive->vbh);
            mgfx_bind_index_buffer(node_primitive->ibh);

            if (!is_shadow_pass) {
                mgfx_bind_descriptor(u_scene_data);
                mgfx_bind_descriptor(u_sun_light);
                mgfx_bind_descriptor(u_shadow_map);
                mgfx_bind_descriptor(u_point_lights);

                mgfx_bind_descriptor(node_primitive->material->u_properties_buffer);
                mgfx_bind_descriptor(node_primitive->material->u_albedo_texture);
                mgfx_bind_descriptor(
                    node_primitive->material->u_metallic_roughness_texture);
                mgfx_bind_descriptor(node_primitive->material->u_normal_texture);
                mgfx_bind_descriptor(node_primitive->material->u_occlusion_texture);
                mgfx_bind_descriptor(node_primitive->material->u_emissive_texture);
            }

            mgfx_submit(target, ph);
        }
    }
}

void mgfx_example_update() {
    static float distance = 10.0f;
    static float angle = 0;

    if (mgfx_get_key(GLFW_KEY_E)) {
        angle += 3.1415925f * MGFX_TIME_DELTA_TIME;
    }

    if (mgfx_get_key(GLFW_KEY_R)) {
        angle -= 3.1415925f * MGFX_TIME_DELTA_TIME;
    }

    // Transform the light direction
    scene.camera_position = g_example_camera.position;
    mgfx_buffer_update(scene_data_buffer.idx, &scene, sizeof(scene_data), 0);

    mx_mat4 rotation_matrix = mx_mat4_rotate_euler(angle, (mx_vec3){1, 0, 1});

    static mx_vec4 start_dir = {1.0f, -1.0f, 0.0f, 1.0f};
    sun_light.data.direction = mx_mat_mul_vec4(rotation_matrix, start_dir).xyz;

    sun_light.data.direction = mx_vec3_norm(sun_light.data.direction);
    mgfx_buffer_update(sun_light_buffer.idx, &sun_light.data, sizeof(sun_light.data), 0);

    // Update object transforms
    mx_mat4 sponza_transform = mx_mat4_mul(mx_translate((mx_vec3){0.0f, 0.0f, 0.0f}),
                                           mx_scale((mx_vec3){1.0f, 1.0f, 1.0f}));

    mx_mat4 helmet_transform =
        mx_mat4_mul(mx_mat4_mul(mx_translate((mx_vec3){0.0f, 1.5f, 0.0f}),
                                mx_mat4_rotate_euler(MX_DEG_TO_RAD(-90), MX_VEC3_UP)),
                    mx_scale((mx_vec3){1.0f, 1.0f, 1.0f}));

    // Draw shadow pass
    mx_vec3 neg_dir = mx_vec3_scale(sun_light.data.direction, -1.0f);

    mx_vec3 light_pos = mx_vec3_scale(mx_vec3_sub(neg_dir, MX_VEC3_ZERO), distance);

    sun_light.camera.position.x = light_pos.x;
    sun_light.camera.position.y = light_pos.y;
    sun_light.camera.position.z = light_pos.z;
    sun_light.camera.forward = sun_light.data.direction;
    camera_update(&sun_light.camera);

    memcpy(sun_light.data.light_space_matrix,
           sun_light.camera.view_proj.val,
           sizeof(float) * 16);

    mgfx_set_proj(sun_light.camera.proj.val);
    mgfx_set_view(sun_light.camera.view.val);

    draw_scene(&sponza,
               SHADOW_FRAME_TARGET,
               shadow_pass_program,
               sponza_transform,
               MX_TRUE);
    draw_scene(&tree,
               SHADOW_FRAME_TARGET,
               shadow_pass_program,
               helmet_transform,
               MX_TRUE);

    // Draw mesh pass
    mgfx_set_proj(g_example_camera.proj.val);
    mgfx_set_view(g_example_camera.view.val);

    draw_scene(&sponza, MESH_FRAME_TARGET, mesh_pass_program, sponza_transform, MX_FALSE);
    draw_scene(&tree, MESH_FRAME_TARGET, mesh_pass_program, helmet_transform, MX_FALSE);

    // ----- Post Processing -----

    // Bloom Blur
    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);

    mgfx_bind_descriptor(u_horiz_blur_settings);
    mgfx_bind_descriptor(u_brightness);

    mgfx_submit(BLUR_0_FRAME_TARGET, blur_pass_program);

    for (int i = 1; i < BLUR_PASS_COUNT; i++) {
        mgfx_bind_vertex_buffer(quad_vbh);
        mgfx_bind_index_buffer(quad_ibh);

        if ((i + 1) % 2 != 0) {
            mgfx_bind_descriptor(u_horiz_blur_settings);
        } else {
            mgfx_bind_descriptor(u_vert_blur_settings);
        }

        mgfx_bind_descriptor(u_blur_pingpong_dh[(i + 1) % 2]);
        mgfx_submit(BLUR_0_FRAME_TARGET + i, blur_pass_program);
    }

    // ----- Post Processing -----

    // ----- HDR, Gamma correction, Post Process Resolution -----

    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);

    if (mgfx_get_key(GLFW_KEY_L)) {
        mgfx_bind_descriptor(u_shadow_map);
    } else {
        mgfx_bind_descriptor(u_mesh_pass_cattachment);
        mgfx_bind_descriptor(u_blur_pingpong_dh[1]);
    }

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, blit_program);

    // ----- HDR, Gamma correction, Post Process Resolution -----

    mgfx_debug_draw_text(0, 0, "distance: %.2f", distance);
    mgfx_debug_draw_text(0,
                         32,
                         "light pos: %.2f %.2f %.2f",
                         sun_light.camera.position.x,
                         sun_light.camera.position.y,
                         sun_light.camera.position.z);

    mgfx_gizmo_draw_cube(g_example_camera.view.val,
                         g_example_camera.proj.val,
                         light_pos,
                         MX_QUAT_IDENTITY,
                         MX_VEC3_ONE);
}

void mgfx_example_shutdown() {
    scene_destroy(&tree);
    scene_destroy(&sponza);

    // Blit pass resources
    mgfx_texture_destroy(mesh_pass_cattachment_th, MX_FALSE);
    mgfx_descriptor_destroy(u_mesh_pass_cattachment);

    mgfx_buffer_destroy(quad_vbh.idx);
    mgfx_buffer_destroy(quad_ibh.idx);

    mgfx_program_destroy(blit_program);
    mgfx_shader_destroy(blit_vs);
    mgfx_shader_destroy(blit_fsh);

    // Mesh pass resources
    mgfx_program_destroy(mesh_pass_program);
    mgfx_shader_destroy(mesh_pass_fs);
    mgfx_shader_destroy(mesh_pass_vs);

    mgfx_image_destroy(mesh_pass_depth_attachment);
    mgfx_image_destroy(mesh_pass_color_attachment);

    // ----- Post Processing -----
    mgfx_texture_destroy(mesh_pass_bright_attachment_th, MX_TRUE);
    mgfx_buffer_destroy(horiz_blur_settings_buffer.idx);
    mgfx_descriptor_destroy(u_horiz_blur_settings);

    mgfx_buffer_destroy(vert_blur_settings_buffer.idx);
    mgfx_descriptor_destroy(u_vert_blur_settings);

    mgfx_descriptor_destroy(u_brightness);

    mgfx_shader_destroy(blur_pass_fs);
    mgfx_program_destroy(blur_pass_program);

    for (int i = 0; i < 2; i++) {
        mgfx_framebuffer_destroy(blur_pingpong_fbh[i]);
        mgfx_descriptor_destroy(u_blur_pingpong_dh[i]);
        mgfx_texture_destroy(blur_pingpong_th[i], MX_TRUE);
    }

    // ----- Post Processing -----

    mgfx_framebuffer_destroy(mesh_pass_fbh);

    // Shadow map resources
    mgfx_texture_destroy(shadow_fba_texture, MX_FALSE);

    mgfx_shader_destroy(shadow_pass_vs);
    mgfx_program_destroy(shadow_pass_program);

    mgfx_descriptor_destroy(u_shadow_map);

    mgfx_image_destroy(shadow_pass_dattachment);
    mgfx_framebuffer_destroy(shadow_pass_fbh);

    // Scene common
    mgfx_descriptor_destroy(u_point_lights);
    mgfx_buffer_destroy(point_lights_buffer.idx);

    mgfx_descriptor_destroy(u_sun_light);
    mgfx_buffer_destroy(sun_light_buffer.idx);

    mgfx_descriptor_destroy(u_scene_data);
    mgfx_buffer_destroy(scene_data_buffer.idx);
}

int main(int argc, char** argv) { mgfx_example_app(); }
