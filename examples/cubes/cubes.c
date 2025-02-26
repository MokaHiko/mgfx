#include "ex_common.h"

#include <cglm/cglm.h>
#include <vulkan/vulkan_core.h>

typedef struct Vertex {
	float position[3];
	float uv_x;
	float normal[3];
	float uv_y;
	float color[4];
} Vertex;

static Vertex k_vertices[] = {
	// Front face
    	{{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-left
    	{{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 0.0f, {0.0f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    	{{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {0.0f, 0.0f, 1.0f, 1.0f}},  // Top-right
    	{{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 0.0f, 1.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}},  // Top-left

    	// Back face
    	{{0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {1.0f, 0.0f, 1.0f, 1.0f}},  // Bottom-left
    	{{-0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 0.0f, {0.0f, 1.0f, 1.0f, 1.0f}},  // Bottom-right
    	{{-0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // Top-right
    	{{0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 0.0f, -1.0f}, 1.0f, {0.5f, 0.5f, 0.5f, 1.0f}},  // Top-left

    	// Left face
    	{{-0.5f, -0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}},  // Bottom-left
    	{{-0.5f, -0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    	{{-0.5f, 0.5f, 0.5f}, 1.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {0.0f, 0.5f, 1.0f, 1.0f}},  // Top-right
    	{{-0.5f, 0.5f, -0.5f}, 0.0f, {-1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.5f, 1.0f}},  // Top-left

    	// Right face
    	{{0.5f, -0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.0f, 1.0f}},  // Bottom-left
    	{{0.5f, -0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 0.0f, {0.5f, 0.5f, 1.0f, 1.0f}},  // Bottom-right
    	{{0.5f, 0.5f, -0.5f}, 1.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {0.5f, 0.0f, 1.0f, 1.0f}},  // Top-right
    	{{0.5f, 0.5f, 0.5f}, 0.0f, {1.0f, 0.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}},  // Top-left

    	// Top face
    	{{-0.5f, 0.5f, 0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.5f, 1.0f}},  // Bottom-left
    	{{0.5f, 0.5f, 0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.5f, 1.0f}},  // Bottom-right
    	{{0.5f, 0.5f, -0.5f}, 1.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {0.0f, 1.0f, 1.0f, 1.0f}},  // Top-right
    	{{-0.5f, 0.5f, -0.5f}, 0.0f, {0.0f, 1.0f, 0.0f}, 1.0f, {1.0f, 1.0f, 0.0f, 1.0f}},  // Top-left

    	// Bottom face
    	{{-0.5f, -0.5f, -0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 0.0f, {1.0f, 0.5f, 0.5f, 1.0f}},  // Bottom-left
    	{{0.5f, -0.5f, -0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 0.0f, {0.5f, 1.0f, 0.0f, 1.0f}},  // Bottom-right
    	{{0.5f, -0.5f, 0.5f}, 1.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {1.0f, 0.0f, 0.5f, 1.0f}},  // Top-right
    	{{-0.5f, -0.5f, 0.5f}, 0.0f, {0.0f, -1.0f, 0.0f}, 1.0f, {0.5f, 0.5f, 1.0f, 1.0f}},  // Top-left
};
const size_t k_vertex_count = sizeof(k_vertices) / sizeof(Vertex);

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
const size_t k_index_count = sizeof(k_indices) / sizeof(uint32_t);

shader_vk vertex_shader;
shader_vk fragment_shader;
program_vk gfx_program;

shader_vk compute_shader;
VkDescriptorSet compute_ds_set;
program_vk compute_program;

texture_vk color_attachment;
VkImageView color_attachment_view;

VertexBufferVk vertex_buffer;
IndexBufferVk index_buffer;

int width = 720;
int height = 480;
void mgfx_example_init() {
	load_shader_from_path("assets/shaders/unlit.vert.glsl.spv", &vertex_shader);
	load_shader_from_path("assets/shaders/unlit.frag.glsl.spv", &fragment_shader);

	program_create_graphics(&vertex_shader, &fragment_shader, &gfx_program);

	load_shader_from_path("assets/shaders/gradient.comp.glsl.spv", &compute_shader);
	program_create_compute(&compute_shader, &compute_program);

	texture_create_2d(width, height,
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
	vertex_buffer_create(k_vertex_count * sizeof(Vertex), k_vertices, &vertex_buffer);
	index_buffer_create(k_index_count * sizeof(uint32_t), k_indices, &index_buffer);
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
	}; vk_cmd_begin_rendering_khr(ctx->cmd, &rendering_info);

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

	static struct graphics_pc{
		mat4 model;
		mat4 view;
		mat4 proj;
	} graphics_pc;
	glm_perspective(glm_rad(60.0), 16.0/9.0, 0.1, 1000.0f, graphics_pc.proj);

	{
		vec3 position = {0.0f, 0.0f, 5.0f};
		vec3 forward = {0.0f, 0.0f, 0.0f};
		vec3 up = {0.0f, 1.0f, 0.0f};
		glm_lookat(position, forward, up, graphics_pc.view);
	}

	VkDeviceSize offsets = 0;
	vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &vertex_buffer.handle, &offsets);
	vkCmdBindIndexBuffer(ctx->cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

	for(int x = -2; x < 3; x++) {
		vec3 position = {x * 2.0f, 0.0f, 0.0f};
		glm_mat4_identity(graphics_pc.model);

		glm_translate(graphics_pc.model, position);

		vec3 axis = {1.0, 1.0, 1.0};
		glm_rotate(graphics_pc.model, pc.time * 3.141593, axis);

		vkCmdPushConstants(ctx->cmd, gfx_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(graphics_pc), &graphics_pc);

		vkCmdDrawIndexed(ctx->cmd, k_index_count, 1, 0, 0, 0);
	}

	vk_cmd_end_rendering_khr(ctx->cmd);

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
