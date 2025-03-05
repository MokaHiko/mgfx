#include "ex_common.h"

shader_vk compute_shader;
VkDescriptorSet compute_ds_set;
program_vk compute_program;

int width  = 720;
int height = 480;

void mgfx_example_init() {
    load_shader_from_path("assets/shaders/gradient.comp.glsl.spv", &compute_shader);

    program_create_compute(&compute_shader, &compute_program);

    /*VkDescriptorImageInfo image_info = {*/
    /*    .sampler = VK_NULL_HANDLE,*/
    /*    .imageView = mesh_pass_fb.color_attachment_views[0],*/
    /*    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,*/
    /*};*/
    /*program_create_descriptor_sets(&compute_program, NULL, &image_info, &compute_ds_set);*/
}

void mgfx_example_updates(const DrawCtx *ctx) {
    VkClearColorValue clear       = {.float32 = {0.6f, 0.5f, 0.2f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_clear_image(ctx->cmd, ctx->frame_target, &range, &clear);

    /*vk_cmd_transition_image(ctx->cmd,*/
    /*		 ctx->frame_target,*/
    /*		 VK_IMAGE_ASPECT_COLOR_BIT,*/
    /*		 VK_IMAGE_LAYOUT_GENERAL);*/
    /**/
    /*vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_program.pipeline);*/
    /**/
    /*static struct ComputePC {*/
    /*	float time;*/
    /*} pc;*/
    /*pc.time += 0.001;*/
    /*vkCmdPushConstants(ctx->cmd, compute_program.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
     * sizeof(pc), &pc);*/
    /**/
    /*vkCmdBindDescriptorSets(ctx->cmd,*/
    /*		 VK_PIPELINE_BIND_POINT_COMPUTE,*/
    /*		 compute_program.layout, 0, 1, &compute_ds_set, 0, NULL);*/
    /*vkCmdDispatch(ctx->cmd, (int)width / 16, (int)height / 16, 1);*/
}

void mgfx_example_shutdown() {
    program_destroy(&compute_program);
    shader_destroy(&compute_shader);
}

int main() { mgfx_example_app(); }
