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

void choose_swapchain_extent_vk(const VkSurfaceCapabilitiesKHR* surface_caps, void* nwh, VkExtent2D* extent);

// Commands.
void vk_cmd_transition_image(VkCommandBuffer cmd,
                             VkImage target,
                             VkImageAspectFlags aspect_flags,
                             VkImageLayout old,
                             VkImageLayout new);

typedef struct {
  VkImage image;
  VkImageView view;

  VmaAllocation alloc;
} TextureVk;

void texture_create(TextureVk* texture);
void texture_destroy(TextureVk* texture);

enum {K_MAX_SWAPCHAIN_IMAGES = 4};
typedef struct SwapchainVk {
  TextureVk textures[K_MAX_SWAPCHAIN_IMAGES];
  uint32_t texture_count;

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
} ShaderVk;

void shader_create(size_t length, const char* code, ShaderVk* shader);
void shader_destroy(ShaderVk* shader);

typedef struct ProgramVk {
  VkPipelineLayout layout;
  VkPipeline pipeline;
} ProgramVk;

typedef struct FrameVk {
  VkCommandPool cmd_pool;
  VkCommandBuffer cmd;

  VkSemaphore render_semaphore;
  VkFence render_fence;

  VkSemaphore swapchain_semaphore;
} FrameVk;

#endif
