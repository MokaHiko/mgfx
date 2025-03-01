#ifndef MGFX_RENDERER_VK
#define MGFX_RENDERER_VK

#include <mx/mx.h>
#include <mx/mx_memory.h>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include "mgfx/defines.h"

#ifdef MX_DEBUG
#define VK_CHECK(call)                                      \
  do {                                                      \
    VkResult result = (call);                               \
    if (result != VK_SUCCESS) {                             \
      fprintf(stderr,                                       \
              "Vulkan Error: %d in %s at line %d\n",        \
              result,                                       \
              __FILE__,                                     \
              __LINE__);                                    \
      __builtin_trap();					    \
    }                                                       \
  } while (0)
#else
#define VK_CHECK(call) (call)
#endif

uint32_t vk_format_size(VkFormat format);

VkResult get_window_surface_vk(VkInstance instance, void* window, VkSurfaceKHR* surface);

int choose_physical_device_vk(VkInstance instance,
                          uint32_t device_ext_count,
                          const char** device_exts,
                          VkPhysicalDevice* phys_device,
                          MxArena* arena);

void choose_swapchain_extent_vk(const VkSurfaceCapabilitiesKHR* surface_caps, 
                                void* nwh, 
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
  VkImage handle;
  VkFormat format;

  VkImageLayout layout;
  VkExtent3D extent;
} image_vk;

enum {
  K_SWAPCHAIN_MAX_IMAGES = 4,
};


typedef struct swapchain_vk {
  image_vk images[K_SWAPCHAIN_MAX_IMAGES];
  VkImageView image_views[K_SWAPCHAIN_MAX_IMAGES];
  uint32_t image_count;

  VkExtent2D extent;
  VkSwapchainKHR handle;
} swapchain_vk;

int swapchain_create(VkSurfaceKHR surface,
                     uint32_t width,
                     uint32_t height,
                     swapchain_vk* swapchain,
                     MxArena* memory_arena);
void swapchain_destroy(swapchain_vk* swapchain);

typedef struct buffer_vk {
  VkBufferUsageFlags usage;

  VmaAllocation allocation;
  VkBuffer handle;
} buffer_vk;

typedef buffer_vk VertexBufferVk;
typedef buffer_vk IndexBufferVk;

void buffer_create(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags, buffer_vk* buffer);
void buffer_destroy(buffer_vk* buffer);

void vertex_buffer_create(size_t size, const void* data, VertexBufferVk* MX_NOT_NULL buffer);
void index_buffer_create(size_t size, const void* data, IndexBufferVk* MX_NOT_NULL buffer);

typedef struct texture_vk {
  VmaAllocation allocation;
  image_vk image;
} texture_vk;

void texture_create_view(const texture_vk* texture, VkImageAspectFlags aspect, VkImageView* view);
void texture_create_2d(uint32_t width,
                       uint32_t height,
                       VkFormat format,
                       VkImageUsageFlags usage,
                       texture_vk* texture);

void texture_update(size_t size, const void* data, texture_vk *texture);
void texture_destroy(texture_vk *texture);

enum {MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS = 4};
typedef struct framebuffer_vk {
  texture_vk* color_attachments[MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS];
  VkImageView color_attachment_views[MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS];
  uint32_t color_attachment_count;

  texture_vk* depth_attachment;
  VkImageView depth_attachment_view;
} framebuffer_vk;

void framebuffer_create(uint32_t color_attachment_count,
                        texture_vk* color_attachments,
                        texture_vk* depth_attachment,
                        framebuffer_vk* framebuffer);
void framebuffer_destroy(framebuffer_vk* fb);

enum { K_SHADER_MAX_DESCRIPTOR_SET = 4};
enum {K_SHADER_MAX_DESCRIPTOR_BINDING = 8};
enum {K_SHADER_MAX_PUSH_CONSTANTS = 4};
enum {K_SHADER_MAX_VERTEX_BINDINGS = 4};
enum {K_SHADER_MAX_VERTEX_ATTRIBUTES = 16};
typedef struct descriptor_set_info_vk {
  VkDescriptorSetLayoutBinding bindings[K_SHADER_MAX_DESCRIPTOR_BINDING];
  uint32_t binding_count;
} descriptor_set_info_vk;

typedef struct shader_vk {
  descriptor_set_info_vk descriptor_sets[K_SHADER_MAX_DESCRIPTOR_SET];
  int descriptor_set_count;

  VkPushConstantRange pc_ranges[K_SHADER_MAX_PUSH_CONSTANTS];
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

typedef struct program_vk {
  const shader_vk* shaders[MGFX_SHADER_STAGE_COUNT];
  VkDescriptorSetLayout ds_layouts[K_SHADER_MAX_DESCRIPTOR_SET];

  VkPipelineLayout layout;
  VkPipeline pipeline;
} program_vk;


void program_create_compute(const shader_vk* cs, program_vk* program);
void program_create_graphics(const shader_vk* vs, const shader_vk* fs, const framebuffer_vk* fb, program_vk* program);

// TODO: Descriptors.
void create_sampler(VkFilter filter, VkSampler* sampler);

void program_create_descriptor_sets(const program_vk* program,
                                    const VkDescriptorBufferInfo* ds_buffer_infos,
                                    const VkDescriptorImageInfo* ds_image_infos,
                                    VkDescriptorSet* ds_sets);
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

void vk_cmd_transition_image(VkCommandBuffer cmd,
                             image_vk* image,
                             VkImageAspectFlags aspect_flags,
                             VkImageLayout new);

void vk_cmd_clear_image(VkCommandBuffer cmd,
                        image_vk* target,
                        const VkImageSubresourceRange *range,
                        const VkClearColorValue *clear);

void vk_cmd_copy_image_to_image(VkCommandBuffer cmd,
                                const image_vk* src,
                                VkImageAspectFlags aspect,
                                image_vk* dst);

void vk_cmd_begin_rendering(VkCommandBuffer cmd, framebuffer_vk* fb);
void vk_cmd_end_rendering(VkCommandBuffer cmd);

//TODO: Change to encoder
typedef struct DrawCtx {
  VkCommandBuffer cmd;
  image_vk* frame_target;
} DrawCtx;

extern void mgfx_example_updates(const DrawCtx* frame);

#endif
