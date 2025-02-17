#ifndef MGFX_RENDERER_VK
#define MGFX_RENDERER_VK

#include <mx/mx.h>
#include <mx/mx_memory.h>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

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
                          mx_arena* arena);

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

enum {K_MAX_SWAPCHAIN_IMAGES = 4};
typedef struct SwapchainVk {
  ImageVk images[K_MAX_SWAPCHAIN_IMAGES];
  VkImageView image_views[K_MAX_SWAPCHAIN_IMAGES];
  uint32_t image_count;

  VkExtent2D extent;
  VkSwapchainKHR handle;
} SwapchainVk;

int swapchain_create(VkSurfaceKHR surface,
                     uint32_t width,
                     uint32_t height,
                     SwapchainVk* swapchain,
                     mx_arena* memory_arena);
void swapchain_destroy(SwapchainVk* swapchain);

typedef struct ShaderVk {
  VkShaderModule module;
  VkDescriptorSetLayoutBinding bindings;
} ShaderVk;

void shader_create(size_t length, const char* code, ShaderVk* shader);
void shader_destroy(ShaderVk* shader);

typedef struct TextureVk {
  ImageVk image;
  VmaAllocation allocation;
} TextureVk;

void texture_create_2d(uint32_t width,
                    uint32_t height,
                    VkFormat format,
                    VkImageUsageFlags usage,
                    TextureVk* texture);
void texture_create_view_2d(const TextureVk* texture, const VkImageAspectFlags aspect, VkImageView* view);
void texture_destroy(TextureVk *texture);

typedef struct ProgramVk {
  VkPipelineLayout layout;
  VkPipeline pipeline;
} ProgramVk;

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
