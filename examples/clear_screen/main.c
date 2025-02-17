#include "ex_common.h"
#include <vulkan/vulkan_core.h>

ShaderVk vertex_shader;
ShaderVk fragment_shader;

TextureVk color_attachment;

void mgfx_example_init() {
	load_shader_from_path("assets/shaders/triangle.vert.spv", &vertex_shader);
	load_shader_from_path("assets/shaders/triangle.frag.spv", &fragment_shader);

	texture_create_2d(720, 480,
		   VK_FORMAT_R16G16B16A16_SFLOAT,
		   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT	|
		   VK_IMAGE_USAGE_TRANSFER_DST_BIT	|
		   VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		   &color_attachment);
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
	// TODO: Usage based deletion queue.
	texture_destroy(&color_attachment);

	shader_destroy(&fragment_shader);
	shader_destroy(&vertex_shader);
}

int main() {
	mgfx_example_app();
}
