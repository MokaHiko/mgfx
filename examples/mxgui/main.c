#include "GLFW/glfw3.h"
#include "ex_common.h"

#include <mx/mx_darray.h>
#include <mx/mx_hash.h>
#include <mx/mx_log.h>

#include <mx/mx_math_mtx.h>

#include "ecs.h"

// Scene graph
typedef mx_vec3 position;
typedef mx_quat rotation;
typedef mx_vec3 scale;

typedef struct transform {
    mx_vec3 forward;
    mx_vec3 right;
    mx_mat4 matrix;

    mx_actor parent;
} transform;

// Renderer
#include <gltf_loader.h>
#include <mgfx/mgfx.h>
#include <mxgui/mxgui.h>

typedef enum frame_targets {
    SHADOW_FRAME_TARGET,
    FORWARD_FRAME_TARGET,
    BLUR_0_FRAME_TARGET, // Horizontal
    BLUR_1_FRAME_TARGET, // Vertical and out
} frame_targets;

// TODO: Make lights hold their shadow maps
typedef struct directional_light {
    mx_mat4 light_space_matrix;
    mx_vec3 direction;
    float distance;
    mx_vec4 intensity;
} directional_light;

typedef struct point_light {
    float light_space_matrix[16];
    mx_vec3 direction;
    float distance;
    mx_vec4 intensity;
} point_light;

typedef struct static_mesh_renderer {
    mx_mat4 transfom_matrix; // global transform

    // Mesh
    mgfx_vbh vbh;
    mgfx_ibh ibh;

    mx_bool is_static;

    // Material
    mgfx_dh u_properties;

    mgfx_dh textures[16];
    uint32_t texture_count;

    mgfx_ph ph;
} static_mesh_renderer;

typedef enum pbr_material_textures {
    PBR_TEXTURE_ALBEDO_MAP,
    PBR_TEXTURE_METALLIC_ROUGHNESS_MAP,
    PBR_TEXTURE_NORMAL_MAP,
    PBR_TEXTURE_OCCLUSION_MAP,
    PBR_TEXTURE_EMISSIVE_MAP,

    PBR_TEXTURE_COUNT,
} pbr_material_textures;

mx_actor actor_spawn_from_gltf(mgfx_scene* gltf, mgfx_ph ph) {
    mx_actor out = mx_actor_spawn({.name = "gltf"});

    mx_actor_set_impl(out, component_id_from_type(position), &MX_VEC3_ONE);
    mx_actor_set_impl(out, component_id_from_type(rotation), &MX_QUAT_IDENTITY);
    mx_actor_set_impl(out, component_id_from_type(scale), &MX_VEC3_ONE);
    mx_actor_set(out, transform, {.parent = MX_INVALID_ACTOR_ID, .matrix = MX_MAT4_IDENTITY});

    for (uint32_t n = 0; n < gltf->node_count; n++) {
        if (gltf->nodes[n].mesh == NULL) {
            continue;
        }

        MX_ASSERT(gltf->nodes[n].mesh->primitive_count > 0);

        for (uint32_t p = 0; p < gltf->nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &gltf->nodes[n].mesh->primitives[p];

            if (node_primitive->index_count <= 0) {
                continue;
            }

            mx_actor node = mx_actor_spawn({.parent = out});

            mx_actor_set_impl(node, component_id_from_type(position), &gltf->nodes[n].position);
            mx_actor_set_impl(node, component_id_from_type(scale), &gltf->nodes[n].scale);
            mx_actor_set_impl(node, component_id_from_type(rotation), &gltf->nodes[n].rotation);

            // TODO: Parent
            mx_actor_set(node, transform, {
                .matrix = MX_MAT4_IDENTITY,
                .parent = out
            });

            static_mesh_renderer* mesh_renderer =
                mx_actor_set(node,
                    static_mesh_renderer,
                    {
                        .vbh = node_primitive->vbh,
                        .ibh = node_primitive->ibh,
                        .ph = ph,
                    });

            mesh_renderer->u_properties = node_primitive->material->u_properties_buffer;

            mesh_renderer->textures[PBR_TEXTURE_ALBEDO_MAP] =
                *(&node_primitive->material->u_albedo_texture);

            mesh_renderer->textures[PBR_TEXTURE_METALLIC_ROUGHNESS_MAP] =
                *(&node_primitive->material->u_metallic_roughness_texture);

            mesh_renderer->textures[PBR_TEXTURE_NORMAL_MAP] =
                *(&node_primitive->material->u_normal_texture);

            mesh_renderer->textures[PBR_TEXTURE_OCCLUSION_MAP] =
                *(&node_primitive->material->u_occlusion_texture);

            mesh_renderer->textures[PBR_TEXTURE_EMISSIVE_MAP] =
                *(&node_primitive->material->u_emissive_texture);

            mesh_renderer->texture_count = PBR_TEXTURE_COUNT;
        }
    }

    return out;
}

struct {
    mx_vec3 camera_position;
    uint32_t point_light_count;
} scene_data;

enum {
    MGFX_MAX_COLOR_ATTACHMENTS = 4,
    MGFX_MAX_UNIFORMS = 16,
};

mgfx_vbh quad_vbh;
mgfx_ibh quad_ibh;
typedef struct blit_pass {
    mgfx_dh pass_uniforms[MGFX_MAX_UNIFORMS];
    uint32_t pass_uniform_count;

    mx_bool no_clear;
    mx_vec4 view_clear;

    uint8_t view_target;
    mgfx_fbh fbh;

    mgfx_ph ph;
} blit_pass;

int blit_pass_begin(blit_pass* blit_pass) {
    // TODO: Put to on creeate
    mgfx_set_view_target(blit_pass->view_target, blit_pass->fbh);
    if (!blit_pass->no_clear) {
        mgfx_set_view_clear(blit_pass->view_target, blit_pass->view_clear.elements);
    }

    for (uint32_t i = 0; i < blit_pass->pass_uniform_count; i++) {
        mgfx_bind_descriptor(blit_pass->pass_uniforms[i]);
    }

    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);

    mgfx_set_transform(MX_MAT4_IDENTITY.val);
    return MX_SUCCESS;
}

void blit_pass_end(blit_pass* blit_pass) {}

typedef struct render_pass {
    struct pass_uniform {
        mgfx_dh uh;
        mgfx_uniform_type type;

        union {
            mgfx_th th;
            mgfx_ubh ubh;
            mgfx_sbh sbh;
        };
    } pass_uniforms[MGFX_MAX_UNIFORMS];
    uint32_t pass_uniform_count;

    mx_vec4 view_clear;
    uint8_t view_target;
    mgfx_fbh fbh;
} render_pass;

int render_pass_begin(render_pass* mesh_pass) {
    mgfx_set_view_target(mesh_pass->view_target, mesh_pass->fbh);
    mgfx_set_view_clear(mesh_pass->view_target, mesh_pass->view_clear.elements);

    for (uint32_t i = 0; i < mesh_pass->pass_uniform_count; i++) {
        switch (mesh_pass->pass_uniforms[i].type) {
        case MGFX_UNIFORM_TYPE_SAMPLER:
        case MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER:
        case MGFX_UNIFORM_TYPE_STORAGE_IMAGE:
        case MGFX_UNIFORM_TYPE_SAMPLED_IMAGE:
            mgfx_set_texture(mesh_pass->pass_uniforms[i].uh,
                             mesh_pass->pass_uniforms[i].th);
            break;
        case MGFX_UNIFORM_TYPE_UNIFORM_TEXEL_BUFFER:
        case MGFX_UNIFORM_TYPE_STORAGE_TEXEL_BUFFER:
        case MGFX_UNIFORM_TYPE_UNIFORM_BUFFER:
        case MGFX_UNIFORM_TYPE_STORAGE_BUFFER:
            mgfx_set_buffer(mesh_pass->pass_uniforms[i].uh,
                            mesh_pass->pass_uniforms[i].ubh);
            break;
        case MGFX_UNIFORM_TYPE_REF:
            break;
        }
    }

    return MX_SUCCESS;
};

mx_bool render_pass_bind_descriptors(render_pass* mesh_pass) {
    for (uint32_t i = 0; i < mesh_pass->pass_uniform_count; i++) {
        mgfx_bind_descriptor(mesh_pass->pass_uniforms[i].uh);
    }

    return MX_TRUE;
}

void render_pass_end(render_pass* mesh_pass) {}

render_pass shadow_pass;
mgfx_ph shadow_ph;

typedef enum forward_pass_uniforms {
    FORWARD_PASS_SCENE_DATA_UNIFORM,
    FORWARD_PASS_DIRECTIONAL_LIGHT_UNIFORM,
    FORWARD_PASS_DIRECTIONAL_LIGHT_SHADOW_MAP_UNIFORM,
    FORWARD_PASS_POINT_LIGHT_UNIFORM,

    FORWARD_PASS_UNIFORM_COUNT,
} forward_pass_uniforms;
render_pass forward_pass;

typedef enum blur_pass_uniforms {
    BLUR_PASS_SETTINGS_UNIFORM,
    BLUR_PASS_BLUR_IMAGE_UNIFORM,
} blur_pass_uniforms;
enum { BLUR_PASS_COUNT = 10 };
blit_pass blur_pingpong_pass[BLUR_PASS_COUNT];

typedef enum post_process_uniforms {
    POST_PROCESS_PASS_COLOR_UNIFORM,
    POST_PROCESS_PASS_BLOOM_UNIFORM,
} post_process_uniforms;
blit_pass post_process_resolve_pass;

point_light point_lights = {0};
mgfx_sbh point_lights_buffer;
mgfx_dh u_point_lights;

void mgfx_example_init() {

    if (mx_ecs_init() != MX_SUCCESS) {
        return;
    }

    // Scene graph
    mx_component_declare(position);
    mx_component_declare(rotation);
    mx_component_declare(scale);
    mx_component_declare(transform);

    // Renderer
    mx_component_declare(static_mesh_renderer);
    mx_component_declare(directional_light);
    mx_component_declare(point_light);

    quad_vbh = mgfx_vertex_buffer_create(MGFX_FS_QUAD_VERTICES,
                                         sizeof(MGFX_FS_QUAD_VERTICES),
                                         &MGFX_PNTU32F_LAYOUT);
    quad_ibh = mgfx_index_buffer_create(MGFX_FS_QUAD_INDICES, sizeof(MGFX_FS_QUAD_INDICES));

    point_lights_buffer = mgfx_storage_buffer_create(&point_lights, sizeof(point_lights));
    u_point_lights = mgfx_descriptor_create_and_set_storage_buffer("point_lights",
                                                                   point_lights_buffer);

    struct mgfx_image_info hdr_color_info = {
        .format = MGFX_FORMAT_R16G16B16A16_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mgfx_imgh shadow_pass_depth_attachment = mgfx_image_create(
        &(mgfx_image_info){
            .format = MGFX_FORMAT_D32_SFLOAT,
            .width = APP_WIDTH,
            .height = APP_HEIGHT,
            .layers = 1,
        },
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    shadow_pass = (render_pass){
        .view_target = SHADOW_FRAME_TARGET,
        .fbh = mgfx_framebuffer_create(NULL, 0, shadow_pass_depth_attachment)};

    shadow_ph = mgfx_program_create_graphics(mgfx_shader_create("shadow.vert.spv"),
                                             (mgfx_sh){0},
                                             {
                                                 .name = "shadow_program",
                                                 .cull_mode = MGFX_CULL_FRONT,
                                             });

    mgfx_imgh forward_pass_bright_attachment = mgfx_image_create(
        &(mgfx_image_info){
            .format = MGFX_FORMAT_R16G16B16A16_SFLOAT, // HDR
            .width = APP_WIDTH,
            .height = APP_HEIGHT,
            .layers = 1,
        },
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    mgfx_imgh forward_pass_color_attachment = mgfx_image_create(
        &(mgfx_image_info){
            .format = MGFX_FORMAT_R16G16B16A16_SFLOAT, // HDR
            .width = APP_WIDTH,
            .height = APP_HEIGHT,
            .layers = 1,
        },
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    forward_pass = (render_pass){
        .view_target = FORWARD_FRAME_TARGET,
        .view_clear = {0.0f, 0.0f, 0.0f, 1.0f},
        .fbh = mgfx_framebuffer_create(
            (mgfx_imgh[]){
                forward_pass_color_attachment,
                forward_pass_bright_attachment,
            },
            2,
            mgfx_image_create(
                &(mgfx_image_info){
                    .format = MGFX_FORMAT_D32_SFLOAT,
                    .width = APP_WIDTH,
                    .height = APP_HEIGHT,
                    .layers = 1,
                },
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)),
        .pass_uniforms =
            {
                [FORWARD_PASS_SCENE_DATA_UNIFORM] =
                    {.ubh = mgfx_uniform_buffer_create(NULL, sizeof(scene_data)),
                     .type = MGFX_UNIFORM_TYPE_UNIFORM_BUFFER,
                     .uh = mgfx_descriptor_create("u_scene_data",
                                                  MGFX_UNIFORM_TYPE_UNIFORM_BUFFER)},

                [FORWARD_PASS_DIRECTIONAL_LIGHT_UNIFORM] =
                    {.ubh = mgfx_uniform_buffer_create(NULL, sizeof(directional_light)),
                     .type = MGFX_UNIFORM_TYPE_UNIFORM_BUFFER,
                     .uh = mgfx_descriptor_create("directional_light",
                                                  MGFX_UNIFORM_TYPE_UNIFORM_BUFFER)},

                [FORWARD_PASS_DIRECTIONAL_LIGHT_SHADOW_MAP_UNIFORM] =
                    {
                        .th = mgfx_texture_create_from_image(shadow_pass_depth_attachment,
                                                             MGFX_FILTER_LINEAR),
                        .type = MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER,
                        .uh = mgfx_descriptor_create(
                            "directional_light_shadow_map",
                            MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER),
                    },

                [FORWARD_PASS_POINT_LIGHT_UNIFORM] =
                    {
                        .uh = u_point_lights,
                        .type = MGFX_UNIFORM_TYPE_REF,
                    },
            },
        .pass_uniform_count = 4,
    };

    mgfx_sh blit_vsh = mgfx_shader_create("blit.vert.spv");
    mgfx_sh blur_fsh = mgfx_shader_create("blur.frag.spv");
    mgfx_ph blur_ph = mgfx_program_create_graphics(blit_vsh,
                                                   blur_fsh,
                                                   {
                                                       .name = "ping_pong",
                                                       .cull_mode = MGFX_CULL_BACK,
                                                       .destroy_shaders = MX_TRUE,
                                                   });

    typedef struct blur_settings {
        float weights[4];
        float base;
        uint32_t horizontal;
        uint32_t padding[2];
    } blur_settings;

    blur_settings horiz = {
        .weights = {0.1945946f, 0.1216216f, 0.054054f, 0.016216f},
        .base = 0.227027f,
        .horizontal = MX_TRUE,
    };

    blur_settings vert = {
        .weights = {0.1945946f, 0.1216216f, 0.054054f, 0.016216f},
        .base = 0.227027f,
        .horizontal = MX_FALSE,
    };

    mgfx_ubh horiz_blur_settings_buffer =
        mgfx_uniform_buffer_create(&horiz, sizeof(blur_settings));
    mgfx_ubh vert_blur_settings_buffer =
        mgfx_uniform_buffer_create(&vert, sizeof(blur_settings));

    mgfx_dh u_horiz_blur_settings =
        mgfx_descriptor_create("horiz_blur", MGFX_UNIFORM_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_horiz_blur_settings, horiz_blur_settings_buffer);

    mgfx_dh u_vert_blur_settings =
        mgfx_descriptor_create("vert_blur", MGFX_UNIFORM_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_vert_blur_settings, vert_blur_settings_buffer);

    mgfx_th bright_texture =
        mgfx_texture_create_from_image(forward_pass_bright_attachment,
                                       MGFX_FILTER_LINEAR);

    mgfx_dh u_brightness =
        mgfx_descriptor_create("brightness", MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER);
    mgfx_set_texture(u_brightness, bright_texture);

    mgfx_imgh horiz_color_attachment = mgfx_image_create(
        &(mgfx_image_info){
            .format = MGFX_FORMAT_R16G16B16A16_SFLOAT, // HDR
            .width = APP_WIDTH,
            .height = APP_HEIGHT,
            .layers = 1,
        },
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    mgfx_imgh vert_color_attachment = mgfx_image_create(
        &(mgfx_image_info){
            .format = MGFX_FORMAT_R16G16B16A16_SFLOAT, // HDR
            .width = APP_WIDTH,
            .height = APP_HEIGHT,
            .layers = 1,
        },
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    mgfx_imgh blur_color_attachments[2] = {horiz_color_attachment, vert_color_attachment};
    mgfx_dh u_blur_settings[2] = {u_horiz_blur_settings, u_vert_blur_settings};

    mgfx_fbh blur_fbs[2] = {
        mgfx_framebuffer_create(&blur_color_attachments[0], 1, (mgfx_imgh){.idx = 0}),
        mgfx_framebuffer_create(&blur_color_attachments[1], 1, (mgfx_imgh){.idx = 0}),
    };

    mgfx_dh blur_texture_dhs[2] = {
        mgfx_descriptor_create_and_set_texture(
            "ping",
            mgfx_texture_create_from_image(blur_color_attachments[0],
                                           MGFX_FILTER_LINEAR)),
        mgfx_descriptor_create_and_set_texture(
            "pong",
            mgfx_texture_create_from_image(blur_color_attachments[1],
                                           MGFX_FILTER_LINEAR)),
    };

    blur_pingpong_pass[0] = (blit_pass){
        .fbh = blur_fbs[0],
        .view_target = BLUR_0_FRAME_TARGET,
        .no_clear = MX_TRUE,
        .pass_uniforms =
            {
                u_blur_settings[0],
                mgfx_descriptor_create_and_set_texture(
                    "copy",
                    mgfx_texture_create_from_image(forward_pass_bright_attachment,
                                                   MGFX_FILTER_LINEAR)),
            },
        .ph = blur_ph,
        .pass_uniform_count = 2,
    };

    for (int i = 1; i < BLUR_PASS_COUNT; i++) {
        blur_pingpong_pass[i] = (blit_pass){
            .fbh = blur_fbs[i % 2],
            .view_target = BLUR_0_FRAME_TARGET + i,
            .no_clear = MX_TRUE,
            .pass_uniforms =
                {
                    u_blur_settings[i % 2],
                    blur_texture_dhs[(i + 1) % 2],
                },
            .ph = blur_ph,
            .pass_uniform_count = 2,
        };
    }

    mxgui_init(&(mxgui_gui_settings){
        .height = APP_HEIGHT,
        .width = APP_WIDTH,
    });

    post_process_resolve_pass = (blit_pass){
        .view_target = MGFX_DEFAULT_VIEW_TARGET,
        .pass_uniforms =
            {
                mgfx_descriptor_create_and_set_texture(
                    "color",
                    mgfx_texture_create_from_image(forward_pass_color_attachment,
                                                   VK_FILTER_LINEAR)),

                mgfx_descriptor_create_and_set_texture(
                    "blur",
                    mgfx_texture_create_from_image(blur_color_attachments[1],
                                                   VK_FILTER_LINEAR)),

            },
        .pass_uniform_count = 2,
        .ph = mgfx_program_create_graphics(blit_vsh,
                                           mgfx_shader_create("blit.frag.spv"),
                                           {.name = "post_process_resolve",
                                            .cull_mode = MGFX_CULL_NONE,
                                            .destroy_shaders = MX_TRUE})};

    // Game
    mgfx_ph pbr_ph = mgfx_program_create_graphics(
        mgfx_shader_create("lit.vert.spv"),
        mgfx_shader_create("lit.frag.spv"),
        {.name = "pbr", .cull_mode = MGFX_CULL_BACK, .destroy_shaders = MX_TRUE});

    // Scene
    mx_actor sun_light = mx_actor_spawn({.name = "sun_light"});
    mx_actor_set(sun_light, position, {0, 10, 0});
    mx_actor_set(sun_light, rotation, {0, 0, 0});
    mx_actor_set(sun_light, scale, {1, 1, 1});
    mx_actor_set(sun_light, transform, {.parent = MX_INVALID_ACTOR_ID, .matrix = MX_MAT4_IDENTITY});
    mx_actor_set(sun_light,
                 directional_light,
                 {
                     .direction = {1.0f, -1.0f, 0.0f},
                     .distance = 1.0f,
                     .intensity = mx_vec4_scale((mx_vec4){1.0f, 1.0f, 1.0f, 1.0f}, 100000.0f),
                 });

    //static mgfx_scene sponza_scene = { 0 };
    //LOAD_GLTF_MODEL("Sponza", gltf_loader_flag_default, &sponza_scene);
    //mx_actor sponza = actor_spawn_from_gltf(&sponza_scene, pbr_ph);

    static mgfx_scene steam_boat_scene = { 0 };
    load_scene_from_path("C:/Users/ADMIN/Downloads/steamboat.gltf", gltf_loader_flag_default, &steam_boat_scene);
    mx_actor steam_boat = actor_spawn_from_gltf(&steam_boat_scene, pbr_ph);
    const position* bot_pos = mx_actor_set(steam_boat, position, { .y = 0.0f, .x = 10.0f });

    static mgfx_scene damaged_helmet_scene = { 0 };
    LOAD_GLTF_MODEL("DamagedHelmet", gltf_loader_flag_default, &damaged_helmet_scene);
    mx_actor damaged_helmet = actor_spawn_from_gltf(&damaged_helmet_scene, pbr_ph);
    position* damaged_helmet_pos = mx_actor_find(damaged_helmet, position);
    *damaged_helmet_pos = mx_vec3_add(*damaged_helmet_pos, (mx_vec3){-10.0f, 5.0f, 0.0f});
    
    static mgfx_scene trees_scene = { 0 };
    load_scene_from_path("C:/Users/ADMIN/Downloads/trees02.gltf",
                         gltf_loader_flag_default,
                         &trees_scene);
    actor_spawn_from_gltf(&trees_scene, pbr_ph);
}

void mgfx_example_update() {
    // Transform update
    {
        mx_ecs_iter iter = mx_ecs_query_iter(transform);
        while (mx_iter_next(&iter)) {
            mx_actor actor = iter.actor;

            const position* p = mx_actor_find(actor, position);
            const rotation* r = mx_actor_find(actor, rotation);
            const scale* s = mx_actor_find(actor, scale);

            transform* local_transform = mx_actor_find(actor, transform);
            local_transform->matrix = mx_mat4_mul(mx_translate(*p), mx_mat4_mul(mx_quat_mat4(*r), mx_scale(*s)));

            const transform* parent_transform = NULL;
            if (local_transform->parent.id != MX_INVALID_ACTOR_ID) {
                parent_transform = mx_actor_find(local_transform->parent, transform);
            }

            mx_mat4 global_transform_mtx = local_transform->matrix;
            if (parent_transform) {
                global_transform_mtx = mx_mat4_mul(parent_transform->matrix, local_transform->matrix);
            }

            static_mesh_renderer* sm = mx_actor_find(actor, static_mesh_renderer);
            if (sm) {
                sm->transfom_matrix = global_transform_mtx;
            }
        }
    }

    // Camera update
    {
        scene_data.camera_position = g_example_camera.position;

        mgfx_buffer_update(
            forward_pass.pass_uniforms[FORWARD_PASS_SCENE_DATA_UNIFORM].ubh.idx,
            &scene_data,
            sizeof(scene_data),
            0);
    }

    mx_darray_t(directional_light) dir_lights = mx_query_view(directional_light);
    if (dir_lights) {
        for (size_t i = 0; i < MX_DARRAY_COUNT(&dir_lights); i++) {

            // Angle of rotation per frame (adjust to control speed)
            float delta_angle = 0.01f; // radians per frame

            // Axis to rotate around (e.g., X axis to go from morning to evening)
            mx_vec3 axis = MX_VEC3_RIGHT; // (1, 0, 0)

            if (mgfx_get_key(GLFW_KEY_E)) {
                // Rotate forward (morning -> noon -> evening)
                mx_mat4 rot = mx_mat4_rotate_euler(delta_angle, axis);
                dir_lights[i].direction = mx_mat_mul_vec4(rot,
                                                          (mx_vec4){
                                                              dir_lights[i].direction.x,
                                                              dir_lights[i].direction.y,
                                                              dir_lights[i].direction.z,
                                                              1.0f,
                                                          })
                                              .xyz;
            }

            if (mgfx_get_key(GLFW_KEY_R)) {
                // Rotate backward (evening -> noon -> morning)
                mx_mat4 rot = mx_mat4_rotate_euler(-delta_angle, axis);
                dir_lights[i].direction = mx_mat_mul_vec4(rot,
                                                          (mx_vec4){
                                                              dir_lights[i].direction.x,
                                                              dir_lights[i].direction.y,
                                                              dir_lights[i].direction.z,
                                                              1.0f,
                                                          })
                                              .xyz;
            }

            dir_lights[i].direction = mx_vec3_norm(dir_lights[i].direction);
            dir_lights[i].distance = 10.0f;

            mx_vec3 position =
                mx_vec3_scale(dir_lights[i].direction, -dir_lights[i].distance);

            mx_vec3 right = mx_vec3_cross(dir_lights->direction, MX_VEC3_UP);
            right = mx_vec3_norm(right);

            mx_vec3 look_at = mx_vec3_add(position, dir_lights[i].direction);
            mx_mat4 view = mx_look_at(position, look_at, MX_VEC3_UP);

            real_t ortho_s = 25.0f;
            real_t inverse_fov = (9.0f / 16.0f);
            mx_mat4 proj = mx_ortho(-ortho_s,
                                    ortho_s,
                                    inverse_fov * -ortho_s,
                                    inverse_fov * ortho_s,
                                    -0.01f,
                                    1000.0f);
            dir_lights[i].light_space_matrix = mx_mat4_mul(proj, view);
            mgfx_buffer_update(
                forward_pass.pass_uniforms[FORWARD_PASS_DIRECTIONAL_LIGHT_UNIFORM]
                    .ubh.idx,
                &dir_lights[i],
                sizeof(directional_light),
                0);

            // ----- Shadow pass -----
            if (render_pass_begin(&shadow_pass) == MX_SUCCESS) {
                mgfx_set_proj(proj.val);
                mgfx_set_view(view.val);

                mx_darray_t(static_mesh_renderer) sms =
                    mx_query_view(static_mesh_renderer);

                for (size_t i = 0; i < MX_DARRAY_COUNT(&sms); i++) {
                    if (render_pass_bind_descriptors(&shadow_pass)) {
                        mgfx_set_transform(sms[i].transfom_matrix.val);

                        mgfx_bind_vertex_buffer(sms[i].vbh);
                        mgfx_bind_index_buffer(sms[i].ibh);

                        mgfx_submit(SHADOW_FRAME_TARGET, shadow_ph);
                    }
                }

                render_pass_end(&shadow_pass);
            }

            // Only one dir light each scene
            break;
        }
    };

    // ----- Forward pass -----
    if (render_pass_begin(&forward_pass) == MX_SUCCESS) {
        mx_darray_t(static_mesh_renderer) sms = mx_query_view(static_mesh_renderer);

        for (size_t i = 0; i < MX_DARRAY_COUNT(&sms); i++) {
			mgfx_set_proj(g_example_camera.proj.val);
			mgfx_set_view(g_example_camera.view.val);

            if (render_pass_bind_descriptors(&forward_pass)) {
                mgfx_set_transform(sms[i].transfom_matrix.val);

                mgfx_bind_descriptor(sms[i].u_properties);

                for (uint32_t texidx = 0; texidx < sms[i].texture_count; texidx++) {
                    mgfx_bind_descriptor(sms[i].textures[texidx]);
                }

                mgfx_bind_vertex_buffer(sms[i].vbh);
                mgfx_bind_index_buffer(sms[i].ibh);

                mgfx_submit(FORWARD_FRAME_TARGET, sms[i].ph);
            }
        }

        render_pass_end(&forward_pass);
    }

    // ----- Post Processing -----

    // Blur pass
    for (uint32_t i = 0; i < BLUR_PASS_COUNT; i++) {
        if (blit_pass_begin(&blur_pingpong_pass[i]) == MX_SUCCESS) {
            mgfx_set_transform(MX_MAT4_IDENTITY.val);
            mgfx_submit(BLUR_0_FRAME_TARGET + i, blur_pingpong_pass[i].ph);
            blit_pass_end(&blur_pingpong_pass[i]);
        }
    }

    // ----- HDR, Gamma correction, Post Process Resolution -----
    if (blit_pass_begin(&post_process_resolve_pass) == MX_SUCCESS) {
        mgfx_set_transform(MX_MAT4_IDENTITY.val);
        mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, post_process_resolve_pass.ph);
        blit_pass_end(&post_process_resolve_pass);
    }
}

void mgfx_example_shutdown() {
    mgfx_buffer_destroy(quad_vbh.idx);
    mgfx_buffer_destroy(quad_ibh.idx);

    mx_ecs_shutdown();
}

int main(int argc, char** argv) { mgfx_example_app(); }
