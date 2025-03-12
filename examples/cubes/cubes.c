#include "ex_common.h"

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
    {{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},  // Top-right
    {{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Back face
    {{0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {1.0f, 0.0f, 1.0f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {0.0f, 1.0f, 1.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}, // Top-right 
    {{0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {0.5f, 0.5f, 0.5f, 1.0f}},  // Top-left

    // Left face
    {{-0.5f, -0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {0.0f, 0.5f, 1.0f, 1.0f}},  // Top-right
    {{-0.5f, 0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.5f, 1.0f}}, // Top-left

    // Right face
    {{0.5f, -0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.0f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {0.5f, 0.5f, 1.0f, 1.0f}},// Bottom right
    {{0.5f, 0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {0.5f, 0.0f, 1.0f, 1.0f}}, // Top-right
    {{0.5f, 0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // Top-left

    // Top face
    {{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.5f, 1.0f}},  // Bottom-right
    {{0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {0.0f, 1.0f, 1.0f, 1.0f}}, // Top-right
    {{-0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Bottom face
    {{-0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {1.0f, 0.0f, 0.5f, 1.0f}},  // Top-right
    {{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {0.5f, 0.5f, 1.0f, 1.0f}}, // Top-left
};
const size_t k_cube_vertex_count = sizeof(k_vertices) / sizeof(Vertex);

static uint32_t k_indices[] = {
    // Front face
    0, 1, 2, 0, 2, 3,
    // Back face
    4, 5, 6, 4, 6, 7,
    // Left face
    8, 9, 10, 8, 10, 11,
    // Right face
    12, 13, 14, 12, 14, 15,
    // Top face
    16, 17, 18, 16, 18, 19,
    // Bottom face
    20, 21, 22, 20, 22, 23
};
const size_t k_cube_index_count = sizeof(k_indices) / sizeof(uint32_t);

// Frame buffer.
image_vk color_attachment;
framebuffer_vk mesh_pass_fb;

shader_vk vertex_shader;
shader_vk fragment_shader;
program_vk gfx_program;

vertex_buffer_vk vertex_buffer;
index_buffer_vk index_buffer;

int width  = APP_WIDTH;
int height = APP_HEIGHT;
void mgfx_example_init() {
    image_create(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, 1,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0,
                      &color_attachment);

    framebuffer_create(1, &color_attachment, NULL, &mesh_pass_fb);

    load_shader_from_path("assets/shaders/unlit.vert.glsl.spv", &vertex_shader);
    load_shader_from_path("assets/shaders/unlit.frag.glsl.spv", &fragment_shader);
    program_create_graphics(&vertex_shader, &fragment_shader, &mesh_pass_fb, &gfx_program);

    vertex_buffer_create(k_cube_vertex_count * sizeof(Vertex), k_vertices, &vertex_buffer);
    index_buffer_create(k_cube_index_count * sizeof(uint32_t), k_indices, &index_buffer);
}

void mgfx_example_updates(const DrawCtx* ctx) {
    VkClearColorValue clear       = {.float32 = {0.6f, 0.5f, 0.2f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_transition_image(ctx->cmd, &color_attachment, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_cmd_clear_image(ctx->cmd, &color_attachment, &range, &clear);

    static struct ComputePC {
        float time;
    } pc;
    pc.time += 0.001;

    vk_cmd_begin_rendering(ctx->cmd, &mesh_pass_fb);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_program.pipeline);

    static struct graphics_pc {
        mat4 model;
        mat4 view;
        mat4 proj;
    } graphics_pc;
    glm_perspective(glm_rad(60.0), 16.0 / 9.0, 0.1f, 10000.0f, graphics_pc.proj);
    graphics_pc.proj[1][1] *= -1;

    {
        vec3 position = {0.0f, 0.0f, 5.0f};
        vec3 forward  = {0.0f, 0.0f, 0.0f};
        vec3 up       = {0.0f, 1.0f, 0.0f};
        glm_lookat(position, forward, up, graphics_pc.view);
    }

    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &vertex_buffer.handle, &offsets);
    vkCmdBindIndexBuffer(ctx->cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    for (int x = -2; x < 3; x++) {
        vec3 position = {x * 2.0f, 0.0f, 0.0f};
        glm_mat4_identity(graphics_pc.model);

        glm_translate(graphics_pc.model, position);

        vec3 axis = {1.0, 1.0, 1.0};
        glm_rotate(graphics_pc.model, pc.time * 3.141593, axis);

        vkCmdPushConstants(ctx->cmd, gfx_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(graphics_pc), &graphics_pc);

        vkCmdDrawIndexed(ctx->cmd, k_cube_index_count, 1, 0, 0, 0);
    }

    vk_cmd_end_rendering(ctx->cmd);

    vk_cmd_transition_image(ctx->cmd, &color_attachment, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_transition_image(ctx->cmd, ctx->frame_target->color_attachments[0], VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_copy_image_to_image(ctx->cmd, &color_attachment, VK_IMAGE_ASPECT_COLOR_BIT,
                               ctx->frame_target->color_attachments[0]);
}

void mgfx_example_shutdown() {
    framebuffer_destroy(&mesh_pass_fb);
    image_destroy(&color_attachment);

    buffer_destroy(&vertex_buffer);
    buffer_destroy(&index_buffer);

    program_destroy(&gfx_program);
    shader_destroy(&fragment_shader);
    shader_destroy(&vertex_shader);
}

int main() { mgfx_example_app(); }
