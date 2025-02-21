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

VkResult get_window_surface_vk(VkInstance instance, void* window, VkSurfaceKHR* surface);

int choose_physical_device_vk(VkInstance instance,
                          uint32_t device_ext_count,
                          const char** device_exts,
                          VkPhysicalDevice* phys_device,
                          MxArena* arena);

void choose_swapchain_extent_vk(const VkSurfaceCapabilitiesKHR* surface_caps, 
                                void* nwh, 
                                VkExtent2D* extent);
typedef struct FrameVk {
  VkCommandPool cmd_pool;
  VkCommandBuffer cmd;

  VkSemaphore render_semaphore;
  VkFence render_fence;

  VkSemaphore swapchain_semaphore;
} FrameVk;

typedef struct ImageVk {
  VkImage handle;
  VkFormat format;

  VkImageLayout layout;
  VkExtent3D extent;
} ImageVk;

enum {K_SWAPCHAIN_MAX_IMAGES = 4};
typedef struct SwapchainVk {
  ImageVk images[K_SWAPCHAIN_MAX_IMAGES];
  VkImageView image_views[K_SWAPCHAIN_MAX_IMAGES];
  uint32_t image_count;

  VkExtent2D extent;
  VkSwapchainKHR handle;
} SwapchainVk;

int swapchain_create(VkSurfaceKHR surface,
                     uint32_t width,
                     uint32_t height,
                     SwapchainVk* swapchain,
                     MxArena* memory_arena);
void swapchain_destroy(SwapchainVk* swapchain);

enum {K_TEXTURE_MAX_VIEWS = 4};
typedef struct TextureVk {
  VkImageView views[K_TEXTURE_MAX_VIEWS];
  int view_count;

  VmaAllocation allocation;
  ImageVk image;
} TextureVk;

VkImageView texture_get_view(const VkImageAspectFlags aspect, TextureVk* texture);

void texture_create_2d(uint32_t width,
                    uint32_t height,
                    VkFormat format,
                    VkImageUsageFlags usage,
                    TextureVk* texture);
void texture_destroy(TextureVk *texture);

enum {K_SHADER_MAX_DESCRIPTOR_SET = 4};
enum {K_SHADER_MAX_DESCRIPTOR_BINDING = 8};
typedef struct DescriptorSetInfoVk {
  VkDescriptorSetLayoutBinding bindings[K_SHADER_MAX_DESCRIPTOR_BINDING];
  uint32_t binding_count;
} DescriptorSetInfoVk;

typedef struct ShaderVk {
  DescriptorSetInfoVk descriptor_sets[K_SHADER_MAX_DESCRIPTOR_SET];
  int descriptor_set_count;

  VkShaderModule module;
} ShaderVk;

void shader_create(size_t length, const char* code, ShaderVk* shader);
void shader_destroy(ShaderVk* shader);

typedef struct ProgramVk {
  VkDescriptorSetLayout ds_layouts[K_SHADER_MAX_DESCRIPTOR_SET];
  const ShaderVk* shaders[MGFX_SHADER_STAGE_COUNT];

  VkPipelineLayout layout;
  VkPipeline pipeline;
} ProgramVk;

void program_create_compute(const ShaderVk* cs, ProgramVk* program);

void program_create_descriptor_sets(const ProgramVk* program,
                                    const VkDescriptorBufferInfo* ds_buffer_infos,
                                    const VkDescriptorImageInfo* ds_image_infos,
                                    VkDescriptorSet* ds_sets);

// Commands.
void vk_cmd_transition_image(VkCommandBuffer cmd,
                             ImageVk* image,
                             VkImageAspectFlags aspect_flags,
                             VkImageLayout new);

void vk_cmd_clear_image(VkCommandBuffer cmd,
                        ImageVk* target,
                        const VkImageSubresourceRange *range,
                        const VkClearColorValue *clear);

void vk_cmd_copy_image_to_image(VkCommandBuffer cmd,
                                const ImageVk* src,
                                VkImageAspectFlags aspect,
                                ImageVk* dst);

// TODO: Remove
//TODO: Change to encoder
typedef struct DrawCtx {
  VkCommandBuffer cmd;
  ImageVk* frame_target;
} DrawCtx;
extern void mgfx_example_updates(const DrawCtx* frame);

#endif
