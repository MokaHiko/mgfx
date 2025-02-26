#include "ex_common.h"

shader_vk vertex_shader;
shader_vk fragment_shader;
program_vk gfx_program;

shader_vk compute_shader;
VkDescriptorSet compute_ds_set;
program_vk compute_program;

texture_vk color_attachment;
VkImageView color_attachment_view;

int width = 720;
int height = 480;
void mgfx_example_init() {
	load_shader_from_path("assets/shaders/triangle.vert.glsl.spv", &vertex_shader);
	load_shader_from_path("assets/shaders/triangle.frag.glsl.spv", &fragment_shader);
	program_create_graphics(&vertex_shader, &fragment_shader, &gfx_program);

	load_shader_from_path("assets/shaders/gradient.comp.glsl.spv", &compute_shader);
	program_create_compute(&compute_shader, &compute_program);

	texture_create_2d(720, 480,
		   VK_FORMAT_R16G16B16A16_SFLOAT,
		   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		   VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		   &color_attachment);
	color_attachment_view = texture_get_view(VK_IMAGE_ASPECT_COLOR_BIT, &color_attachment);

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

	static struct ComputePC {
		float time;
	} pc;
	pc.time += 0.001;
	vkCmdPushConstants(ctx->cmd, compute_program.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	vkCmdBindDescriptorSets(ctx->cmd,
			 VK_PIPELINE_BIND_POINT_COMPUTE,
			 compute_program.layout, 0, 1, &compute_ds_set, 0, NULL);

	vkCmdDispatch(ctx->cmd, (int)width / 16, (int)height / 16, 1);

	vk_cmd_transition_image(ctx->cmd,
			&color_attachment.image,
			 VK_IMAGE_ASPECT_COLOR_BIT,
			 VK_IMAGE_LAYOUT_GENERAL);

	VkRenderingAttachmentInfo color_attachment_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = NULL,
		.imageView = color_attachment_view,
		.imageLayout = color_attachment.image.layout,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.resolveImageView = VK_NULL_HANDLE,
		.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};

	VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    		.pNext = NULL,
    		.flags = 0,
    		.renderArea = {.offset = {0, 0}, .extent = {width, height}},
    		.layerCount = 1,
    		.viewMask = 0,
    		.colorAttachmentCount = 1,
    		.pColorAttachments = &color_attachment_info,
    		.pDepthAttachment = NULL,
    		.pStencilAttachment = NULL,
	}; /*vkCmdBeginRenderingKHR(ctx->cmd, &rendering_info); */

	if (!vk_cmd_begin_rendering_khr) {
		printf("vkCmdBeginRenderingKHR is NULL! Did you load it correctly?\n");
		return;
	} vk_cmd_begin_rendering_khr(ctx->cmd, &rendering_info);

	VkViewport view_port = {
	    .x = 0,
	    .y = 0,
	    .width = width,
	    .height = height,
	    .minDepth = 0.0f,
	    .maxDepth = 1.0f,
	}; vkCmdSetViewport(ctx->cmd, 0, 1, &view_port);

	VkRect2D scissor = {
		.offset = {0,0},
		.extent = {width, height},
	}; vkCmdSetScissor(ctx->cmd, 0, 1, &scissor);

	vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_program.pipeline);
	vkCmdDraw(ctx->cmd, 3, 1, 0, 0);

	if (!vk_cmd_end_rendering_khr) {
		printf("vkCmdendRenderingKHR is NULL! Did you load it correctly?\n");
		return;
	} vk_cmd_end_rendering_khr(ctx->cmd);

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
		
	program_destroy(&compute_program);
	shader_destroy(&compute_shader);

	program_destroy(&gfx_program);
	shader_destroy(&fragment_shader);
	shader_destroy(&vertex_shader);
}

int main() {
	mgfx_example_app();
}
