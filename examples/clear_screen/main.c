#include "ex_common.h"

ShaderVk vertex_shader;
ShaderVk fragment_shader;

ShaderVk compute_shader;
VkDescriptorSet compute_ds_set;
ProgramVk compute_program;

TextureVk color_attachment;

int width = 720;
int height = 480;
void mgfx_example_init() {
	load_shader_from_path("assets/shaders/triangle.vert.spv", &vertex_shader);
	load_shader_from_path("assets/shaders/triangle.frag.spv", &fragment_shader);
	load_shader_from_path("assets/shaders/gradient.comp.spv", &compute_shader);

	texture_create_2d(720, 480,
		   VK_FORMAT_R16G16B16A16_SFLOAT,
		   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		   &color_attachment);

	VkImageView color_attachment_view = texture_get_view(VK_IMAGE_ASPECT_COLOR_BIT, &color_attachment);
	program_create_compute(&compute_shader, &compute_program);

	VkDescriptorImageInfo image_info = {
	    .sampler = VK_NULL_HANDLE,
	    .imageView = color_attachment_view,
	    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	};

	program_create_descriptor_sets(&compute_program, NULL, &image_info, &compute_ds_set);
}

void mgfx_example_updates(const DrawCtx* ctx) {
	VkClearColorValue clear = { .float32 = {0.0f, 1.0f, 0.0f, 1.0f} };
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	vk_cmd_transition_image(ctx->cmd,
			 &color_attachment.image,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vk_cmd_clear_image(ctx->cmd,
		    &color_attachment.image,
		    &range,
		    &clear);

	vk_cmd_transition_image(ctx->cmd,
			 &color_attachment.image,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 VK_IMAGE_LAYOUT_GENERAL);

	vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_program.pipeline);

	vkCmdBindDescriptorSets(ctx->cmd,
			 VK_PIPELINE_BIND_POINT_COMPUTE,
			 compute_program.layout, 0, 1, &compute_ds_set, 0, NULL);

	vkCmdDispatch(ctx->cmd, (int)width / 16, (int)height / 16, 1);

	vk_cmd_transition_image(ctx->cmd,
			&color_attachment.image,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	vk_cmd_transition_image(ctx->cmd,
			 ctx->frame_target,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vk_cmd_copy_image_to_image(ctx->cmd,
			    &color_attachment.image,
			    VK_IMAGE_ASPECT_COLOR_BIT,
			    ctx->frame_target);
}

void mgfx_example_shutdown() {
	texture_destroy(&color_attachment);
		
	shader_destroy(&compute_shader);
	shader_destroy(&fragment_shader);
	shader_destroy(&vertex_shader);
}

int main() {
	mgfx_example_app();
}
