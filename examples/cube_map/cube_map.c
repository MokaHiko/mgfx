#include <ex_common.h>
#include <gltf_loading/gltf_loader.h>

#include <mx/mx_log.h>
#include <GLFW/glfw3.h>

#include <mx/mx_memory.h>
#include <mx/mx_file.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

// Frame buffer
image_vk color_attachment;
image_vk depth_attachment;
framebuffer_vk mesh_pass_fb;

shader_vk vertex_shader;
shader_vk fragment_shader;
program_vk gfx_program;

shader_vk skybox_vertex_shader;
shader_vk skybox_fragment_shader;
program_vk skybox_program;

texture_vk skybox_cube_map;
mgfx_scene skybox_scene;

descriptor_vk skybox_cube_map_descriptor;
VkDescriptorSet skybox_cube_map_ds;

mgfx_scene lantern_scene;

void mgfx_example_init() {
    image_create(APP_WIDTH, APP_HEIGHT, VK_FORMAT_R16G16B16A16_SFLOAT, 1,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0,
                    &color_attachment);

    image_create(APP_WIDTH, APP_HEIGHT, VK_FORMAT_D32_SFLOAT, 1,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &depth_attachment);
    framebuffer_create(1, &color_attachment, &depth_attachment, &mesh_pass_fb);

    // Load cube map
    load_shader_from_path("assets/shaders/skybox.vert.glsl.spv", &skybox_vertex_shader);
    load_shader_from_path("assets/shaders/skybox.frag.glsl.spv", &skybox_fragment_shader);

    program_create_graphics(&skybox_vertex_shader,
                            &skybox_fragment_shader,
                            &mesh_pass_fb,
                            &skybox_program);

    load_texture_2d_from_path;
    texture_create_3d(1080, 1080, VK_FORMAT_R8G8B8A8_UNORM, VK_FILTER_NEAREST, &skybox_cube_map);
    //image_update(size_t size, const void *data, image_vk *image)

    program_create_descriptor_set(&skybox_program, &skybox_cube_map_ds);
    descriptor_texture_create(&skybox_cube_map, &skybox_cube_map_descriptor);
    descriptor_write(skybox_cube_map_ds, 0, &skybox_cube_map_descriptor);

    mgfx_vertex_layout skybox_vl;

    vertex_layout_begin(&skybox_vl);
    vertex_layout_add(&skybox_vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
    vertex_layout_end(&skybox_vl);

    skybox_scene.vl = &skybox_vl;

    load_scene_from_path("/Users/christianmarkg.solon/Dev/mx_app/mgfx/examples/assets/models/cube.gltf",
                         &skybox_program, gltf_loader_flag_meshes | gltf_loader_flag_flip_winding, &skybox_scene);

    // Laod scenes
    load_shader_from_path("assets/shaders/lit.vert.glsl.spv", &vertex_shader);
    load_shader_from_path("assets/shaders/lit.frag.glsl.spv", &fragment_shader);

    program_create_graphics(&vertex_shader,
                            &fragment_shader,
                            &mesh_pass_fb,
                            &gfx_program);

    mgfx_vertex_layout pbr_vl;
    vertex_layout_begin(&pbr_vl);
        vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
        vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
        vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
        vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
        vertex_layout_add(&pbr_vl, MGFX_VERTEX_ATTRIBUTE_TANGENT, sizeof(float) * 4);
    vertex_layout_end(&pbr_vl);

    lantern_scene.vl = &pbr_vl;
    LOAD_GLTF_MODEL("Lantern", &gfx_program, gltf_loader_flag_default, &lantern_scene);
    /*LOAD_GLTF_MODEL("DamagedHelmet", &gfx_program, &gltf);*/
}

void mgfx_example_updates(const DrawCtx* ctx) {
    VkClearColorValue clear = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_transition_image(ctx->cmd, &color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_cmd_clear_image(ctx->cmd, &color_attachment, &range, &clear);

    vk_cmd_transition_image(ctx->cmd, &color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

    vk_cmd_begin_rendering(ctx->cmd, &mesh_pass_fb);
    static struct graphics_pc {
        mat4 model;
        mat4 view;
        mat4 proj;
        mat4 view_inv;
    } graphics_pc;

    glm_mat4_copy(g_example_camera.view, graphics_pc.view);
    glm_mat4_copy(g_example_camera.proj, graphics_pc.proj);
    glm_mat4_copy(g_example_camera.inverse_view, graphics_pc.view_inv);

    // Draw skybox
    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_program.pipeline);

    VkDeviceSize offsets = 0;

    const struct primitive* cube = &skybox_scene.meshes[0].primitives[0];
    vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &cube->vb.handle, &offsets);
    vkCmdBindIndexBuffer(ctx->cmd, cube->ib.handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skybox_program.layout, 0, 1, &skybox_cube_map_ds, 0, NULL);

    vkCmdPushConstants(ctx->cmd, gfx_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(graphics_pc), &graphics_pc);
    vkCmdDrawIndexed(ctx->cmd, cube->index_count, 1, 0, 0, 0);

    // Draw scene
    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_program.pipeline);
    for(uint32_t n = 0; n < lantern_scene.node_count; n++) {
        if(lantern_scene.nodes[n].mesh == NULL) {
            continue;
        }

        assert(lantern_scene.nodes[n].mesh->primitive_count > 0);

        glm_mat4_identity(graphics_pc.model);
        glm_mat4_mul(lantern_scene.nodes[n].matrix, graphics_pc.model, graphics_pc.model);

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

        for(uint32_t p = 0; p < lantern_scene.nodes[n].mesh->primitive_count; p++) {
            const struct primitive* node_primitive = &lantern_scene.nodes[n].mesh->primitives[p];

            if(node_primitive->index_count <= 0) {
                continue;
            }

            VkDeviceSize offsets = 0;
            vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &node_primitive->vb.handle, &offsets);
            vkCmdBindIndexBuffer(ctx->cmd, node_primitive->ib.handle, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                    gfx_program.layout, 0, 1, &node_primitive->material->ds, 0, NULL);

            vkCmdPushConstants(ctx->cmd, gfx_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(graphics_pc), &graphics_pc);
            vkCmdDrawIndexed(ctx->cmd, node_primitive->index_count, 1, 0, 0, 0);
        }
    }

    vk_cmd_end_rendering(ctx->cmd);

    vk_cmd_transition_image(ctx->cmd, &color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_transition_image(ctx->cmd, ctx->frame_target->color_attachments[0],
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_copy_image_to_image(ctx->cmd,
                               &color_attachment,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               ctx->frame_target->color_attachments[0]);
}

void mgfx_example_shutdown() {
    image_destroy(&depth_attachment);
    image_destroy(&color_attachment);

    scene_destroy(&lantern_scene);

    framebuffer_destroy(&mesh_pass_fb);

    scene_destroy(&skybox_scene);
    texture_destroy(&skybox_cube_map);

    shader_destroy(&skybox_vertex_shader);
    shader_destroy(&skybox_fragment_shader);
    program_destroy(&skybox_program);

    program_destroy(&gfx_program);
    shader_destroy(&fragment_shader);
    shader_destroy(&vertex_shader);
}

int main(int argc, char** argv) {
    mgfx_example_app();
}
