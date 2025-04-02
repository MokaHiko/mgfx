#ifndef MGFX_RENDERER_VK
#define MGFX_RENDERER_VK

#include <mx/mx.h>
#include <mx/mx_log.h>
#include <mx/mx_memory.h>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "mgfx/defines.h"

#ifdef MX_DEBUG
#define VK_CHECK(call)                                                                             \
    do {                                                                                           \
        VkResult result = (call);                                                                  \
        if (result != VK_SUCCESS) {                                                                \
            MX_LOG_ERROR("Vulkan Error: %d in %s at line %d", result, __FILE__, __LINE__);         \
            __builtin_trap();                                                                      \
        }                                                                                          \
    } while (0)
#else
#define VK_CHECK(call) (call)
#endif

uint32_t vk_format_size(VkFormat format);

VkResult get_window_surface_vk(VkInstance instance, void* window, VkSurfaceKHR* surface);

int choose_physical_device_vk(VkInstance instance, uint32_t device_ext_count,
                              const char** device_exts,
                              VkPhysicalDeviceProperties* phys_device_props,
                              VkPhysicalDevice* phys_device, mx_arena* arena);

void choose_swapchain_extent_vk(const VkSurfaceCapabilitiesKHR* surface_caps, void* nwh,
                                VkExtent2D* extent);

enum {
    MGFX_FRAME_0,
    MGFX_FRAME_1,
    MGFX_FRAME_COUNT = 2 /* MGFX_FRAME_OVERLAP */
};

typedef struct frame_vk {
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd;

    VkSemaphore render_semaphore;
    VkFence render_fence;

    VkSemaphore swapchain_semaphore;
} frame_vk;

typedef struct image_vk {
    VkExtent3D extent;
    VkFormat format;
    VkImageLayout layout;

    uint32_t layer_count;

    VkImage handle;
    VmaAllocation allocation;
} image_vk;

void image_create(uint32_t width,
                  uint32_t height,
                  VkFormat format,
                  uint32_t array_layers,
                  VkImageUsageFlags usage,
                  VkImageCreateFlags flags,
                  image_vk* image);
void image_create_view(const image_vk* image,
                       VkImageViewType type,
                       VkImageAspectFlags aspect,
                       VkImageView* view);

void image_update(size_t size, const void* data, image_vk* image);
void image_destroy(image_vk* image);

// TODO: Remove
typedef struct texture_vk {
    image_vk image;
    VkImageView view;
    VkSampler sampler;
} texture_vk;
void texture_create_2d(uint32_t width,
                       uint32_t height,
                       VkFormat format,
                       VkFilter filter,
                       texture_vk* texture);
void texture_create_3d(uint32_t width,
                       uint32_t height,
                       VkFormat format,
                       VkFilter filter,
                       texture_vk* texture);
void texture_destroy(texture_vk* texture);

enum { MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS = 4 };
typedef struct framebuffer_vk {
    image_vk* color_attachments[MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS];
    VkImageView color_attachment_views[MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS];
    uint32_t color_attachment_count;

    image_vk* depth_attachment;
    VkImageView depth_attachment_view;
} framebuffer_vk;

void framebuffer_create(uint32_t color_attachment_count, image_vk* color_attachments,
                        image_vk* depth_attachment, framebuffer_vk* framebuffer);
void framebuffer_destroy(framebuffer_vk* fb);

enum {
    K_SWAPCHAIN_MAX_IMAGES = 4,
};

typedef struct swapchain_vk {
    image_vk images[K_SWAPCHAIN_MAX_IMAGES];
    VkImageView image_views[K_SWAPCHAIN_MAX_IMAGES];
    uint32_t image_count;

    VkExtent2D extent;
    VkSwapchainKHR handle;

    framebuffer_vk framebuffer;

    uint32_t free_idx;
    mx_bool resize;
} swapchain_vk;

int swapchain_create(VkSurfaceKHR surface, uint32_t w, uint32_t h, swapchain_vk* swapchain,
                     mx_arena* memory_arena);
void swapchain_destroy(swapchain_vk* swapchain);

typedef struct buffer_vk {
    VkBufferUsageFlags usage;

    VmaAllocation allocation;
    VkBuffer handle;
} buffer_vk;

typedef buffer_vk vertex_buffer_vk;
typedef buffer_vk index_buffer_vk;
typedef buffer_vk uniform_buffer_vk;

void buffer_create(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags,
                   buffer_vk* buffer);
void buffer_update(buffer_vk* buffer, size_t buffer_offset, size_t size, const void* data);
void buffer_destroy(buffer_vk* buffer);

void vertex_buffer_create(size_t size, const void* data, vertex_buffer_vk* MX_NOT_NULL buffer);
void index_buffer_create(size_t size, const void* data, index_buffer_vk* MX_NOT_NULL buffer);

void uniform_buffer_create(size_t size, const void* data, uniform_buffer_vk* buffer);

typedef struct buffer_pool_vk {
    buffer_vk buffer;
    uint32_t head;
} buffer_pool_vk;

typedef struct buffer_slice_vk {
    buffer_pool_vk* buffer_pool;
    size_t offset;
    size_t size;
} buffer_slice_vk;

void buffer_slice_allocate(buffer_pool_vk* pool, size_t size, buffer_slice_vk* slice);
void buffer_slice_update(buffer_slice_vk* slice, size_t size, const void* data);

typedef struct descriptor_set_info_vk {
    VkDescriptorSetLayoutBinding bindings[MGFX_SHADER_MAX_DESCRIPTOR_BINDING];
    uint32_t binding_count;
} descriptor_set_info_vk;

typedef struct shader_vk {
    descriptor_set_info_vk descriptor_sets[MGFX_SHADER_MAX_DESCRIPTOR_SET];
    int descriptor_set_count;

    VkPushConstantRange pc_ranges[MGFX_SHADER_MAX_PUSH_CONSTANTS];
    int pc_count;

    // Vertex shader.
    VkVertexInputBindingDescription vertex_bindings[K_SHADER_MAX_VERTEX_BINDINGS];
    int vertex_binding_count;

    VkVertexInputAttributeDescription vertex_attributes[K_SHADER_MAX_VERTEX_ATTRIBUTES];
    int vertex_attribute_count;

    VkShaderModule module;
} shader_vk;

void shader_create(size_t length, const char* code, shader_vk* shader);
void shader_destroy(shader_vk* shader);

typedef struct descriptor_info_vk {
    char name[128];
    VkDescriptorType type;

    union {
        VkDescriptorImageInfo image_info;
        VkDescriptorBufferInfo buffer_info;
    };
} descriptor_info_vk;

void descriptor_buffer_create(const buffer_vk* buffer, descriptor_info_vk* descriptor);
void descriptor_image_create(const VkImageView* image_view, descriptor_info_vk* descriptor);
void descriptor_texture_create(const texture_vk* texture, descriptor_info_vk* descriptor);

void descriptor_write(VkDescriptorSet set, uint32_t binding, const descriptor_info_vk* descriptor);

// TODO: Remove
typedef struct program_vk {
    const shader_vk* shaders[MGFX_SHADER_STAGE_COUNT];

    VkDescriptorSetLayout descriptor_set_layouts[MGFX_SHADER_MAX_DESCRIPTOR_SET];
    uint32_t descriptor_set_count;

    VkPipelineLayout layout;
    VkPipeline pipeline;
} program_vk;
void program_create_compute(const shader_vk* cs, program_vk* program);
void program_create_graphics(const shader_vk* vs,
                             const shader_vk* fs,
                             const framebuffer_vk* fb,
                             program_vk* program);
// TODO: Remove
void program_create_descriptor_set(program_vk* program, VkDescriptorSet* ds);
void program_destroy(program_vk* program);

// TODO: Remove.
extern PFN_vkCmdBeginRenderingKHR vk_cmd_begin_rendering_khr;
extern PFN_vkCmdEndRenderingKHR vk_cmd_end_rendering_khr;

// Commands.
typedef struct buffer_to_buffer_copy_vk {
    VkBufferCopy copy;
    const buffer_vk* src;
    buffer_vk* dst;
} buffer_to_buffer_copy_vk;

typedef struct buffer_to_image_copy_vk {
    VkBufferImageCopy copy;
    const buffer_vk* src;
    image_vk* dst;
} buffer_to_image_copy_vk;

void vk_cmd_transition_image(VkCommandBuffer cmd, image_vk* image, VkImageAspectFlags aspect_flags,
                             VkImageLayout new);

void vk_cmd_clear_image(VkCommandBuffer cmd, image_vk* target, const VkImageSubresourceRange* range,
                        const VkClearColorValue* clear);

void vk_cmd_copy_image_to_image(VkCommandBuffer cmd, const image_vk* src, VkImageAspectFlags aspect,
                                image_vk* dst);

void vk_cmd_begin_rendering(VkCommandBuffer cmd, framebuffer_vk* fb);
void vk_cmd_end_rendering(VkCommandBuffer cmd);

// TODO: Change to encoder
typedef struct draw_ctx {
    VkCommandBuffer cmd;
    framebuffer_vk* frame_target;
} draw_ctx;

// TODO: Remove
extern void mgfx_example_updates(const draw_ctx* frame);

#endif
