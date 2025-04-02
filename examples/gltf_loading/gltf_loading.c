#include <ex_common.h>
#include "gltf_loader.h"

#include <mx/mx_log.h>
#include <GLFW/glfw3.h>

#include <mx/mx_memory.h>
#include <mx/mx_file.h>

// Frame buffer
image_vk forward_pass_color_attachment;
image_vk forward_pass_depth_attachment;
framebuffer_vk forward_pass_fb;

shader_vk forward_pass_vs;
shader_vk forward_pass_fs;
program_vk forward_pass_program;

mgfx_scene gltf;

void mgfx_example_init() {
    image_create(APP_WIDTH, APP_HEIGHT, VK_FORMAT_R16G16B16A16_SFLOAT, 1,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                 0, &forward_pass_color_attachment);

    image_create(APP_WIDTH, APP_HEIGHT, VK_FORMAT_D32_SFLOAT, 1,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &forward_pass_depth_attachment);
    framebuffer_create(1, &forward_pass_color_attachment, &forward_pass_depth_attachment, &forward_pass_fb);

    load_shader_from_path("assets/shaders/lit.vert.glsl.spv", &forward_pass_vs);
    load_shader_from_path("assets/shaders/lit.frag.glsl.spv", &forward_pass_fs);

    program_create_graphics(&forward_pass_vs, &forward_pass_fs, &forward_pass_fb, &forward_pass_program);

    mgfx_vertex_layout vl;

    vertex_layout_begin(&vl);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TANGENT, sizeof(float) * 4);
    vertex_layout_end(&vl);

    gltf.vl = &vl;

    /*LOAD_GLTF_MODEL("Lantern", &gltf);*/
    LOAD_GLTF_MODEL("DamagedHelmet", &forward_pass_program, gltf_loader_flag_default, &gltf);
    /*LOAD_GLTF_MODEL("Sponza", &forward_pass_program, gltf_loader_flag_default, &gltf);*/
    /*LOAD_GLTF_MODEL("conan_1", &gltf);*/
    //LOAD_GLTF_MODEL("MetalRoughSpheresNoTextures", &gltf);
    /*LOAD_GLTF_MODEL("MetalRoughSpheres", &gltf);*/
}

void mgfx_example_updates(const draw_ctx* ctx) {
    VkClearColorValue clear = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_transition_image(ctx->cmd, &forward_pass_color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_cmd_clear_image(ctx->cmd, &forward_pass_color_attachment, &range, &clear);

    vk_cmd_transition_image(ctx->cmd, &forward_pass_color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

    vk_cmd_begin_rendering(ctx->cmd, &forward_pass_fb);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, forward_pass_program.pipeline);

    static struct graphics_pc {
        mat4 model;
        mat4 view;
        mat4 proj;
        mat4 view_inv;
    } graphics_pc;

    glm_mat4_copy(g_example_camera.view, graphics_pc.view);
    glm_mat4_copy(g_example_camera.proj, graphics_pc.proj);
    glm_mat4_copy(g_example_camera.inverse_view, graphics_pc.view_inv);

    // Draw scene
    for(uint32_t n = 0; n < gltf.node_count; n++) {
        if(gltf.nodes[n].mesh == NULL) {
            continue;
        }

        assert(gltf.nodes[n].mesh->primitive_count > 0);

        glm_mat4_identity(graphics_pc.model);
        glm_mat4_mul(gltf.nodes[n].matrix, graphics_pc.model, graphics_pc.model);

        vec3 position = {0.0f, 0.0f, 0.0f};
        glm_translate(graphics_pc.model, position);

        static vec3 up = {0.0, 1.0, 0.0};
        static float y_rotation = 0;
        if(mgfx_get_key(GLFW_KEY_RIGHT)) {
            y_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if(mgfx_get_key(GLFW_KEY_LEFT)) {
            y_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(graphics_pc.model, y_rotation, up);

        static vec3 right = {1.0, 0.0, 0.0};
        static float z_rotation = 0;
        if(mgfx_get_key(GLFW_KEY_UP)) {
            z_rotation += MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        } else if(mgfx_get_key(GLFW_KEY_DOWN)) {
            z_rotation -= MGFX_TIME_DELTA_TIME * 3.141593 / 5.0f;
        }
        glm_rotate(graphics_pc.model, z_rotation, right);

        vec3 uniform_scale = {1.0, 1.0, 1.0};
        glm_vec3_scale(uniform_scale, 1.0, uniform_scale);
        glm_scale(graphics_pc.model, uniform_scale);

        for(uint32_t p = 0; p < gltf.nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &gltf.nodes[n].mesh->primitives[p];

            if(node_primitive->index_count <= 0) {
                continue;
            }

            VkDeviceSize offsets = 0;
            vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &node_primitive->vb.handle, &offsets);
            vkCmdBindIndexBuffer(ctx->cmd, node_primitive->ib.handle, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                    forward_pass_program.layout, 0, 1, &node_primitive->material->ds, 0, NULL);

            vkCmdPushConstants(ctx->cmd, forward_pass_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(graphics_pc), &graphics_pc);
            vkCmdDrawIndexed(ctx->cmd, node_primitive->index_count, 1, 0, 0, 0);
        }
    }

    vk_cmd_end_rendering(ctx->cmd);

    vk_cmd_transition_image(ctx->cmd, &forward_pass_color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_transition_image(ctx->cmd, ctx->frame_target->color_attachments[0],
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_copy_image_to_image(ctx->cmd,
                               &forward_pass_color_attachment,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               ctx->frame_target->color_attachments[0]);
}

void mgfx_example_shutdown() {
    image_destroy(&forward_pass_depth_attachment);
    image_destroy(&forward_pass_color_attachment);

    scene_destroy(&gltf);

    framebuffer_destroy(&forward_pass_fb);

    program_destroy(&forward_pass_program);
    shader_destroy(&forward_pass_fs);
    shader_destroy(&forward_pass_vs);
}

int main(int argc, char** argv) {
    mgfx_example_app();
}
