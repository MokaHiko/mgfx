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

#include <string.h>

typedef struct directional_light {
    struct {
        float light_space_matrix[16];
        mx_vec3 direction;
        float distance;
        mx_vec4 color;
    } data;

    camera camera;
} directional_light;

typedef struct point_light {
    float position[4];

    float color[3];
    float intensity;
} point_light;

directional_light sun_light = {.data = {.direction = {3.0f, -8.0f, 3.0f},
                                        .distance = 1.0f,
                                        .color = {100, 100, 100, 100.0f},
                                        .light_space_matrix = {0.0f}}};
mgfx_ubh sun_light_buffer;
mgfx_dh u_sun_light;

mgfx_imgh shadow_pass_dattachmen;
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

mgfx_sh blit_vsh, blit_fsh;
mgfx_ph blit_program;

mgfx_vbh quad_vbh;
mgfx_ibh quad_ibh;

mgfx_th mesh_pass_cattachment_th;
mgfx_dh u_mesh_pass_cattachment;

mgfx_scene sponza;
mgfx_scene helmet;
void mgfx_example_init() {
    // Common
    camera_create(mgfx_camera_type_orthographic, &sun_light.camera);

    sun_light.data.color.xyz = mx_vec3_norm(sun_light.data.color.xyz);

    sun_light_buffer = mgfx_uniform_buffer_create(&sun_light.data, sizeof(sun_light.data));
    u_sun_light = mgfx_descriptor_create("sun_light", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    mgfx_set_buffer(u_sun_light, sun_light_buffer);

    // Directional light shadow pass
    struct mgfx_image_info shadow_pass_depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH * 2,
        .height = APP_HEIGHT * 2,
        .layers = 1,
    };

    shadow_pass_dattachmen =
        mgfx_image_create(&shadow_pass_depth_attachment_info,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    shadow_pass_fbh = mgfx_framebuffer_create(NULL, 0, shadow_pass_dattachmen);
    mgfx_set_view_target(0, shadow_pass_fbh);

    shadow_pass_vs = mgfx_shader_create(MGFX_ASSET_PATH "shaders/shadows.vert.glsl.spv");

    const mgfx_graphics_ex_create_info shadow_pass_create_info = {
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .cull_mode = VK_CULL_MODE_FRONT_BIT,
    };
    shadow_pass_program = mgfx_program_create_graphics_ex(
        shadow_pass_vs, (mgfx_sh){.idx = 0}, &shadow_pass_create_info);

    // HDR Mesh pass
    struct mgfx_image_info color_attachment_info = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mesh_pass_color_attachment =
        mgfx_image_create(&color_attachment_info,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT);

    struct mgfx_image_info depth_attachment_info = {
        .format = VK_FORMAT_D32_SFLOAT,
        .width = APP_WIDTH,
        .height = APP_HEIGHT,
        .layers = 1,
    };

    mesh_pass_depth_attachment =
        mgfx_image_create(&depth_attachment_info, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    mesh_pass_fbh =
        mgfx_framebuffer_create(&mesh_pass_color_attachment, 1, mesh_pass_depth_attachment);
    mgfx_set_view_target(1, mesh_pass_fbh);
    mgfx_set_view_clear(1, (float[4]){0.0f, 0.0f, 0.0f, 1.0f});

    mesh_pass_vs = mgfx_shader_create(MGFX_ASSET_PATH "shaders/shadows_lit.vert.glsl.spv");
    mesh_pass_fs = mgfx_shader_create(MGFX_ASSET_PATH "shaders/shadows_lit.frag.glsl.spv");
    mesh_pass_program = mgfx_program_create_graphics(mesh_pass_vs, mesh_pass_fs);

    u_shadow_map = mgfx_descriptor_create("shadow_map", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    shadow_fba_texture = mgfx_texture_create_from_image(shadow_pass_dattachmen, VK_FILTER_LINEAR);
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
    blit_vsh = mgfx_shader_create(MGFX_ASSET_PATH "shaders/blit.vert.glsl.spv");
    blit_fsh = mgfx_shader_create(MGFX_ASSET_PATH "shaders/blit.frag.glsl.spv");
    blit_program = mgfx_program_create_graphics(blit_vsh, blit_fsh);

    quad_vbh = mgfx_vertex_buffer_create(MGFX_FS_QUAD_VERTICES, sizeof(MGFX_FS_QUAD_VERTICES));
    quad_ibh = mgfx_index_buffer_create(MGFX_FS_QUAD_INDICES, sizeof(MGFX_FS_QUAD_INDICES));

    u_mesh_pass_cattachment =
        mgfx_descriptor_create("diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    mesh_pass_cattachment_th =
        mgfx_texture_create_from_image(mesh_pass_color_attachment, VK_FILTER_LINEAR);
    mgfx_set_texture(u_mesh_pass_cattachment, mesh_pass_cattachment_th);
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

            mgfx_set_transform(scene->nodes[n].matrix.val);

            mgfx_bind_vertex_buffer(node_primitive->vbh);
            mgfx_bind_index_buffer(node_primitive->ibh);

            if (!is_shadow_pass) {
                mgfx_bind_descriptor(0, u_sun_light);
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
    static float distance = 10.0f;
    static float angle = 0;

    if (mgfx_get_key(GLFW_KEY_E)) {
        angle += 3.1415925f * MGFX_TIME_DELTA_TIME;
    }

    if (mgfx_get_key(GLFW_KEY_R)) {
        angle -= 3.1415925f * MGFX_TIME_DELTA_TIME;
    }

    // Transform the light direction
    mx_mat4 rotation_matrix = mx_mat4_rotate_euler(angle, (mx_vec3){1, 0, 1});

    static mx_vec4 start_dir = {1.0f, -1.0f, 0.0f, 1.0f};
    sun_light.data.direction = mx_mat_mul_vec4(rotation_matrix, start_dir).xyz;

    sun_light.data.direction = mx_vec3_norm(sun_light.data.direction);
    mgfx_buffer_update(sun_light_buffer.idx, &sun_light.data, sizeof(sun_light.data), 0);

    // Update object transforms
    mx_mat4 sponza_transform = mx_mat4_mul(mx_translate((mx_vec3){0.0f, 0.0f, 0.0f}),
                                           mx_scale((mx_vec3){1.0f, 1.0f, 1.0f}));

    mx_mat4 helmet_transform = mx_mat4_mul(mx_translate((mx_vec3){0.0f, 1.5f, 1.0f}),
                                           mx_scale((mx_vec3){1.0f, 1.0f, 1.0f}));

    // Draw shadow pass
    mx_vec3 neg_dir = mx_vec3_scale(sun_light.data.direction, -1.0f);

    mx_vec3 light_pos = mx_vec3_scale(mx_vec3_sub(neg_dir, MX_VEC3_ZERO), distance);

    sun_light.camera.position.x = light_pos.x;
    sun_light.camera.position.y = light_pos.y;
    sun_light.camera.position.z = light_pos.z;
    sun_light.camera.forward = sun_light.data.direction;
    camera_update(&sun_light.camera);

    memcpy(sun_light.data.light_space_matrix, sun_light.camera.view_proj.val, sizeof(float) * 16);

    mgfx_set_proj(sun_light.camera.proj.val);
    mgfx_set_view(sun_light.camera.view.val);

    draw_scene(&sponza, 0, shadow_pass_program, sponza_transform, MX_TRUE);
    draw_scene(&helmet, 0, shadow_pass_program, helmet_transform, MX_TRUE);

    // Draw mesh pass
    mgfx_set_proj(g_example_camera.proj.val);
    mgfx_set_view(g_example_camera.view.val);

    draw_scene(&sponza, 1, mesh_pass_program, sponza_transform, MX_FALSE);
    draw_scene(&helmet, 1, mesh_pass_program, helmet_transform, MX_FALSE);

    // Blit to backbuffer
    mgfx_bind_vertex_buffer(quad_vbh);
    mgfx_bind_index_buffer(quad_ibh);

    if (mgfx_get_key(GLFW_KEY_L)) {
        mgfx_bind_descriptor(0, u_shadow_map);
    } else {
        mgfx_bind_descriptor(0, u_mesh_pass_cattachment);
    }

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, blit_program);

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
                         (mx_quat)MX_QUAT_IDENTITY,
                         (mx_vec3)MX_VEC3_ONE);
}

void mgfx_example_shutdown() {
    scene_destroy(&helmet);
    scene_destroy(&sponza);

    // Blit pass resources
    mgfx_texture_destroy(mesh_pass_cattachment_th, MX_FALSE);
    mgfx_descriptor_destroy(u_mesh_pass_cattachment);

    mgfx_buffer_destroy(quad_vbh.idx);
    mgfx_buffer_destroy(quad_ibh.idx);

    mgfx_program_destroy(blit_program);
    mgfx_shader_destroy(blit_vsh);
    mgfx_shader_destroy(blit_fsh);

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

    mgfx_image_destroy(shadow_pass_dattachmen);
    mgfx_framebuffer_destroy(shadow_pass_fbh);

    // Common
    mgfx_descriptor_destroy(u_sun_light);
    mgfx_buffer_destroy(sun_light_buffer.idx);
}

int main(int argc, char** argv) { mgfx_example_app(); }
