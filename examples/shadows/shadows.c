#include <ex_common.h>
#include <gltf_loading/gltf_loader.h>

#include <GLFW/glfw3.h>
#include <mx/mx_log.h>

#include <mx/mx_file.h>
#include <mx/mx_memory.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

// Shadow pass
texture_vk shadow_pass_depth_attachment;
framebuffer_vk shadow_pass_fb;
camera shadow_pass_camera = {0};

// Forward pass
image_vk forward_pass_color_attachment;
image_vk forward_pass_depth_attachment;
framebuffer_vk forward_pass_fb;

shader_vk forward_pass_vs;
shader_vk forward_pass_fs;
program_vk pbr_program;

VkDescriptorSet shadows_ds;
/*descriptor_vk u_shadow_map;*/

shader_vk shadow_pass_vs;
program_vk shadow_program;

typedef struct graphics_pc {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 view_inv;
} graphics_pc;

mgfx_scene lantern_scene;
mgfx_scene cube_scene;

void mgfx_example_init() {
    // Attachment create
    image_create(APP_WIDTH,
                 APP_HEIGHT,
                 VK_FORMAT_R16G16B16A16_SFLOAT,
                 1,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                 0,
                 &forward_pass_color_attachment);
    image_create(APP_WIDTH,
                 APP_HEIGHT,
                 VK_FORMAT_D32_SFLOAT,
                 1,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 0,
                 &forward_pass_depth_attachment);
    framebuffer_create(1, &forward_pass_color_attachment, &forward_pass_depth_attachment, &forward_pass_fb);

    // Attachment create as texture
    image_create(APP_WIDTH,
                 APP_HEIGHT,
                 VK_FORMAT_D32_SFLOAT,
                 1,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 0,
                 &shadow_pass_depth_attachment.image);
    image_create_view(&shadow_pass_depth_attachment.image,
                      VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_DEPTH_BIT,
                      &shadow_pass_depth_attachment.view);
    framebuffer_create(0, NULL, &shadow_pass_depth_attachment.image, &shadow_pass_fb);

    load_shader_from_path("assets/shaders/shadows.vert.glsl.spv", &shadow_pass_vs);
    program_create_graphics(&shadow_pass_vs, NULL, &shadow_pass_fb, &shadow_program);

    load_shader_from_path("assets/shaders/lit.vert.glsl.spv", &forward_pass_vs);
    load_shader_from_path("assets/shaders/lit.frag.glsl.spv", &forward_pass_fs);

    program_create_graphics(&forward_pass_vs, &forward_pass_fs, &forward_pass_fb, &pbr_program);

    camera_create(mgfx_camera_type_orthographic, &shadow_pass_camera);
    vec3 light_dir = {1.0, -1.0, 0.0};

    // glm_vec3_copy(light_dir, shadow_pass_camera.position);
    glm_normalize(light_dir);
    glm_vec3_copy(light_dir, shadow_pass_camera.forward);

    camera_update(&shadow_pass_camera);

    mgfx_vertex_layout pbr_vl;
    vertex_layout_begin(&pbr_vl);
    vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
    vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
    vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
    vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
    vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_TANGENT, sizeof(float) * 4);
    vertex_layout_end(&pbr_vl);

    lantern_scene.vl = &pbr_vl;
    LOAD_GLTF_MODEL("Lantern", &pbr_program, gltf_loader_flag_default, &lantern_scene);

    cube_scene.vl = &pbr_vl;
    load_scene_from_path("/Users/christianmarkg.solon/Dev/mx_app/mgfx/examples/assets/models/cube.gltf",
                         &pbr_program,
                         gltf_loader_flag_default,
                         &cube_scene);

    program_create_descriptor_set(&pbr_program, &shadows_ds);
    /*descriptor_texture_create(&shadow_pass_depth_attachment, &u_shadow_map);*/
    /*descriptor_write(shadows_ds, 0, &u_shadow_map);*/
}

void scene_draw(const mgfx_scene* scene, const draw_ctx* ctx, const camera* cam) {
    graphics_pc pc = {0};
    glm_mat4_copy(g_example_camera.view, pc.view);
    glm_mat4_copy(g_example_camera.proj, pc.proj);
    glm_mat4_copy(g_example_camera.inverse_view, pc.view_inv);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_program.pipeline);

    for (uint32_t n = 0; n < lantern_scene.node_count; n++) {
        if (lantern_scene.nodes[n].mesh == NULL) {
            continue;
        }

        assert(lantern_scene.nodes[n].mesh->primitive_count > 0);

        glm_mat4_identity(pc.model);
        glm_mat4_mul(lantern_scene.nodes[n].matrix, pc.model, pc.model);

        vec3 position = {0.0f, 0.0f, 0.0f};
        glm_translate(pc.model, position);

        static vec3 up = {0.0, 1.0, 0.0};
        static float y_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_RIGHT)) {
            y_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if (mgfx_get_key(GLFW_KEY_LEFT)) {
            y_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(pc.model, y_rotation, up);

        static vec3 right = {1.0, 0.0, 0.0};
        static float z_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_UP)) {
            z_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if (mgfx_get_key(GLFW_KEY_DOWN)) {
            z_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(pc.model, z_rotation, right);

        vec3 uniform_scale = {1.0, 1.0, 1.0};
        glm_vec3_scale(uniform_scale, 1.0, uniform_scale);
        glm_scale(pc.model, uniform_scale);

        for (uint32_t p = 0; p < lantern_scene.nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &lantern_scene.nodes[n].mesh->primitives[p];

            if (node_primitive->index_count <= 0) {
                continue;
            }

            VkDeviceSize offsets = 0;
            vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &node_primitive->vb.handle, &offsets);
            vkCmdBindIndexBuffer(ctx->cmd, node_primitive->ib.handle, 0, VK_INDEX_TYPE_UINT32);

            // Material ds
            vkCmdBindDescriptorSets(ctx->cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pbr_program.layout,
                                    0,
                                    1,
                                    &node_primitive->material->ds,
                                    0,
                                    NULL);

            // Shadow ds
            vkCmdBindDescriptorSets(
                ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_program.layout, 0, 1, &shadows_ds, 0, NULL);

            vkCmdPushConstants(ctx->cmd, pbr_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
            vkCmdDrawIndexed(ctx->cmd, node_primitive->index_count, 1, 0, 0, 0);
        }
    }
}

void mgfx_example_updates(const draw_ctx* ctx) {
    VkClearColorValue clear = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_transition_image(ctx->cmd,
                            &forward_pass_color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_cmd_clear_image(ctx->cmd, &forward_pass_color_attachment, &range, &clear);

    vk_cmd_transition_image(
        ctx->cmd, &forward_pass_color_attachment, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

    // Shadow pass draw
    vk_cmd_begin_rendering(ctx->cmd, &shadow_pass_fb);

    graphics_pc shadow_pc = {0};
    glm_mat4_copy(shadow_pass_camera.view, shadow_pc.view);
    glm_mat4_copy(shadow_pass_camera.proj, shadow_pc.proj);
    glm_mat4_copy(shadow_pass_camera.inverse_view, shadow_pc.view_inv);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_program.pipeline);

    for (uint32_t n = 0; n < lantern_scene.node_count; n++) {
        if (lantern_scene.nodes[n].mesh == NULL) {
            continue;
        }

        assert(lantern_scene.nodes[n].mesh->primitive_count > 0);

        glm_mat4_identity(shadow_pc.model);
        glm_mat4_mul(lantern_scene.nodes[n].matrix, shadow_pc.model, shadow_pc.model);

        vec3 position = {0.0f, 0.0f, 0.0f};
        glm_translate(shadow_pc.model, position);

        static vec3 up = {0.0, 1.0, 0.0};
        static float y_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_RIGHT)) {
            y_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if (mgfx_get_key(GLFW_KEY_LEFT)) {
            y_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(shadow_pc.model, y_rotation, up);

        static vec3 right = {1.0, 0.0, 0.0};
        static float z_rotation = 0;
        if (mgfx_get_key(GLFW_KEY_UP)) {
            z_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if (mgfx_get_key(GLFW_KEY_DOWN)) {
            z_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(shadow_pc.model, z_rotation, right);

        vec3 uniform_scale = {1.0, 1.0, 1.0};
        glm_vec3_scale(uniform_scale, 1.0, uniform_scale);
        glm_scale(shadow_pc.model, uniform_scale);

        for (uint32_t p = 0; p < lantern_scene.nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &lantern_scene.nodes[n].mesh->primitives[p];

            if (node_primitive->index_count <= 0) {
                continue;
            }

            VkDeviceSize offsets = 0;
            vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &node_primitive->vb.handle, &offsets);
            vkCmdBindIndexBuffer(ctx->cmd, node_primitive->ib.handle, 0, VK_INDEX_TYPE_UINT32);

            vkCmdPushConstants(
                ctx->cmd, pbr_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadow_pc), &shadow_pc);
            vkCmdDrawIndexed(ctx->cmd, node_primitive->index_count, 1, 0, 0, 0);
        }
    }

    vk_cmd_end_rendering(ctx->cmd);

    // Forward pass draw
    vk_cmd_begin_rendering(ctx->cmd, &forward_pass_fb);
    scene_draw(&lantern_scene, ctx, &g_example_camera);
    vk_cmd_end_rendering(ctx->cmd);

    vk_cmd_transition_image(ctx->cmd,
                            &forward_pass_color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_transition_image(ctx->cmd,
                            ctx->frame_target->color_attachments[0],
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_copy_image_to_image(ctx->cmd,
                               &forward_pass_color_attachment,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               ctx->frame_target->color_attachments[0]);
}

void mgfx_example_shutdown() {
    scene_destroy(&cube_scene);
    scene_destroy(&lantern_scene);

    image_destroy(&forward_pass_depth_attachment);
    image_destroy(&forward_pass_color_attachment);

    program_destroy(&pbr_program);
    shader_destroy(&forward_pass_fs);
    shader_destroy(&forward_pass_vs);
    framebuffer_destroy(&forward_pass_fb);

    texture_destroy(&shadow_pass_depth_attachment);
    program_destroy(&shadow_program);
    shader_destroy(&shadow_pass_vs);
    framebuffer_destroy(&shadow_pass_fb);
}

int main(int argc, char** argv) { mgfx_example_app(); }
