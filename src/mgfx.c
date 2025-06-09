#include "mgfx/mgfx.h"
#include "mgfx/defines.h"
#include "mx/mx_string.h"

#include <mx/mx_log.h>
#include <mx/mx_math.h>

#include <mx/mx.h>
#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_hash.h>
#include <mx/mx_memory.h>
#include <stdint.h>
#include <string.h>

#include <vulkan/vulkan_core.h>

#ifdef MX_MACOS
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_metal.h>
#elif defined(MX_WIN32)
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <vulkan/vulkan_win32.h>
// clang-format on
#endif

#include "renderer_vk.h"

#include <spirv_reflect/spirv_reflect.h>
#include <vma/vk_mem_alloc.h>

#include <mx/mx_math_mtx.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

typedef struct mgfx_texture {
  mgfx_imgh imgh;        // VkImage
  uint64_t address_mode; // VkSamplerAddressMode
  uint64_t sampler;      // VkSampler
} mgfx_texture;

enum { MGFX_ENTRY_MAX_NAME = 64 };
typedef struct mgfx_program {
  char name[MGFX_ENTRY_MAX_NAME];
  mgfx_sh shaders[MGFX_SHADER_STAGE_COUNT];
  uint64_t dsls[MGFX_SHADER_MAX_DESCRIPTOR_SET]; // VkPipelineLayout

  // Only for graphics pipelines
  union {
    struct {
      int32_t primitive_topology;
      int32_t polygon_mode;
      int32_t cull_mode;
    };
  };

  uint64_t pipeline;        // VkPipeline
  uint64_t pipeline_layout; // VkPipelineLayout

} mgfx_program;

// Renderer api
void buffer_create(size_t size, VkBufferUsageFlags usage,
                   VmaAllocationCreateFlags flags, buffer_vk *buffer);

// ~ VULKAN RENDERER  ~ //

// Extension function pointers.
PFN_vkCmdBeginRenderingKHR vk_cmd_begin_rendering_khr;
PFN_vkCmdEndRenderingKHR vk_cmd_end_rendering_khr;

const char *k_req_exts[] = {VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef MX_DEBUG
                            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif

#ifdef MX_MACOS
                            VK_EXT_METAL_SURFACE_EXTENSION_NAME,
                            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#elif defined(MX_WIN32)
                            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
};
const int k_req_ext_count = sizeof(k_req_exts) / sizeof(const char *);

const char *k_req_device_ext_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef MX_MACOS
                                        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
};
const uint32_t k_req_device_ext_count =
    (uint32_t)(sizeof(k_req_device_ext_names) / sizeof(const char *));

const VkFormat k_surface_fmt = VK_FORMAT_B8G8R8A8_SRGB;
const VkColorSpaceKHR k_surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
const VkPresentModeKHR k_present_mode = VK_PRESENT_MODE_FIFO_KHR;

// TODO: Keep track of descripotr pool memory allocations
const VkDescriptorPoolSize k_ds_pool_sizes[] = {
    {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 128},
    {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 128},
    {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 32},
    {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 128},
};
uint32_t k_ds_pool_sizes_count = 4;

#ifdef MX_DEBUG
VkResult create_debug_util_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  PFN_vkCreateDebugUtilsMessengerEXT func =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != NULL) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void destroy_create_debug_util_messenger_ext(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator) {
  PFN_vkDestroyDebugUtilsMessengerEXT func =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != NULL) {
    func(instance, debugMessenger, pAllocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                  VkDebugUtilsMessageTypeFlagsEXT msg_type,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                  void *pUserData) {

  static const char *msg_type_strings[0x9] = {
      [0x00000001] = "VK_GENERAL",
      [0x00000002] = "VK_VALIDATION",
      [0x00000004] = "VK_PERFORMANCE",
      [0x00000008] = "VK_DEVICE_ADDRESS_BINDING",
  };

  switch (msg_severity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    MX_LOG_DEBUG("[%s]: %s", msg_type_strings[msg_type],
                 pCallbackData->pMessage);
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    MX_LOG_DEBUG("[%s]: %s", msg_type_strings[msg_type],
                 pCallbackData->pMessage);
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    MX_LOG_WARN("%s", pCallbackData->pMessage);
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    MX_LOG_ERROR("[%s]: %s", msg_type_strings[msg_type],
                 pCallbackData->pMessage);
    MX_ASSERT(0);
    break;
  default:
    break;
  };

  return VK_FALSE;
}

static VkDebugUtilsMessengerEXT s_debug_messenger;
#endif

static VkInstance s_instance = VK_NULL_HANDLE;

static VkPhysicalDevice s_phys_device = VK_NULL_HANDLE;
static VkPhysicalDeviceProperties s_phys_device_props;

static VkDevice s_device = VK_NULL_HANDLE;
static VmaAllocator s_allocator;

static VkQueue s_queues[VK_QUEUE_COUNT];
static uint32_t s_queue_indices[VK_QUEUE_COUNT];

static VkSurfaceKHR s_surface = VK_NULL_HANDLE;
static VkSurfaceCapabilitiesKHR s_surface_caps;
static swapchain_vk s_swapchain;

static frame_vk s_frames[MGFX_FRAME_COUNT];
static uint32_t s_frame_idx = 0;

static VkDescriptorPool s_ds_pool;

enum { MAX_FRAME_BUFFER_COPIES = 300 };
static buffer_to_buffer_copy_vk
    s_buffer_to_buffer_copy_queue[MAX_FRAME_BUFFER_COPIES];
static uint32_t s_buffer_to_buffer_copy_count = 0;

static buffer_to_image_copy_vk
    s_buffer_to_image_copy_queue[MAX_FRAME_BUFFER_COPIES];
static uint32_t s_buffer_to_image_copy_count = 0;

static ring_buffer_vk s_transient_vertex_buffer_pool;
static ring_buffer_vk s_transient_index_buffer_pool;

static mgfx_transient_buffer s_tsbs[MAX_FRAME_BUFFER_COPIES];
static uint32_t s_transient_staging_buffer_count = 0;
static ring_buffer_vk s_transient_staging_buffer_pool;

void image_create(const mgfx_image_info *info, VkImageUsageFlags usage,
                  VkImageCreateFlags flags, image_vk *image) {
  image->format = (VkFormat)info->format;
  image->layer_count = info->layers;
  image->extent = (VkExtent3D){info->width, info->height, 1};

  usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = flags,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = image->format,
      .extent = image->extent,
      .mipLevels = 1,
      .arrayLayers = info->layers,
      .samples = 1,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &s_queue_indices[VK_QUEUE_GRAPHICS],
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo alloc_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };

  VK_CHECK(vmaCreateImage(s_allocator, &image_info, &alloc_info, &image->handle,
                          &image->allocation, NULL));

#ifdef MX_DEBUG
  static size_t mem_acc = 0;

  VmaAllocationInfo image_alloc_info = {0};
  vmaGetAllocationInfo(s_allocator, image->allocation, &image_alloc_info);
  mem_acc += image_alloc_info.size;

  if (image_alloc_info.size > MX_MB) {
    MX_LOG_TRACE("[ImageAlloc] + %lu mb = %.2f mb",
                 image_alloc_info.size / MX_MB, (float)mem_acc / MX_MB);
  } else if (image_alloc_info.size > MX_KB) {
    MX_LOG_TRACE("[ImageAlloc] + %lu kb = %.2f mb",
                 image_alloc_info.size / MX_KB, (float)mem_acc / MX_MB);
  } else {
    MX_LOG_TRACE("[ImageAlloc] + %lu b = %.2f mb", image_alloc_info.size,
                 (float)mem_acc / MX_MB);
  }

#endif
}

void image_create_view(const image_vk *image, VkImageViewType type,
                       VkImageAspectFlags aspect, VkImageView *view) {
  VkImageViewCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .image = image->handle,
      .viewType = type,
      .format = image->format,
      .components =
          {
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange =
          {
              .aspectMask = aspect,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = image->layer_count,
          },
  };

  VK_CHECK(vkCreateImageView(s_device, &info, NULL, view));
}

void transient_buffer_allocate(ring_buffer_vk *pool, const void *data,
                               size_t len, mgfx_transient_buffer *out);
void image_update(const void *data, size_t size, image_vk *image) {
  MX_ASSERT(s_transient_staging_buffer_count < MAX_FRAME_BUFFER_COPIES);
  mgfx_transient_buffer *staging_buffer =
      &s_tsbs[s_transient_staging_buffer_count++];
  transient_buffer_allocate(&s_transient_staging_buffer_pool, data, size,
                            staging_buffer);

  MX_ASSERT(s_buffer_to_image_copy_count < MAX_FRAME_BUFFER_COPIES);
  s_buffer_to_image_copy_queue[s_buffer_to_image_copy_count++] =
      (buffer_to_image_copy_vk){
          .copy =
              {
                  .bufferOffset = staging_buffer->offset,
                  .bufferRowLength = 0,
                  .bufferImageHeight = 0,
                  .imageSubresource =
                      {
                          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                          .mipLevel = 0,
                          .baseArrayLayer = 0,
                          .layerCount = 1,
                      },
                  .imageOffset = 0,
                  .imageExtent = image->extent,
              },
          .src = &s_transient_staging_buffer_pool.buffer,
          .dst = image,
      };
};

void image_destroy(image_vk *image) {
  vmaDestroyImage(s_allocator, image->handle, image->allocation);

  image->handle = VK_NULL_HANDLE;
  image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

int swapchain_create(VkSurfaceKHR surface, uint32_t w, uint32_t h,
                     swapchain_vk *sc, mx_allocator_t allocator) {
  sc->extent.width = w;
  sc->extent.height = h;

  VkSwapchainCreateInfoKHR swapchain_info = {0};
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.pNext = NULL;
  swapchain_info.flags = 0;
  swapchain_info.surface = surface;

  uint32_t image_count = s_surface_caps.minImageCount + 1;
  if (s_surface_caps.maxImageCount > 0 &&
      image_count > s_surface_caps.maxImageCount) {
    image_count = s_surface_caps.maxImageCount;
  }
  swapchain_info.minImageCount = image_count;

  swapchain_info.imageFormat = k_surface_fmt;
  swapchain_info.imageColorSpace = k_surface_color_space;

  swapchain_info.imageExtent.width = sc->extent.width;
  swapchain_info.imageExtent.height = sc->extent.height;

  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (s_queue_indices[VK_QUEUE_GRAPHICS] != s_queue_indices[VK_QUEUE_PRESENT]) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = 2;
    swapchain_info.pQueueFamilyIndices = &s_queue_indices[VK_QUEUE_PRESENT];
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices = NULL;
  }

  swapchain_info.preTransform = s_surface_caps.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = k_present_mode;
  swapchain_info.clipped = VK_TRUE;
  swapchain_info.oldSwapchain = NULL;

  VK_CHECK(vkCreateSwapchainKHR(s_device, &swapchain_info, NULL, &sc->handle));

  vkGetSwapchainImagesKHR(s_device, sc->handle, &sc->image_count, NULL);

  MX_ASSERT(sc->image_count < K_SWAPCHAIN_MAX_IMAGES,
            "Requested swapchain image count (%d) larger than max capacity! %d",
            sc->image_count, K_SWAPCHAIN_MAX_IMAGES);

  VkImage *images = mx_alloc(allocator, sc->image_count * sizeof(VkImage));
  vkGetSwapchainImagesKHR(s_device, sc->handle, &sc->image_count, images);

  VkImageViewCreateInfo sc_img_view_info = {0};
  sc_img_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  sc_img_view_info.pNext = NULL;
  sc_img_view_info.flags = 0;
  sc_img_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  sc_img_view_info.format = k_surface_fmt;
  sc_img_view_info.components = (VkComponentMapping){
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
  };

  sc_img_view_info.subresourceRange = (VkImageSubresourceRange){
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  for (uint32_t i = 0; i < sc->image_count; i++) {
    sc->images[i] = (image_vk){
        .handle = images[i],
        .format = k_surface_fmt,
        .layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .extent = (VkExtent3D){w, h, 1},
    };

    sc_img_view_info.image = sc->images[i].handle;
    vkCreateImageView(s_device, &sc_img_view_info, NULL, &sc->image_views[i]);
  }

  sc->framebuffer = (framebuffer_vk){
      .color_attachments = &s_swapchain.images[0],
      .color_attachment_views = s_swapchain.image_views[0],
      .color_attachment_count = 1,

      .depth_attachment = NULL,
      .depth_attachment_view = NULL,
  };

  return MGFX_SUCCESS;
}

void swapchain_destroy(swapchain_vk *swapchain) {
  for (uint32_t i = 0; i < swapchain->image_count; i++) {
    vkDestroyImageView(s_device, swapchain->image_views[i], NULL);
  }

  vkDestroySwapchainKHR(s_device, swapchain->handle, NULL);
}

void buffer_create(size_t size, VkBufferUsageFlags usage,
                   VmaAllocationCreateFlags flags, buffer_vk *buffer) {
  buffer->usage = usage;

  VkBufferCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = size,
      .usage = buffer->usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  if ((flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) ==
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) {
    flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
  };

  VmaAllocationCreateInfo alloc_info = {
      .flags = flags,
      .usage = VMA_MEMORY_USAGE_AUTO,
  };

  VK_CHECK(vmaCreateBuffer(s_allocator, &info, &alloc_info, &buffer->handle,
                           &buffer->allocation, NULL));

#ifdef MX_DEBUG
  static size_t buffer_memory_accum = 0;

  VmaAllocationInfo buffer_alloc_info = {0};
  vmaGetAllocationInfo(s_allocator, buffer->allocation, &buffer_alloc_info);
  buffer_memory_accum += buffer_alloc_info.size;

  if (buffer_alloc_info.size > MX_MB) {
    MX_LOG_TRACE("[BufferAlloc] + %lu mb = %.2f mb",
                 buffer_alloc_info.size / MX_MB,
                 (float)buffer_memory_accum / MX_MB);
  } else if (buffer_alloc_info.size > MX_KB) {
    MX_LOG_TRACE("[BufferAlloc] + %lu kb = %.2f mb",
                 buffer_alloc_info.size / MX_KB,
                 (float)buffer_memory_accum / MX_MB);
  } else {
    MX_LOG_TRACE("[BufferAlloc] + %lu b = %.2f mb", buffer_alloc_info.size,
                 (float)buffer_memory_accum / MX_MB);
  }
#endif
};

// Forward declare transient for staging buffers
void buffer_update(buffer_vk *buffer, size_t buffer_offset, size_t size,
                   const void *data) {
  size_t dst_offset = buffer_offset;

  VmaAllocationInfo alloc_info;
  vmaGetAllocationInfo(s_allocator, buffer->allocation, &alloc_info);

  MX_ASSERT(buffer_offset + size <= alloc_info.size);

  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(s_phys_device, &mem_props);
  uint32_t memory_type_idx = alloc_info.memoryType;

  VkMemoryPropertyFlags memory_flags =
      mem_props.memoryTypes[memory_type_idx].propertyFlags;

  if ((memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
    memcpy((uint8_t *)alloc_info.pMappedData + dst_offset, data, size);
    return;
  }

  // TODO: Recover and resize
  MX_ASSERT(s_transient_staging_buffer_count < MAX_FRAME_BUFFER_COPIES);
  mgfx_transient_buffer *staging_buffer =
      &s_tsbs[s_transient_staging_buffer_count++];
  transient_buffer_allocate(&s_transient_staging_buffer_pool, data, size,
                            staging_buffer);

  MX_ASSERT(s_buffer_to_buffer_copy_count < MAX_FRAME_BUFFER_COPIES);
  s_buffer_to_buffer_copy_queue[s_buffer_to_buffer_copy_count++] =
      (buffer_to_buffer_copy_vk){
          .copy =
              {
                  .srcOffset = staging_buffer->offset,
                  .dstOffset = dst_offset,
                  .size = size,
              },
          .src = &s_transient_staging_buffer_pool.buffer,
          .dst = buffer,
      };
}

void buffer_resize(buffer_vk *buffer, size_t size) {
  VmaAllocationInfo alloc_info;
  vmaGetAllocationInfo(s_allocator, buffer->allocation, &alloc_info);

  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(s_phys_device, &mem_props);
  uint32_t memory_type_idx = alloc_info.memoryType;

  VkMemoryPropertyFlags memory_flags =
      mem_props.memoryTypes[memory_type_idx].propertyFlags;

  MX_ASSERT(alloc_info.size <= size,
            "[Buffer] Cannot resize to less than or equal of previous size!");

  // buffer_update(buffer, 0, alloc_info.size, const void *data)
}

void buffer_destroy(buffer_vk *buffer) {
  vmaDestroyBuffer(s_allocator, (VkBuffer)buffer->handle, buffer->allocation);

  buffer->handle = VK_NULL_HANDLE;
  buffer->allocation = VK_NULL_HANDLE;
}

void vertex_buffer_create(const void *data, size_t len, buffer_vk *vb) {
  buffer_create(
      len, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      0, vb);

  if (data) {
    buffer_update(vb, 0, len, data);
  }
};

void index_buffer_create(const void *data, size_t len,
                         index_buffer_vk *buffer) {
  buffer_create(
      len, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      0, buffer);

  if (data) {
    buffer_update(buffer, 0, len, data);
  }
};

void uniform_buffer_create(const void *data, size_t len,
                           uniform_buffer_vk *buffer) {
  buffer_create(len, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, buffer);

  if (data) {
    buffer_update(buffer, 0, len, data);
  }
};

void storage_buffer_create(const void *data, size_t len,
                           storage_buffer_vk *buffer) {
  buffer_create(len, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, buffer);

  if (data) {
    buffer_update(buffer, 0, len, data);
  }
};

void ring_buffer_create(size_t size, VkBufferUsageFlags usage,
                        VmaAllocationCreateFlags allocation_flags,
                        ring_buffer_vk *out) {
  memset(out, 0, sizeof(ring_buffer_vk));

  buffer_create(size, usage, allocation_flags, &out->buffer);
  out->size = size;
}

void transient_buffer_allocate(ring_buffer_vk *pool, const void *data,
                               size_t len, mgfx_transient_buffer *out) {
  size_t free_size = pool->size - pool->head;

  // Pad to prevent wrap around
  uint32_t padding = 0;
  uint32_t offset = 0;

  if (len > free_size) {
    padding = free_size;

    uint32_t overflow = len - free_size;
    if (overflow > pool->tail) {
      // TODO:Resize
      MX_ASSERT(MX_FALSE, "[TransientBuffer] allocation failed. Must consume "
                          "allocated transient buffers!");
    }

    offset = 0;
  } else {
    offset = pool->head;
  }

  buffer_update(&pool->buffer, offset, len, data);

  *out = (mgfx_transient_buffer){
      .requested_size = (uint32_t)len,
      .actual_size = (uint32_t)len + padding,
      .offset = offset,
      .buffer_handle = (mx_ptr_t)pool->buffer.handle,
  };

  pool->head = (uint32_t)((pool->head + len + padding) % pool->size);
}

void transient_vertex_buffer_free(const mgfx_transient_buffer *tvb) {
  s_transient_vertex_buffer_pool.tail =
      (s_transient_vertex_buffer_pool.tail + tvb->actual_size) %
      s_transient_vertex_buffer_pool.size;
}

void transient_index_buffer_free(const mgfx_transient_buffer *tib) {
  s_transient_index_buffer_pool.tail =
      (s_transient_index_buffer_pool.tail + tib->actual_size) %
      s_transient_index_buffer_pool.size;
}

void transient_staging_buffer_free(mgfx_transient_buffer *tsb) {
  s_transient_staging_buffer_pool.tail =
      (s_transient_staging_buffer_pool.tail + tsb->actual_size) %
      s_transient_staging_buffer_pool.size;
}

void framebuffer_create(uint32_t color_attachment_count,
                        image_vk *color_attachments, image_vk *depth_attachment,
                        framebuffer_vk *framebuffer) {

  framebuffer->color_attachment_count = color_attachment_count;

  for (uint32_t i = 0; i < color_attachment_count; i++) {
    framebuffer->color_attachments[i] = &color_attachments[i];
    image_create_view(framebuffer->color_attachments[i], VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_COLOR_BIT,
                      &framebuffer->color_attachment_views[i]);
  }

  if (depth_attachment) {
    framebuffer->depth_attachment = depth_attachment;
    image_create_view(framebuffer->depth_attachment, VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_DEPTH_BIT,
                      &framebuffer->depth_attachment_view);
  }
}

void framebuffer_destroy(framebuffer_vk *fb) {
  // TODO: Should be api agonstic level
  for (uint32_t i = 0; i < fb->color_attachment_count; i++) {
    fb->color_attachments[i] = NULL;
    vkDestroyImageView(s_device, fb->color_attachment_views[i], NULL);
  }

  if (fb->depth_attachment) {
    fb->depth_attachment = NULL;
    vkDestroyImageView(s_device, fb->depth_attachment_view, NULL);
  }
}

void shader_handle_struct(shader_vk *shader,
                          const SpvReflectTypeDescription *td) {
  for (uint32_t i = 0; i < td->member_count; i++) {
    if ((td->members[i].type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) ==
        SPV_REFLECT_TYPE_FLAG_STRUCT) {
      MX_LOG_TRACE("\t - type_name: %s", td->members[i].struct_member_name);
      shader_handle_struct(shader, &td->members[i]);
    } else {
      MX_LOG_TRACE("\t\t - %s", td->members[i].struct_member_name);
    }
  }
}

void shader_create(size_t length, const char *code, shader_vk *shader) {
  mx_allocator_t tmp = mx_arena_create(10 * MX_KB);

  if (length % sizeof(uint32_t) != 0) {
    MX_LOG_ERROR("Shader code size must be a multiple of 4!");
    return;
  }

  SpvReflectShaderModule module;
  SpvReflectResult result = spvReflectCreateShaderModule(length, code, &module);
  MX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

  MX_LOG_TRACE("Entry Point: %s (Stage: %u)", module.entry_points[0].name,
               module.entry_points[0].shader_stage);

  if (module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
    uint32_t input_count = 0;
    spvReflectEnumerateInputVariables(&module, &input_count, NULL);
    if (input_count > 0) {
      SpvReflectInterfaceVariable **input_variables =
          (mx_alloc(tmp, input_count * sizeof(SpvReflectInterfaceVariable)));
      spvReflectEnumerateInputVariables(&module, &input_count, input_variables);

      // Sort by location and filter out built in variables.
      const SpvReflectInterfaceVariable
          *sorted_iv[MGFX_VERTEX_ATTRIBUTE_COUNT] = {0};
      uint32_t sorted_iv_count = 0;
      for (uint32_t i = 0; i < input_count; i++) {
        const SpvReflectInterfaceVariable *input_variable = input_variables[i];

        if (input_variable->built_in != -1) {
          continue;
        }

        sorted_iv[input_variable->location] = input_variable;
        ++sorted_iv_count;
      }
      shader->vertex_attribute_count = sorted_iv_count;

      uint32_t input_offset = 0;
      for (uint32_t i = 0; i < MGFX_VERTEX_ATTRIBUTE_COUNT; i++) {
        const SpvReflectInterfaceVariable *input_variable = sorted_iv[i];

        if (!input_variable) {
          continue;
        }

        shader->vertex_attributes[i] = (VkVertexInputAttributeDescription){
            .location = input_variable->location,
            .binding = 0,
            .format = (VkFormat)input_variable->format,
            .offset = input_offset,
        };

        input_offset += vk_format_size((VkFormat)input_variable->format);

        MX_LOG_TRACE("input variable: %s", input_variable->name);
        MX_LOG_TRACE("\tlocation: (%u)", input_variable->location);
        MX_LOG_TRACE("\toffset: %u", input_variable->word_offset.location);
        MX_LOG_TRACE("\tformat: %u (%d bytes)", input_variable->format,
                     vk_format_size((VkFormat)input_variable->format));

        sorted_iv_count--;
        if (sorted_iv_count == 0) {
          break;
        };
      }

      if (shader->vertex_attribute_count > 0) {
        shader->vertex_binding = (VkVertexInputBindingDescription){
            .binding = 0,
            .stride = input_offset,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
      }
    }
  }

  uint32_t binding_count = 0;
  shader->ds_count = module.descriptor_set_count;
  result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, NULL);
  if (binding_count > 0) {
    MX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    SpvReflectDescriptorBinding **bindings =
        mx_alloc(tmp, binding_count * sizeof(SpvReflectDescriptorBinding *));
    spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings);

    for (uint32_t i = 0; i < binding_count; i++) {
      descriptor_set_info_vk *ds = &shader->ds_infos[bindings[i]->set];

      ds->bindings[bindings[i]->binding] = (VkDescriptorSetLayoutBinding){
          .binding = bindings[i]->binding,
          .descriptorType = (VkDescriptorType)bindings[i]->descriptor_type,
          .descriptorCount = 1,
          .stageFlags = module.shader_stage,
          .pImmutableSamplers = NULL,
      };

      ds->binding_count++;

      MX_LOG_TRACE("%s set(%d) binding:(%d)", bindings[i]->name,
                   bindings[i]->set, bindings[i]->binding);
      MX_LOG_TRACE("\ttype: %u", bindings[i]->descriptor_type);

      if ((bindings[i]->type_description->type_flags &
           SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT) {
        shader_handle_struct(shader, bindings[i]->type_description);
      }
    }
  }

  uint32_t pc_count = 0;
  result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, NULL);
  MX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
  if (pc_count > 0) {
    SpvReflectBlockVariable **push_constants =
        mx_alloc(tmp, pc_count * sizeof(SpvReflectBlockVariable));
    spvReflectEnumeratePushConstantBlocks(&module, &pc_count, push_constants);
    for (uint32_t block_idx = 0; block_idx < pc_count; block_idx++) {
      const SpvReflectBlockVariable *pc_block = push_constants[block_idx];

      MX_LOG_TRACE("pc block: %s", pc_block->name);
      MX_LOG_TRACE("\tstage: %u", module.shader_stage);
      MX_LOG_TRACE("\tsize: %u", push_constants[block_idx]->size);
      MX_LOG_TRACE("\tstride: %u", push_constants[block_idx]->array.stride);
      for (uint32_t member_idx = 0;
           member_idx < push_constants[block_idx]->member_count; member_idx++) {
        MX_LOG_TRACE("\t - %s",
                     push_constants[block_idx]->members[member_idx].name);
        MX_LOG_TRACE("\t\t - offset: %u",
                     push_constants[block_idx]->members[member_idx].offset);
        MX_LOG_TRACE("\t\t - size: %u",
                     push_constants[block_idx]->members[member_idx].size);
        MX_LOG_TRACE(
            "\t\t - stride: %u",
            push_constants[block_idx]->members[member_idx].array.stride);
        shader->pc_ranges[block_idx] = (VkPushConstantRange){
            .stageFlags = module.shader_stage,
            .offset = push_constants[block_idx]->offset,
            .size = push_constants[block_idx]->size,
        };
      }
      ++shader->pc_count;
    }
  }

  spvReflectDestroyShaderModule(&module);

  VkShaderModuleCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .codeSize = length,
      .pCode = (uint32_t *)(void *)code,
  };

  VK_CHECK(vkCreateShaderModule(s_device, &info, NULL, &shader->module));

  mx_arena_destroy(tmp);
};

void shader_destroy(shader_vk *shader) {
  vkDestroyShaderModule(s_device, shader->module, NULL);
}

void pipeline_create_graphics(const shader_vk *vs, const shader_vk *fs,
                              const framebuffer_vk *fb,
                              const mgfx_vertex_layout *vl,
                              mgfx_program *program) {
  MX_ASSERT(vs != NULL,
            "Graphics program requires at least a valid vertex shader!");

  const mgfx_shader_stage gfx_stages[] = {MGFX_SHADER_STAGE_VERTEX,
                                          MGFX_SHADER_STAGE_FRAGMENT};
  uint32_t shader_stage_count = fs != NULL ? 2 : 1;

  VkGraphicsPipelineCreateInfo info = {0};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.stageCount = shader_stage_count;

  VkPipelineShaderStageCreateInfo shader_stage_infos[2] = {0};

  shader_stage_infos[0] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vs->module,
      .pName = "main",
      .pSpecializationInfo = NULL,
  };

  if (fs) {
    shader_stage_infos[1] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fs->module,
        .pName = "main",
        .pSpecializationInfo = NULL,
    };
  }

  info.pStages = shader_stage_infos;

  VkVertexInputAttributeDescription
      flat_vertex_attribs[MGFX_VERTEX_ATTRIBUTE_COUNT];
  uint32_t flat_attrib_count = 0;

  for (int i = 0; i < MGFX_VERTEX_ATTRIBUTE_COUNT; i++) {
    if (flat_attrib_count == vs->vertex_attribute_count) {
      break;
    }

    if (vs->vertex_attributes[i].format == VK_FORMAT_UNDEFINED) {
      continue;
    }

    const mgfx_vertex_attribute attrib =
        (mgfx_vertex_attribute)vs->vertex_attributes[i].location;

    flat_vertex_attribs[flat_attrib_count++] =
        (VkVertexInputAttributeDescription){
            .location = attrib,
            .binding = vs->vertex_attributes[i].binding,
            .offset = vl->offsets[attrib],
            .format = vs->vertex_attributes[i].format,
        };
  }

  VkVertexInputBindingDescription vertex_binding_desc = {
      .binding = 0,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      .stride = vl->stride,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_binding_desc,
      .vertexAttributeDescriptionCount = vs->vertex_attribute_count,
      .pVertexAttributeDescriptions = flat_vertex_attribs,
  };
  info.pVertexInputState = &vertex_input_state_info;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info = {0};
  input_assembly_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state_info.pNext = NULL;
  input_assembly_state_info.flags = 0;
  input_assembly_state_info.topology =
      (VkPrimitiveTopology)program->primitive_topology;
  input_assembly_state_info.primitiveRestartEnable = VK_FALSE;
  info.pInputAssemblyState = &input_assembly_state_info;

  VkPipelineTessellationStateCreateInfo tesselation_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .patchControlPoints = 0,
  };
  info.pTessellationState = &tesselation_state_info;

  VkPipelineViewportStateCreateInfo viewport_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = NULL, // Part of dynamic state.
      .scissorCount = 1,
      .pScissors = NULL, // Part of dynamic state.
  };
  info.pViewportState = &viewport_state_info;

  VkPipelineRasterizationStateCreateInfo rasterization_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = (VkPolygonMode)program->polygon_mode,
      .cullMode = (VkCullModeFlags)program->cull_mode,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };
  info.pRasterizationState = &rasterization_state_info;

  VkPipelineMultisampleStateCreateInfo multisample_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0,
      .pSampleMask = NULL,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };
  info.pMultisampleState = &multisample_state_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info = {0};
  if (fb->depth_attachment) {
    depth_stencil_state_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {0},
        .back = {0},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };
  } else {
    depth_stencil_state_info = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {0},
        .back = {0},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };
  }
  info.pDepthStencilState = &depth_stencil_state_info;

  VkPipelineColorBlendAttachmentState
      color_blend_attachments[MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS];
  for (uint32_t color_attachment_idx = 0;
       color_attachment_idx < fb->color_attachment_count;
       color_attachment_idx++) {
    color_blend_attachments[color_attachment_idx] =
        (VkPipelineColorBlendAttachmentState){
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
  }

  VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = fb->color_attachment_count,
      .pAttachments = color_blend_attachments,
  };
  info.pColorBlendState = &color_blend_state_info;

  const VkDynamicState k_dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR};
  const uint32_t k_dynamic_state_count =
      sizeof(k_dynamic_states) / sizeof(VkDynamicState);

  VkPipelineDynamicStateCreateInfo dynamic_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .dynamicStateCount = k_dynamic_state_count,
      .pDynamicStates = k_dynamic_states,
  };
  info.pDynamicState = &dynamic_state_info;

  int ds_count = 0;

  VkDescriptorSetLayoutBinding
      flat_bindings[MGFX_SHADER_MAX_DESCRIPTOR_SET *
                    MGFX_SHADER_MAX_DESCRIPTOR_BINDING] = {0};

  uint32_t max_bindings[MGFX_SHADER_MAX_DESCRIPTOR_SET *
                        MGFX_SHADER_MAX_DESCRIPTOR_BINDING] = {0};

  VkDescriptorSetLayout ds_layouts[MGFX_SHADER_MAX_DESCRIPTOR_SET];
  memset(ds_layouts, 0,
         sizeof(VkDescriptorSetLayout) * MGFX_SHADER_MAX_DESCRIPTOR_SET);

  VkPushConstantRange flat_pc_ranges[MGFX_SHADER_MAX_PUSH_CONSTANTS];
  memset(flat_pc_ranges, 0,
         sizeof(VkPushConstantRange) * MGFX_SHADER_MAX_PUSH_CONSTANTS);
  int flat_pc_range_count = 0;

  // Resolve multi stage descriptor bindings and push constants
  for (int stage_idx = 0; stage_idx < 2; stage_idx++) {
    const shader_vk *shader = (stage_idx == 0) ? vs : fs;

    if (shader == NULL) {
      continue;
    }

    ds_count = shader->ds_count > ds_count ? shader->ds_count : ds_count;

    for (int ds_idx = 0; ds_idx < shader->ds_count; ds_idx++) {
      const descriptor_set_info_vk *ds = &shader->ds_infos[ds_idx];

      if (ds->binding_count <= 0) {
        continue;
      };

      uint32_t binding = 0;
      for (int processed_bindings = 0; processed_bindings < ds->binding_count;
           binding++) {
        MX_ASSERT(binding < MGFX_SHADER_MAX_DESCRIPTOR_BINDING);

        // Skip unbound bindings
        if (ds->bindings[binding].stageFlags == 0) {
          continue;
        };

        max_bindings[ds_idx] =
            binding > max_bindings[ds_idx] ? binding : max_bindings[ds_idx];

        VkDescriptorSetLayoutBinding *ds_binding =
            &flat_bindings[ds_idx * MGFX_SHADER_MAX_DESCRIPTOR_BINDING +
                           binding];

        if (ds_binding->stageFlags != 0) {
          ds_binding->stageFlags |= stage_idx == 0
                                        ? VK_SHADER_STAGE_VERTEX_BIT
                                        : VK_SHADER_STAGE_FRAGMENT_BIT;
        } else {
          *ds_binding = ds->bindings[binding];
        }

        processed_bindings++;
      }
    }

    // TODO: Check for built in mgfx pc, for bindless textures
    for (int pc_index = 0; pc_index < shader->pc_count; pc_index++) {
      flat_pc_ranges[flat_pc_range_count] = shader->pc_ranges[pc_index];
      ++flat_pc_range_count;
    }
  }

  for (int ds_idx = 0; ds_idx < ds_count; ds_idx++) {
    VkDescriptorSetLayoutCreateInfo dsl_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = max_bindings[ds_idx] + 1,
        .pBindings =
            &flat_bindings[ds_idx * MGFX_SHADER_MAX_DESCRIPTOR_BINDING],
    };

    VK_CHECK(vkCreateDescriptorSetLayout(
        s_device, &dsl_info, NULL,
        (VkDescriptorSetLayout *)&program->dsls[ds_idx]));
    ds_layouts[ds_idx] = (VkDescriptorSetLayout)program->dsls[ds_idx];
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .setLayoutCount = ds_count,
      .pSetLayouts = ds_layouts,
      .pushConstantRangeCount = flat_pc_range_count,
      .pPushConstantRanges = flat_pc_ranges,
  };

  VK_CHECK(
      vkCreatePipelineLayout(s_device, &pipeline_layout_info, NULL,
                             (VkPipelineLayout *)&program->pipeline_layout));
  info.layout = (VkPipelineLayout)program->pipeline_layout;

  // Dynamic Rendering.
  info.renderPass = NULL;

  VkFormat color_attachment_formats[MGFX_FRAMEBUFFER_MAX_COLOR_ATTACHMENTS] = {
      0};
  for (uint32_t i = 0; i < fb->color_attachment_count; i++) {
    color_attachment_formats[i] = fb->color_attachments[i]->format;
  }

  VkPipelineRenderingCreateInfoKHR rendering_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = NULL,
      .viewMask = 0,
      .colorAttachmentCount = fb->color_attachment_count,
      .pColorAttachmentFormats = color_attachment_formats,
      .depthAttachmentFormat = fb->depth_attachment
                                   ? fb->depth_attachment->format
                                   : VK_FORMAT_UNDEFINED,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
  };
  info.pNext = &rendering_create_info;

  VK_CHECK(vkCreateGraphicsPipelines(s_device, NULL, 1, &info, NULL,
                                     (VkPipeline *)&program->pipeline));
}

void pipeline_destroy(mgfx_program *program) {
  for (uint32_t descriptor_idx = 0;
       descriptor_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET; descriptor_idx++) {
    if ((VkDescriptorSetLayout)program->dsls[descriptor_idx] ==
        VK_NULL_HANDLE) {
      continue;
    }

    vkDestroyDescriptorSetLayout(
        s_device, (VkDescriptorSetLayout)program->dsls[descriptor_idx], NULL);
  }

  vkDestroyPipelineLayout(s_device, (VkPipelineLayout)program->pipeline_layout,
                          NULL);
  vkDestroyPipeline(s_device, (VkPipeline)program->pipeline, NULL);
}

mx_bool swapchain_update(const frame_vk *frame, int width, int height,
                         swapchain_vk *sc) {
  if (sc->resize == MX_TRUE) {
    vkDeviceWaitIdle(s_device);

    swapchain_destroy(&s_swapchain);

    if (s_surface_caps.currentExtent.width == UINT32_MAX) {
      width = (int)mx_clamp((float)width,
                            (float)s_surface_caps.minImageExtent.width,
                            (float)s_surface_caps.maxImageExtent.width);
      height = (int)mx_clamp((float)height,
                             (float)s_surface_caps.minImageExtent.height,
                             (float)s_surface_caps.maxImageExtent.height);
    }

    // TODO: Memory leak
    swapchain_create(s_surface, width, height, &s_swapchain,
                     mx_default_allocator());
    sc->resize = MX_FALSE;
  }

  // Wait for available next swapchain image.
  VkResult acquire_sc_image_result = vkAcquireNextImageKHR(
      s_device, s_swapchain.handle, UINT64_MAX, frame->swapchain_semaphore,
      VK_NULL_HANDLE, &sc->free_idx);

  switch (acquire_sc_image_result) {
  case (VK_SUBOPTIMAL_KHR):
    break;
  case (VK_ERROR_OUT_OF_DATE_KHR): {
    sc->resize = MX_TRUE;
    return MX_FALSE;
  } break;

  default:
    VK_CHECK(acquire_sc_image_result);
    break;
  }

  sc->framebuffer = (framebuffer_vk){
      .color_attachments = &s_swapchain.images[sc->free_idx],
      .color_attachment_views = s_swapchain.image_views[sc->free_idx],
      .color_attachment_count = 1,

      .depth_attachment = NULL,
      .depth_attachment_view = NULL,
  };

  return MX_TRUE;
}

// ~ VULKAN RENDERER  ~ //

void mgfx_vertex_layout_begin(mgfx_vertex_layout *vl) {
  memset(vl, 0, sizeof(mgfx_vertex_layout));
  memset(vl->attributes, MGFX_VERTEX_ATTRIBUTE_MAX, sizeof(vl->attributes));
}

void mgfx_vertex_layout_add(mgfx_vertex_layout *vl,
                            mgfx_vertex_attribute attribute, size_t size) {
  if (vl->sizes[attribute] > 0) {
    MX_LOG_WARN("Vertex Attribute %d already assigned!");
    return;
  };

  vl->offsets[attribute] = vl->stride;

  vl->sizes[attribute] = size;
  vl->stride += size;

  vl->attributes[vl->attribute_count++] = attribute;
};

void mgfx_vertex_layout_end(mgfx_vertex_layout *vl) {}

const mgfx_pntuc32f MGFX_FS_QUAD_VERTICES[4] = {
    // Vertex 0: Bottom-left
    {.position = {-1.0f, -1.0f, 1.0f}, .uv = {0.0f, 1.0f}},

    // Vertex 1: Bottom-right
    {.position = {1.0f, -1.0f, 1.0f}, .uv = {1.0f, 1.0f}},

    // Vertex 2: Top-right
    {.position = {1.0f, 1.0f, 1.0f}, .uv = {1.0f, 0.0f}},

    // Vertex 3: Top-left
    {.position = {-1.0f, 1.0f, 1.0f}, .uv = {0.0f, 0.0f}},
};
mgfx_vertex_layout MGFX_PNTU32F_LAYOUT;

const uint32_t MGFX_FS_QUAD_INDICES[6] = {0, 1, 2, 2, 3, 0};

typedef struct mgfx_draw {
  struct descriptor_sets {
    mgfx_dh dhs[MGFX_SHADER_MAX_DESCRIPTOR_BINDING];
    uint32_t dh_count;
  } desc_sets[MGFX_SHADER_MAX_DESCRIPTOR_SET];

  mgfx_vbh vbh;
  mgfx_ibh ibh;

  mgfx_transient_vb tvb;
  mgfx_transient_buffer tib;

  mgfx_ph ph;
  uint8_t view_target;

  uint64_t sort_key;
} mgfx_draw;

static int draw_compare_fn(const void *a, const void *b) {
  const mgfx_draw *draw_a = (mgfx_draw *)a;
  const mgfx_draw *draw_b = (mgfx_draw *)b;

  return draw_a->sort_key > draw_b->sort_key ? 1 : -1;
}

typedef struct buffer_entry {
  VkBuffer key;
  buffer_vk value;
  UT_hash_handle hh;
} buffer_entry;
static buffer_entry *s_buffer_table;

typedef struct image_entry {
  VkImage key;
  image_vk value;
  UT_hash_handle hh;
} image_entry;
static image_entry *s_image_table;

typedef struct shader_entry {
  VkShaderModule key;
  shader_vk value;
  UT_hash_handle hh;
} shader_entry;
static shader_entry *s_shader_table;

typedef struct texture_entry {
  mgfx_th key;
  mgfx_texture value;
  UT_hash_handle hh;
} texture_entry;
static texture_entry *s_texture_table;

typedef struct program_entry {
  mgfx_ph key;
  mgfx_program value;
  UT_hash_handle hh;
} program_entry;
static program_entry *s_program_table;

typedef struct descriptor_entry {
  mgfx_dh key;
  descriptor_info_vk value;
  UT_hash_handle hh;
} descriptor_entry;
static descriptor_entry *s_descriptor_table;

typedef struct descriptor_set_entry {
  uint32_t key;
  VkDescriptorSet value;
  UT_hash_handle hh;
} descriptor_set_entry;
static descriptor_set_entry *s_descriptor_set_table;

typedef struct framebuffer_entry {
  mgfx_fbh key;
  framebuffer_vk value;
  UT_hash_handle hh;
} framebuffer_entry;
static framebuffer_entry *s_framebuffer_table;

typedef struct vb_info {
  mx_str name;
  mgfx_vertex_layout vl;
} vb_info;

mx_map_t(mgfx_vbh, vb_info, 1000) vb_infos;
mx_map_find_fn(vb_info_find, mgfx_vbh, vb_info);
mx_map_insert_fn(vb_info_insert, mgfx_vbh, vb_info);

mgfx_fbh s_view_targets[0xFF];
VkClearColorValue s_view_clears[0XFF] = {0};

uint32_t s_width;
uint32_t s_height;

enum { MGFX_MAX_DRAW_COUNT = 256 };
static mgfx_draw s_draws[MGFX_MAX_DRAW_COUNT];
static uint32_t s_draw_count = 0;

typedef struct transform_data {
  mx_mat4 transform;
  mx_mat4 view;
  mx_mat4 proj;

  mx_mat4 inverse_transform;
  mx_mat4 inverse_view;
} transform_data;
static mgfx_sbh transform_storage_buffer;
static mgfx_dh u_transforms;
static transform_data transforms[MGFX_MAX_DRAW_COUNT];

static transform_data current_transform;

// Built in assets
mgfx_th MGFX_WHITE_TEXTURE;
mgfx_th MGFX_BLACK_TEXTURE;
mgfx_th MGFX_LOCAL_NORMAL_TEXTURE;

// Debug Text
enum { MGFX_DEBUG_MAX_TEXT = 64 };
static const uint32_t atlas_w = 512;
static const uint32_t atlas_h = 512;

static const char *font_path = MGFX_ASSET_PATH "fonts/Roboto-Regular.ttf";
stbtt_packedchar chars[96]; // For ASCII 32..127

mgfx_sh dbg_ui_vsh;
mgfx_sh dbg_ui_fsh;
mgfx_ph dbg_ui_ph;

mgfx_th dbg_ui_font_atlas_th;
mgfx_dh dbg_ui_font_atlas_dh;

// Debug Gizmos
mgfx_vbh dbg_quad_vbh;
mgfx_ibh dbg_quad_ibh;

static mx_allocator_t mgfx_allocator;

size_t mgfx_format_size(mgfx_format format) {
  switch (format) {
  case MGFX_FORMAT_D32_SFLOAT:
    return 4;
  case MGFX_FORMAT_R16G16B16A16_SFLOAT:
    return 4 * 2;
  default:
    MX_LOG_ERROR("[MX] Uknown format  (%u)", format);
    return 0;
  }
}

int mgfx_init(const mgfx_init_info *info) {
  mx_allocator_t tmp = mx_arena_create(MX_MB);

  VkApplicationInfo app_info = {0};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = NULL;
  app_info.pApplicationName = info->name;
  app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
  app_info.pEngineName = "MGFX";
  app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);

  uint32_t api_version;
  vkEnumerateInstanceVersion(&api_version);

#ifdef MX_WIN32
  app_info.apiVersion = VK_API_VERSION_1_3;
#else
  app_info.apiVersion = VK_API_VERSION_1_2;
#endif

  VkInstanceCreateInfo instance_info = {0};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pNext = NULL;
  instance_info.pApplicationInfo = &app_info;
#ifdef MX_MACOS
  instance_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  // Check if required extensions are available.
  uint32_t avail_prop_count = 0;
  VK_CHECK(
      vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count, NULL));
  VkExtensionProperties *avail_props =
      mx_alloc(tmp, avail_prop_count * sizeof(VkExtensionProperties));
  VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count,
                                                  avail_props));

  for (int req_index = 0; req_index < k_req_ext_count; req_index++) {
    int validated = -1;
    for (uint32_t avail_index = 0; avail_index < avail_prop_count;
         avail_index++) {
      if (strcmp(k_req_exts[req_index],
                 avail_props[avail_index].extensionName) == 0) {
        validated = MX_SUCCESS;
        continue;
      }
    }

    if (validated != MX_SUCCESS) {
      MX_LOG_ERROR("Extension required not supported: %s!\n",
                   k_req_exts[req_index]);
      return -1;
    }
  }

  instance_info.enabledExtensionCount = k_req_ext_count;
  instance_info.ppEnabledExtensionNames = k_req_exts;

#ifdef MX_DEBUG
  // Check and enable validation layers.
  uint32_t layer_prop_count;
  vkEnumerateInstanceLayerProperties(&layer_prop_count, NULL);
  VkLayerProperties *layer_props =
      mx_alloc(tmp, sizeof(VkLayerProperties) * layer_prop_count);
  vkEnumerateInstanceLayerProperties(&layer_prop_count, layer_props);

  const char *validationLayers = {"VK_LAYER_KHRONOS_validation"};

  for (uint32_t i = 0; i < layer_prop_count; i++) {
    if (strcmp(validationLayers, layer_props[i].layerName) == 0) {
      MX_LOG_WARN("Validation layers enabled");
      instance_info.enabledLayerCount = 1;
      instance_info.ppEnabledLayerNames = &validationLayers;
      break;
    }
  }
#endif

  VK_CHECK(vkCreateInstance(&instance_info, NULL, &s_instance));

  // Extension functions.
  vk_cmd_begin_rendering_khr =
      (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(
          s_instance, "vkCmdBeginRenderingKHR");
  vk_cmd_end_rendering_khr = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(
      s_instance, "vkCmdEndRenderingKHR");

#ifdef MX_DEBUG
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {0};
  debug_messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_messenger_info.pfnUserCallback = vk_debug_callback;
  debug_messenger_info.pUserData = NULL;

  create_debug_util_messenger_ext(s_instance, &debug_messenger_info, NULL,
                                  &s_debug_messenger);
#endif
  if (choose_physical_device_vk(s_instance, k_req_device_ext_count,
                                k_req_device_ext_names, &s_phys_device_props,
                                &s_phys_device, tmp) != VK_SUCCESS) {
    MX_LOG_ERROR("Failed to find suitable physical device!");
    return -1;
  }

  VK_CHECK(get_window_surface_vk(s_instance, info->nwh, &s_surface));

  uint32_t queue_family_props_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device,
                                           &queue_family_props_count, NULL);
  VkQueueFamilyProperties *queue_family_props =
      mx_alloc(tmp, sizeof(VkQueueFamilyProperties) * queue_family_props_count);

  vkGetPhysicalDeviceQueueFamilyProperties(
      s_phys_device, &queue_family_props_count, queue_family_props);

  int unique_queue_count = 0;
  for (uint32_t i = 0; i < queue_family_props_count; i++) {
    if ((queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ==
        VK_QUEUE_GRAPHICS_BIT) {
      s_queue_indices[VK_QUEUE_GRAPHICS] = i;
    }

    VkBool32 present_supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(s_phys_device, i, s_surface,
                                         &present_supported);
    if (present_supported == VK_TRUE) {
      s_queue_indices[VK_QUEUE_PRESENT] = i;
    }

    ++unique_queue_count;

    // Check if complete.
    int complete = MGFX_SUCCESS;
    for (uint16_t j = 0; j < unique_queue_count; j++) {
      if (s_queue_indices[VK_QUEUE_PRESENT] == -1) {
        complete = !MGFX_SUCCESS;
      }
    }

    if (complete == MGFX_SUCCESS) {
      break;
    }
  }

  for (uint16_t i = 0; i < VK_QUEUE_COUNT; i++) {
    if (s_queue_indices[VK_QUEUE_PRESENT] == -1) {
      MX_LOG_ERROR("Failed to find graphics queue!");
      break;
    }
  }

  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_infos[VK_QUEUE_COUNT] = {0};
  for (uint16_t i = 0; i < unique_queue_count; i++) {
    queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[i].pNext = NULL;
    queue_infos[i].flags = 0;
    queue_infos[i].queueFamilyIndex = s_queue_indices[i];
    queue_infos[i].queueCount = 1;
    queue_infos[i].pQueuePriorities = &queue_priority;
  }

  VkPhysicalDeviceFeatures phys_device_features = {0};
  phys_device_features.fillModeNonSolid = VK_TRUE;

  VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
      .dynamicRendering = VK_TRUE,
  };

  VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &dynamic_rendering_features,
      .flags = 0,
      .queueCreateInfoCount = unique_queue_count,
      .pQueueCreateInfos = queue_infos,
      .enabledExtensionCount = k_req_device_ext_count,
      .ppEnabledExtensionNames = k_req_device_ext_names,
      .pEnabledFeatures = &phys_device_features,
  };
  VK_CHECK(vkCreateDevice(s_phys_device, &device_info, NULL, &s_device));

  for (uint16_t i = 0; i < unique_queue_count; i++) {
    vkGetDeviceQueue(s_device, s_queue_indices[i], 0, &s_queues[i]);
  }

#ifdef MX_MACOS
  PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
#endif

  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys_device, s_surface,
                                                     &s_surface_caps));

  uint32_t surface_fmt_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys_device, s_surface,
                                       &surface_fmt_count, NULL);
  VkSurfaceFormatKHR *surface_fmts =
      mx_alloc(tmp, surface_fmt_count * sizeof(VkSurfaceFormatKHR));
  vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys_device, s_surface,
                                       &surface_fmt_count, surface_fmts);

  int surface_format_found = -1;
  for (uint32_t i = 0; i < surface_fmt_count; i++) {
    if (surface_fmts[i].format == k_surface_fmt &&
        surface_fmts[i].colorSpace == k_surface_color_space) {
      surface_format_found = MGFX_SUCCESS;
      break;
    }
  }

  MX_ASSERT(surface_format_found == MGFX_SUCCESS,
            "Required surface format not found!");

  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys_device, s_surface,
                                            &present_mode_count, NULL);
  VkPresentModeKHR *present_modes =
      mx_alloc(tmp, surface_fmt_count * sizeof(VkPresentModeKHR));
  vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys_device, s_surface,
                                            &present_mode_count, present_modes);
  for (uint32_t i = 0; i < present_mode_count; i++) {
  }

  VkExtent2D swapchain_extent;
  choose_swapchain_extent_vk(&s_surface_caps, info->nwh, &swapchain_extent);

  swapchain_create(s_surface, swapchain_extent.width, swapchain_extent.height,
                   &s_swapchain, tmp);

  VmaAllocatorCreateInfo allocator_info = {
      .instance = s_instance,
      .physicalDevice = s_phys_device,
      .device = s_device,
      /*.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,*/
  };
  VK_CHECK(vmaCreateAllocator(&allocator_info, &s_allocator));

  for (int i = 0; i < MGFX_FRAME_COUNT; i++) {
    VkCommandPoolCreateInfo gfx_cmd_pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = s_queue_indices[VK_QUEUE_GRAPHICS],
    };
    VK_CHECK(vkCreateCommandPool(s_device, &gfx_cmd_pool, NULL,
                                 &s_frames[i].cmd_pool));

    VkCommandBufferAllocateInfo buffer_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = s_frames[i].cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(s_device, &buffer_alloc_info,
                                      &s_frames[i].cmd));

    VkSemaphoreCreateInfo swapchain_semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VK_CHECK(vkCreateSemaphore(s_device, &swapchain_semaphore_info, NULL,
                               &s_frames[i].swapchain_semaphore));

    VkSemaphoreCreateInfo render_semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VK_CHECK(vkCreateSemaphore(s_device, &render_semaphore_info, NULL,
                               &s_frames[i].render_semaphore));

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_CHECK(
        vkCreateFence(s_device, &fence_info, NULL, &s_frames[i].render_fence));
  }

  VkDescriptorPoolCreateInfo ds_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .maxSets = 300,
      .poolSizeCount = k_ds_pool_sizes_count,
      .pPoolSizes = k_ds_pool_sizes,
  };
  VK_CHECK(vkCreateDescriptorPool(s_device, &ds_pool_info, NULL, &s_ds_pool));

  // Initialize staging buffers
  enum { MGFX_MAX_STAGING_BUFFER_SIZE = MX_MB * 500 };
  ring_buffer_create(MGFX_MAX_STAGING_BUFFER_SIZE,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                     &s_transient_staging_buffer_pool);

  ring_buffer_create(MGFX_DEBUG_MAX_TEXT * 4 * 48,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     0, &s_transient_vertex_buffer_pool);

  ring_buffer_create(MGFX_DEBUG_MAX_TEXT * 6 * sizeof(uint32_t),
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     0, &s_transient_index_buffer_pool);

  memset(s_view_targets, 0, sizeof(mgfx_fbh) * 0xFF);

  // Init mgfx
  mgfx_set_view_clear(MGFX_DEFAULT_VIEW_TARGET,
                      (float[4]){0.0f, 0.0f, 0.0f, 1.0f});

  transform_storage_buffer = mgfx_storage_buffer_create(
      NULL, sizeof(transform_data) * MGFX_MAX_DRAW_COUNT);
  u_transforms =
      mgfx_descriptor_create("u_transforms", MGFX_UNIFORM_TYPE_STORAGE_BUFFER);
  mgfx_set_buffer(u_transforms, (mgfx_ubh){transform_storage_buffer.idx});

  // Init primitives
  mgfx_vertex_layout_begin(&MGFX_PNTU32F_LAYOUT);
  mgfx_vertex_layout_add(&MGFX_PNTU32F_LAYOUT, MGFX_VERTEX_ATTRIBUTE_POSITION,
                         sizeof(mx_vec3));
  mgfx_vertex_layout_add(&MGFX_PNTU32F_LAYOUT, MGFX_VERTEX_ATTRIBUTE_NORMAL,
                         sizeof(mx_vec3));
  mgfx_vertex_layout_add(&MGFX_PNTU32F_LAYOUT, MGFX_VERTEX_ATTRIBUTE_TEXCOORD0,
                         sizeof(mx_vec2));
  mgfx_vertex_layout_add(&MGFX_PNTU32F_LAYOUT, MGFX_VERTEX_ATTRIBUTE_COLOR,
                         sizeof(mx_vec4));
  mgfx_vertex_layout_add(&MGFX_PNTU32F_LAYOUT, MGFX_VERTEX_ATTRIBUTE_TANGENT,
                         sizeof(mx_vec4));
  mgfx_vertex_layout_end(&MGFX_PNTU32F_LAYOUT);

  // Init debug text
  dbg_ui_vsh =
      mgfx_shader_create(MGFX_ASSET_PATH "shaders/mgfxDebugText.vert.spv");
  dbg_ui_fsh =
      mgfx_shader_create(MGFX_ASSET_PATH "shaders/mgfxDebugText.frag.spv");
  dbg_ui_ph = mgfx_program_create_graphics(dbg_ui_vsh, dbg_ui_fsh,
                                           {.name = "mgfx_debug_text"});

  size_t font_file_size;
  if (mx_read_file(font_path, &font_file_size, NULL) != MX_SUCCESS) {
    MX_LOG_ERROR("Failed to load font: %s!", font_path);
    exit(-1);
  }

  unsigned char *ttf_buffer = mx_alloc(tmp, font_file_size); // load .ttf file
  mx_read_file(font_path, &font_file_size, ttf_buffer);

  unsigned char atlas_bitmap[512 * 512];

  stbtt_pack_context context;
  stbtt_PackBegin(&context, atlas_bitmap, atlas_w, atlas_h, 0, 1, NULL);
  stbtt_PackFontRange(&context, ttf_buffer, 0, 32.0f, 32, 96, chars);

  // Done packing
  stbtt_PackEnd(&context);

  stbtt_fontinfo font;
  stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));

  const mgfx_image_info img_info = {
      .format = MGFX_FORMAT_R8_UNORM,
      .width = atlas_w,
      .height = atlas_h,
      .layers = 1,
      .cube_map = MX_FALSE,
  };

  dbg_ui_font_atlas_th = mgfx_texture_create_from_memory(
      &img_info, VK_FILTER_LINEAR, atlas_bitmap, atlas_w * atlas_h);
  dbg_ui_font_atlas_dh = mgfx_descriptor_create(
      "u_diffuse", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  mgfx_set_texture(dbg_ui_font_atlas_dh, dbg_ui_font_atlas_th);

  const mgfx_image_info texture_info = {
      .format = MGFX_FORMAT_R8G8B8A8_UNORM,
      .width = 1,
      .height = 1,
      .layers = 1,
      .cube_map = MX_FALSE,
  };

  MGFX_WHITE_TEXTURE = mgfx_texture_create_from_memory(
      &texture_info, VK_FILTER_NEAREST, (uint8_t[4]){255, 255, 255, 255}, 4);

  MGFX_BLACK_TEXTURE = mgfx_texture_create_from_memory(
      &texture_info, VK_FILTER_NEAREST, (uint8_t[4]){0, 0, 0, 0}, 4);

  MGFX_LOCAL_NORMAL_TEXTURE = mgfx_texture_create_from_memory(
      &texture_info, VK_FILTER_NEAREST, (uint8_t[4]){128, 128, 255, 255}, 4);

  // Init gizmos
  dbg_quad_vbh = mgfx_vertex_buffer_create(MGFX_FS_QUAD_VERTICES,
                                           sizeof(MGFX_FS_QUAD_VERTICES),
                                           &MGFX_PNTU32F_LAYOUT);
  dbg_quad_ibh = mgfx_index_buffer_create(MGFX_FS_QUAD_INDICES,
                                          sizeof(MGFX_FS_QUAD_INDICES));

  MX_LOG_SUCCESS("MGFX Initialized!");
  return MGFX_SUCCESS;

  mx_arena_destroy(tmp);
}

mgfx_vbh mgfx_vertex_buffer_create(const void *data, size_t len,
                                   const mgfx_vertex_layout *vl) {
  if (len <= 0) {
    MX_LOG_WARN("Attempting to create vertex buffer with size 0!");
    return (mgfx_vbh){.idx = MGFX_INVALID_HANDLE};
  };

  if (!vl) {
    MX_LOG_WARN("Attempting to create vertex buffer with null vertex layout!");
    return (mgfx_vbh){.idx = MGFX_INVALID_HANDLE};
  };

  buffer_entry *entry = mx_alloc(mx_default_allocator(), sizeof(buffer_entry));
  memset(entry, 0, sizeof(buffer_entry));
  vertex_buffer_create(data, len, &entry->value);

  entry->key = entry->value.handle;
  HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_vbh), entry);

  vb_info_insert(&vb_infos, (mgfx_vbh){.idx = (uint64_t)entry->key},
                 &(vb_info){.vl = *vl});

  return (mgfx_vbh){.idx = (uint64_t)entry->key};
}

void mgfx_transient_vertex_buffer_allocate(const void *data, size_t len,
                                           mgfx_transient_vb *out) {
  transient_buffer_allocate(&s_transient_vertex_buffer_pool, data, len,
                            &out->tb);
}

void mgfx_transient_index_buffer_allocate(const void *data, size_t len,
                                          mgfx_transient_buffer *out) {
  transient_buffer_allocate(&s_transient_index_buffer_pool, data, len, out);
}

mgfx_ibh mgfx_index_buffer_create(const void *data, size_t len) {
  buffer_entry *entry = mx_alloc(mx_default_allocator(), sizeof(buffer_entry));
  memset(entry, 0, sizeof(buffer_entry));

  index_buffer_create(data, len, &entry->value);

  entry->key = entry->value.handle;
  HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_ibh), entry);

  return (mgfx_ibh){.idx = (uint64_t)entry->key};
}

mgfx_ubh mgfx_uniform_buffer_create(const void *data, size_t len) {
  buffer_entry *entry = mx_alloc(MX_DEFAULT_ALLOCATOR, sizeof(buffer_entry));
  uniform_buffer_create(data, len, &entry->value);

  entry->key = entry->value.handle;
  HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_ubh), entry);

  return (mgfx_ubh){.idx = (uint64_t)entry->key};
}

MX_API MX_NO_DISCARD mgfx_sbh mgfx_storage_buffer_create(const void *data,
                                                         size_t len) {
  buffer_entry *entry = mx_alloc(MX_DEFAULT_ALLOCATOR, sizeof(buffer_entry));
  storage_buffer_create(data, len, &entry->value);

  entry->key = entry->value.handle;
  HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_sbh), entry);

  return (mgfx_sbh){.idx = (uint64_t)entry->key};
}

void mgfx_buffer_update(uint64_t buffer_idx, const void *data, size_t len,
                        size_t offset) {
  buffer_entry *entry;
  HASH_FIND(hh, s_buffer_table, &buffer_idx, sizeof(uint64_t), entry);

  MX_ASSERT(entry != NULL, "Buffer invalid handle!");

  buffer_update(&entry->value, offset, len, data);
}

void mgfx_buffer_destroy(uint64_t idx) {
  vkDeviceWaitIdle(s_device);
  buffer_entry *entry;
  HASH_FIND(hh, s_buffer_table, &idx, sizeof(idx), entry);

  MX_ASSERT(entry != NULL, "Buffer invalid handle!");
  buffer_destroy(&entry->value);
}

mgfx_sh mgfx_shader_create(const char *path) {
  size_t size;
  if (mx_read_file(path, &size, NULL) != MX_SUCCESS) {
    MX_LOG_ERROR("Failed to load shader: %s!", path);
    exit(-1);
  }

  char *shader_code = mx_alloc(mx_default_allocator(), size);
  mx_read_file(path, &size, shader_code);

  shader_entry *entry = mx_alloc(mx_default_allocator(), sizeof(shader_entry));
  memset(entry, 0, sizeof(shader_entry));

  MX_LOG_TRACE("%s...", path);
  shader_create(size, shader_code, &entry->value);

  entry->key = entry->value.module;
  HASH_ADD(hh, s_shader_table, key, sizeof(VkShaderModule), entry);

  return (mgfx_sh){.idx = (uint64_t)entry->key};
}

void mgfx_shader_destroy(mgfx_sh sh) {
  shader_entry *entry;
  HASH_FIND(hh, s_shader_table, &sh, sizeof(sh), entry);

  MX_ASSERT(entry != NULL, "Shader invalid handle!");

  shader_destroy(&entry->value);

  HASH_DEL(s_shader_table, entry);
  mx_free(mx_default_allocator(), entry);
}

mgfx_ph
mgfx_program_create_graphics_impl(mgfx_sh vsh, mgfx_sh fsh,
                                  const mgfx_graphics_create_info *ex_info) {
  program_entry *entry =
      mx_alloc(mx_default_allocator(), sizeof(program_entry));
  memset(entry, 0, sizeof(program_entry));
  entry->key.idx = (uint64_t)entry;

  entry->value.shaders[MGFX_SHADER_STAGE_VERTEX] = vsh;
  entry->value.shaders[MGFX_SHADER_STAGE_FRAGMENT] = fsh;

  switch (ex_info->polygon_mode) {
  case MGFX_FILL:
    entry->value.polygon_mode = VK_POLYGON_MODE_FILL;
    break;
  case MGFX_LINE:
    entry->value.polygon_mode = VK_POLYGON_MODE_LINE;
    break;
  }

  switch (ex_info->primitive_topology) {
  case MGFX_TRIANGLE_LIST:
    entry->value.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    break;
  }

  entry->value.cull_mode = ex_info->cull_mode;

  if (ex_info->name.data != NULL) {
    memcpy(entry->value.name, ex_info->name.c_str, mx_strlen(ex_info->name));
  };

  if (ex_info->instanced) {
  }

  HASH_ADD(hh, s_program_table, key, sizeof(entry->key), entry);

  return entry->key;
}

mgfx_ph mgfx_program_create_compute(mgfx_sh csh) {
  program_entry *entry =
      mx_alloc(mx_default_allocator(), sizeof(program_entry));
  memset(entry, 0, sizeof(program_entry));

  entry->key.idx = (uint64_t)entry;
  entry->value.shaders[MGFX_SHADER_STAGE_COMPUTE] = csh;

  HASH_ADD(hh, s_program_table, key, sizeof(entry->key), entry);

  return entry->key;
}

void mgfx_program_destroy(mgfx_ph ph) {
  program_entry *entry;
  HASH_FIND(hh, s_program_table, &ph, sizeof(ph), entry);

  pipeline_destroy(&entry->value);

  HASH_DEL(s_program_table, entry);
  mx_free(mx_default_allocator(), entry);
}

mgfx_imgh mgfx_image_create(const mgfx_image_info *info, uint32_t usage) {
  image_entry *entry = mx_alloc(mx_default_allocator(), sizeof(image_entry));
  memset(entry, 0, sizeof(image_entry));

  usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  image_create(info, usage, 0, &entry->value);

  entry->key = entry->value.handle;
  HASH_ADD(hh, s_image_table, key, sizeof(VkImage), entry);

  return (mgfx_imgh){.idx = (uint64_t)entry->key};
}

void mgfx_image_destroy(mgfx_imgh imgh) {
  image_entry *entry;
  HASH_FIND(hh, s_image_table, &imgh, sizeof(mgfx_imgh), entry);

  if (!entry) {
    MX_LOG_WARN("Attempting to destroy invalid image handle!");
    return;
  }

  image_destroy(&entry->value);

  HASH_DEL(s_image_table, entry);
  mx_free(mx_default_allocator(), entry);
}

mgfx_th mgfx_texture_create_from_memory(const mgfx_image_info *info,
                                        uint32_t filter, const void *data,
                                        size_t len) {
  texture_entry *entry =
      mx_alloc(mx_default_allocator(), sizeof(texture_entry));
  memset(entry, 0, sizeof(texture_entry));

  entry->value.imgh = mgfx_image_create(info, VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                  VK_IMAGE_USAGE_SAMPLED_BIT);

  image_entry *image_entry;
  HASH_FIND(hh, s_image_table, &entry->value.imgh, sizeof(mgfx_imgh),
            image_entry);

  image_update(data, len, &image_entry->value);

  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .magFilter = filter,
      .minFilter = filter,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 1.0f,
      .borderColor = 0,
      .unnormalizedCoordinates = VK_FALSE,
  };

  VK_CHECK(vkCreateSampler(s_device, &sampler_info, NULL,
                           (VkSampler *)&entry->value.sampler));

  image_create_view(&image_entry->value, VK_IMAGE_VIEW_TYPE_2D,
                    VK_IMAGE_ASPECT_COLOR_BIT, (VkImageView *)(&entry->key));
  HASH_ADD(hh, s_texture_table, key, sizeof(uint64_t), entry);

  return entry->key;
};

mgfx_th mgfx_texture_create_from_image(mgfx_imgh img, const uint32_t filter) {
  texture_entry *entry =
      mx_alloc(mx_default_allocator(), sizeof(texture_entry));
  memset(entry, 0, sizeof(texture_entry));

  entry->value.imgh = img;

  image_entry *image_entry;
  HASH_FIND(hh, s_image_table, &entry->value.imgh, sizeof(mgfx_imgh),
            image_entry);

  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .magFilter = filter,
      .minFilter = filter,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,

      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

      // Shadow
      //.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      //.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      //.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      //.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,*/

      .mipLodBias = 0,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 1.0f,

      .borderColor = 0,
      .unnormalizedCoordinates = 0,
  };

  // TODO: Make aspect argument
  if (image_entry->value.format == VK_FORMAT_D32_SFLOAT) {
    image_create_view(&image_entry->value, VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_DEPTH_BIT, (VkImageView *)(&entry->key));
    HASH_ADD(hh, s_texture_table, key, sizeof(uint64_t), entry);
  } else {
    image_create_view(&image_entry->value, VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_COLOR_BIT, (VkImageView *)(&entry->key));
    HASH_ADD(hh, s_texture_table, key, sizeof(uint64_t), entry);
  }

  VK_CHECK(vkCreateSampler(s_device, &sampler_info, NULL,
                           (VkSampler *)&entry->value.sampler));

  return entry->key;
}

mgfx_th mgfx_texture_create_from_path(const char *path, mgfx_format format) {
  int width, height, channel_count;

  stbi_uc *data =
      stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);

  MX_ASSERT(data, "Texture at %s failed to load", path);
  MX_ASSERT(data != NULL, "[MGFX_EXAMPLES]: Failed to load texture!");

  size_t size = width * height * 4;
  MX_LOG_TRACE("Texture loaded: %s (%.2f mb)!", path, (float)size / MX_MB);

  mgfx_image_info info = {
      .format = format,

      .width = width,
      .height = height,
      .layers = 1,

      .cube_map = MX_FALSE,
  };

  mgfx_th th =
      mgfx_texture_create_from_memory(&info, VK_FILTER_LINEAR, data, size);

  stbi_image_free(data);
  return th;
}

void mgfx_texture_destroy(mgfx_th th, mx_bool release_image) {
  VK_CHECK(vkDeviceWaitIdle(s_device));

  texture_entry *entry;
  HASH_FIND(hh, s_texture_table, &th, sizeof(th), entry);
  MX_ASSERT(entry != NULL, "Texture invalid handle!");

  // TODO: Move to texture_destroy
  vkDestroyImageView(s_device, (VkImageView)entry->key.idx, NULL);
  vkDestroySampler(s_device, (VkSampler)entry->value.sampler, NULL);

  HASH_DEL(s_texture_table, entry);
  if (release_image) {
    mgfx_image_destroy(entry->value.imgh);
  }

  mx_free(mx_default_allocator(), entry);
}

mgfx_dh mgfx_descriptor_create(const char *name, uint32_t type) {
  descriptor_entry *entry =
      mx_alloc(mx_default_allocator(), sizeof(descriptor_entry));
  memset(entry, 0, sizeof(descriptor_entry));

  entry->key = (mgfx_dh){.idx = (uint64_t)(entry)};
  entry->value = (descriptor_info_vk){.type = type};

  MX_ASSERT(strlen(name) <= sizeof(entry->value.name));
  strcpy(entry->value.name, name);

  HASH_ADD(hh, s_descriptor_table, key, sizeof(uint64_t), entry);

  return entry->key;
}

void mgfx_set_buffer(mgfx_dh dh, mgfx_ubh ubh) {
  descriptor_entry *entry;
  HASH_FIND(hh, s_descriptor_table, &dh, sizeof(dh), entry);
  MX_ASSERT(entry != NULL, "Descriptor invalid handle!");

  buffer_entry *buffer_entry;
  HASH_FIND(hh, s_buffer_table, &ubh, sizeof(ubh), buffer_entry);
  MX_ASSERT(buffer_entry != NULL, "buffer invalid handle!");

  entry->value.buffer_info.buffer = (VkBuffer)ubh.idx;
  entry->value.buffer_info.offset = 0;
  entry->value.buffer_info.range = VK_WHOLE_SIZE;

  entry->value.buffer = &buffer_entry->value;
}

void mgfx_set_texture(mgfx_dh dh, mgfx_th th) {
  descriptor_entry *entry;
  HASH_FIND(hh, s_descriptor_table, &dh, sizeof(dh), entry);
  MX_ASSERT(entry != NULL, "Descriptor invalid handle!");

  texture_entry *texture_entry;
  HASH_FIND(hh, s_texture_table, &th, sizeof(th), texture_entry);
  MX_ASSERT(texture_entry != NULL, "Texture invalid handle!");

  entry->value.image_info.imageView = (VkImageView)th.idx;
  entry->value.image_info.sampler = (VkSampler)texture_entry->value.sampler;
  entry->value.image_info.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Expected layout

  image_entry *image_entry;
  HASH_FIND(hh, s_image_table, &texture_entry->value.imgh, sizeof(mgfx_imgh),
            image_entry);
  MX_ASSERT(image_entry != NULL, "Image invalid handle!");
  entry->value.image = &image_entry->value;
}

mgfx_dh mgfx_descriptor_create_and_set_uniform_buffer(const char *name,
                                                      mgfx_ubh ubh) {
  mgfx_dh out = mgfx_descriptor_create(name, MGFX_UNIFORM_TYPE_UNIFORM_BUFFER);
  mgfx_set_buffer(out, ubh);
  return out;
}

mgfx_dh mgfx_descriptor_create_and_set_storage_buffer(const char *name,
                                                      mgfx_sbh sbh) {
  mgfx_dh out = mgfx_descriptor_create(name, MGFX_UNIFORM_TYPE_STORAGE_BUFFER);
  mgfx_set_buffer(out, (mgfx_ubh){.idx = sbh.idx});
  return out;
}

mgfx_dh mgfx_descriptor_create_and_set_texture(const char *name, mgfx_th th) {
  mgfx_dh out =
      mgfx_descriptor_create(name, MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER);
  mgfx_set_texture(out, th);
  return out;
}

void mgfx_descriptor_destroy(mgfx_dh dh) {}

mgfx_fbh mgfx_framebuffer_create(mgfx_imgh *color_attachments,
                                 uint32_t color_attachment_count,
                                 mgfx_imgh depth_attachment) {
  // TODO: Framebuffer should be an api agonstic concept.
  framebuffer_entry *entry =
      mx_alloc(mx_default_allocator(), sizeof(framebuffer_entry));
  memset(entry, 0, sizeof(framebuffer_entry));

  framebuffer_vk *fb = &entry->value;
  fb->color_attachment_count = color_attachment_count;
  for (uint32_t i = 0; i < color_attachment_count; i++) {
    image_entry *color_entry;
    HASH_FIND(hh, s_image_table, &color_attachments[i], sizeof(mgfx_imgh),
              color_entry);

    fb->color_attachments[i] = &color_entry->value;
    image_create_view(fb->color_attachments[i], VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_COLOR_BIT,
                      &fb->color_attachment_views[i]);
  }

  image_entry *depth_entry;
  HASH_FIND(hh, s_image_table, &depth_attachment, sizeof(mgfx_imgh),
            depth_entry);
  if (depth_entry) {
    fb->depth_attachment = &depth_entry->value;
    image_create_view(fb->depth_attachment, VK_IMAGE_VIEW_TYPE_2D,
                      VK_IMAGE_ASPECT_DEPTH_BIT, &fb->depth_attachment_view);
  }

  entry->key.idx = (uint64_t)fb;
  HASH_ADD(hh, s_framebuffer_table, key, sizeof(mgfx_fbh), entry);

  return entry->key;
}

void mgfx_framebuffer_destroy(mgfx_fbh fbh) {
  framebuffer_entry *framebuffer_entry;
  HASH_FIND(hh, s_framebuffer_table, &fbh, sizeof(mgfx_fbh), framebuffer_entry);

  if (!framebuffer_entry) {
    MX_LOG_WARN("Attempting to destroy invalid framebuffer handle!");
    return;
  }

  framebuffer_destroy(&framebuffer_entry->value);

  HASH_DEL(s_framebuffer_table, framebuffer_entry);
  mx_free(mx_default_allocator(), framebuffer_entry);
}

void mgfx_bind_vertex_buffer(mgfx_vbh vbh) {
  mgfx_draw *current_draw = &s_draws[s_draw_count];
  current_draw->vbh = vbh;
}

void mgfx_bind_transient_vertex_buffer(mgfx_transient_vb tvb) {
  mgfx_draw *current_draw = &s_draws[s_draw_count];
  current_draw->tvb = tvb;
}

void mgfx_bind_index_buffer(mgfx_ibh ibh) {
  mgfx_draw *current_draw = &s_draws[s_draw_count];
  current_draw->ibh = ibh;
}

void mgfx_bind_transient_index_buffer(mgfx_transient_buffer tib) {
  mgfx_draw *current_draw = &s_draws[s_draw_count];
  current_draw->tib = tib;
}

void mgfx_bind_descriptor(mgfx_dh dh) {
  uint32_t ds_idx = 0;
  MX_ASSERT(ds_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET);

  mgfx_draw *current_draw = &s_draws[s_draw_count];

  // Descriptor slot 0 is reserved for built in
  uint32_t descriptor_idx = current_draw->desc_sets[ds_idx].dh_count;
  MX_ASSERT(descriptor_idx < MGFX_SHADER_MAX_DESCRIPTOR_BINDING);

  current_draw->desc_sets[ds_idx].dhs[descriptor_idx] = dh;
  ++current_draw->desc_sets[ds_idx].dh_count;
}

void mgfx_set_view_clear(uint8_t target, float *color_4) {
  if (target < 0xFF) {
    memcpy(s_view_clears[target].float32, color_4, sizeof(float) * 4);
    return;
  }

  MX_LOG_ERROR("Attemping to set clear color for unknown target. Please call "
               "mgfx_set_view_target first");
}

void mgfx_set_view_target(uint8_t target, mgfx_fbh fb) {
  s_view_targets[target] = fb;
}

void mgfx_set_transform(const float *mtx) {
  memcpy(current_transform.transform.val, mtx, sizeof(float) * 16);
  current_transform.inverse_transform =
      mx_mat4_inverse(current_transform.transform);
}

void mgfx_set_view(const float *mtx) {
  memcpy(current_transform.view.val, mtx, sizeof(float) * 16);
  current_transform.inverse_view = mx_mat4_inverse(current_transform.view);
}

void mgfx_set_proj(const float *mtx) {
  memcpy(current_transform.proj.val, mtx, sizeof(float) * 16);
}

void mgfx_submit(uint8_t target, mgfx_ph ph) {
  MX_ASSERT(s_draw_count < MGFX_MAX_DRAW_COUNT,
            "Reached max draws! Consider instancing or batching!");

  // Bind mgfx built in descriptors.
  mgfx_bind_descriptor(u_transforms);

  mgfx_draw *cdraw = &s_draws[s_draw_count];

  program_entry *program_entry;
  HASH_FIND(hh, s_program_table, &ph, sizeof(ph), program_entry);
  MX_ASSERT(program_entry != NULL, "Program invalid handle!");

#ifdef MX_DEBUG
  mx_bool valid_mtx = MX_FALSE;
  for (int i = 0; i < 16; i++) {
    if (current_transform.transform.val[i] != 0) {
      valid_mtx = MX_TRUE;
    }
  }

  if (!valid_mtx) {
    MX_LOG_WARN("Draw call to %s transform not set!",
                program_entry->value.name);
  }
#endif
  transforms[s_draw_count] = current_transform;

  if ((VkPipeline)program_entry->value.pipeline == VK_NULL_HANDLE) {
    mgfx_program *program = &program_entry->value;

    shader_entry *vs_entry;
    HASH_FIND(hh, s_shader_table, &program->shaders[MGFX_SHADER_STAGE_VERTEX],
              sizeof(VkShaderModule), vs_entry);
    const shader_vk *vs = vs_entry ? &vs_entry->value : NULL;

    shader_entry *fs_entry;
    HASH_FIND(hh, s_shader_table, &program->shaders[MGFX_SHADER_STAGE_FRAGMENT],
              sizeof(VkShaderModule), fs_entry);
    const shader_vk *fs = fs_entry ? &fs_entry->value : NULL;

    const mgfx_vertex_layout *vl = NULL;

    if (cdraw->vbh.idx) {
      vb_info *vertex_info =
          vb_info_find(&vb_infos, (mgfx_vbh){.idx = cdraw->vbh.idx});

      if (vertex_info) {
        vl = &vertex_info->vl;
      }
    }

    if (cdraw->tvb.tb.buffer_handle) {
      if (vl) {
        MX_LOG_WARN("Draw call has both vertex and transient vertex buffer!");
        return;
      }

      vl = cdraw->tvb.vl;
    }

    if (target == MGFX_DEFAULT_VIEW_TARGET) {
      pipeline_create_graphics(vs, fs, &s_swapchain.framebuffer, vl, program);
    } else {
      framebuffer_entry *fb_entry;
      HASH_FIND(hh, s_framebuffer_table, &s_view_targets[target],
                sizeof(mgfx_fbh), fb_entry);

      if (fb_entry) {
        pipeline_create_graphics(vs, fs, &fb_entry->value, vl, program);
      } else {
        MX_LOG_ERROR("Submitting to unknown view target '%d'! Please call "
                     "mgfx_set_view_target().",
                     target);
      }
    }
  }

  cdraw->view_target = target;
  cdraw->ph = ph;

  // TODO: Hash each to uint16_t
  cdraw->sort_key = ((uint64_t)(target) << 56) | // highest priority (view)
                    ((uint64_t)(ph.idx & 0xFFFF) << 40); // then shader program
  /*| ((uint64_t)(descriptors_hash) << 8);*/

  ++s_draw_count;
}

int mgfx_frame() {
  static uint64_t s_frame_ctr = 0;

  // Get current frame.
  frame_vk *frame = &s_frames[s_frame_idx];

  // Wait and reset render fence of current frame idx.
  VK_CHECK(
      vkWaitForFences(s_device, 1, &frame->render_fence, VK_TRUE, UINT64_MAX));

  // Update bulilt in descriptors
  mgfx_buffer_update(transform_storage_buffer.idx, transforms,
                     sizeof(transform_data) * s_draw_count, 0);

  if (!swapchain_update(frame, s_width, s_height, &s_swapchain)) {
    return MGFX_WARN_SWAPCHAIN_RESIZE;
  }

  VK_CHECK(vkResetFences(s_device, 1, &frame->render_fence));

  vkResetCommandBuffer(frame->cmd, 0);
  VkCommandBufferBeginInfo cmd_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL,
  };

  VK_CHECK(vkBeginCommandBuffer(frame->cmd, &cmd_begin_info));

  // Buffer to buffer copy queue
  if (s_buffer_to_buffer_copy_count > 0) {
    VkMemoryBarrier vb_cpy_barriers[MAX_FRAME_BUFFER_COPIES];
    uint32_t vb_cpy_barrier_count = 0;

    for (uint32_t copy_idx = 0; copy_idx < s_buffer_to_buffer_copy_count;
         copy_idx++) {
      const buffer_vk *src = s_buffer_to_buffer_copy_queue[copy_idx].src;
      buffer_vk *dst = s_buffer_to_buffer_copy_queue[copy_idx].dst;
      VkBufferCopy *cpy = &s_buffer_to_buffer_copy_queue[copy_idx].copy;

      vkCmdCopyBuffer(frame->cmd, src->handle, dst->handle, 1, cpy);

      if ((dst->usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ==
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
        vb_cpy_barriers[vb_cpy_barrier_count++] = (VkMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT};
      } else if ((dst->usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ==
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
        vb_cpy_barriers[vb_cpy_barrier_count++] =
            (VkMemoryBarrier){.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                              .pNext = NULL,
                              .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                              .dstAccessMask = VK_ACCESS_INDEX_READ_BIT};
      };
    };

    // Vertex buffer memory barriers
    vkCmdPipelineBarrier(frame->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0,
                         vb_cpy_barrier_count, vb_cpy_barriers, 0, NULL, 0,
                         NULL);

    s_buffer_to_buffer_copy_count = 0;
  }

  // Buffer to image copy queue
  if (s_buffer_to_image_copy_count > 0) {
    VkImageMemoryBarrier sampled_barriers[MAX_FRAME_BUFFER_COPIES];
    uint32_t sampled_copy_count = 0;

    VkImageMemoryBarrier pre_copy_barriers[MAX_FRAME_BUFFER_COPIES];
    uint32_t pre_copy_count = 0;

    for (uint32_t i = 0; i < s_buffer_to_image_copy_count; i++) {
      pre_copy_barriers[pre_copy_count++] = (VkImageMemoryBarrier){
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .pNext = NULL,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
          .oldLayout = s_buffer_to_image_copy_queue[i].dst->layout,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = s_buffer_to_image_copy_queue[i].dst->handle,
          .subresourceRange =
              {
                  .aspectMask = s_buffer_to_image_copy_queue[i]
                                    .copy.imageSubresource.aspectMask,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = s_buffer_to_image_copy_queue[i]
                                        .copy.imageSubresource.baseArrayLayer,
                  .layerCount = s_buffer_to_image_copy_queue[i]
                                    .copy.imageSubresource.layerCount,
              },
      };
      s_buffer_to_image_copy_queue[i].dst->layout =
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

      // TODO: Check if sampled.
      sampled_barriers[sampled_copy_count++] = (VkImageMemoryBarrier){
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .pNext = NULL,
          .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout = s_buffer_to_image_copy_queue[i].dst->layout,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = s_buffer_to_image_copy_queue[i].dst->handle,
          .subresourceRange =
              {
                  .aspectMask = s_buffer_to_image_copy_queue[i]
                                    .copy.imageSubresource.aspectMask,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = s_buffer_to_image_copy_queue[i]
                                        .copy.imageSubresource.baseArrayLayer,
                  .layerCount = s_buffer_to_image_copy_queue[i]
                                    .copy.imageSubresource.layerCount,
              },
      };

      s_buffer_to_image_copy_queue[i].dst->layout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Fragment Sampled image memory barriers
    vkCmdPipelineBarrier(frame->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                         pre_copy_count, pre_copy_barriers);

    for (uint32_t i = 0; i < s_buffer_to_image_copy_count; i++) {
      vkCmdCopyBufferToImage(
          frame->cmd, (VkBuffer)s_buffer_to_image_copy_queue[i].src->handle,
          (VkImage)s_buffer_to_image_copy_queue[i].dst->handle,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
          &s_buffer_to_image_copy_queue[i].copy);
    }

    // Fragment Sampled image memory barriers
    vkCmdPipelineBarrier(frame->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, sampled_copy_count, sampled_barriers);

    s_buffer_to_image_copy_count = 0;
  }

  // Clear transient staging buffers
  for (uint32_t tsb_idx = 0; tsb_idx < s_transient_staging_buffer_count;
       tsb_idx++) {
    s_transient_staging_buffer_pool.tail =
        (s_transient_staging_buffer_pool.tail + s_tsbs[tsb_idx].actual_size) %
        s_transient_staging_buffer_pool.size;
  }
  s_transient_staging_buffer_count = 0;

  vk_cmd_transition_image(frame->cmd, &s_swapchain.images[s_swapchain.free_idx],
                          VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

  VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  // Sort draws by view target, program, descriptor sets
  qsort(s_draws, (size_t)s_draw_count, sizeof(mgfx_draw), draw_compare_fn);

  framebuffer_vk *fb = NULL;
  uint8_t target = MGFX_DEFAULT_VIEW_TARGET - 1;

  VkDescriptorSet flat_ds[MGFX_SHADER_MAX_DESCRIPTOR_SET];
  uint32_t flat_ds_count = 0;

  mgfx_program *last_program = NULL;

  VkBuffer last_vb = VK_NULL_HANDLE;
  VkDeviceSize last_offset = 0;

  VkBuffer last_ib = VK_NULL_HANDLE;
  uint32_t last_idx_count = 0;

  for (uint32_t draw_idx = 0; draw_idx < s_draw_count; draw_idx++) {
    const mgfx_draw *draw = &s_draws[draw_idx];
    program_entry *program_entry;
    HASH_FIND(hh, s_program_table, &draw->ph, sizeof(mgfx_ph), program_entry);

    MX_ASSERT(program_entry != NULL);

    if (draw->view_target != target) {
      target = draw->view_target;

      if (fb != NULL) {
        vk_cmd_end_rendering(frame->cmd);
      }

      // Target pre pass resource barriers and transitions
      for (uint32_t target_draw_idx = draw_idx; target_draw_idx < s_draw_count;
           target_draw_idx++) {
        const mgfx_draw *target_draw = &s_draws[target_draw_idx];

        if (target_draw->view_target != target) {
          break;
        }

        for (uint32_t ds_idx = 0; ds_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET;
             ds_idx++) {
          if (target_draw->desc_sets[ds_idx].dh_count <= 0) {
            continue;
          }

          for (uint32_t bind_idx = 0;
               bind_idx < target_draw->desc_sets[ds_idx].dh_count; bind_idx++) {
            mgfx_dh dh = target_draw->desc_sets[ds_idx].dhs[bind_idx];
            const descriptor_entry *descriptor_entry;

            HASH_FIND(hh, s_descriptor_table, &dh, sizeof(mgfx_dh),
                      descriptor_entry);

            if (!descriptor_entry) {
              MX_LOG_WARN("Shader Program '%s' binding (%u) invalid handle!",
                          program_entry->value.name, bind_idx);
              MX_ASSERT(0);
            }

            switch (descriptor_entry->value.type) {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
              if (descriptor_entry->value.image->layout !=
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                switch (descriptor_entry->value.image->format) {
                case (VK_FORMAT_D32_SFLOAT):
                  vk_cmd_transition_image(
                      frame->cmd, descriptor_entry->value.image,
                      VK_IMAGE_ASPECT_DEPTH_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                  break;
                default:
                  vk_cmd_transition_image(
                      frame->cmd, descriptor_entry->value.image,
                      VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                  break;
                }
              }
              break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
              break;

            case VK_DESCRIPTOR_TYPE_MAX_ENUM:
            default:
              MX_ASSERT(0, "Uknown descriptor set");
              break;
            }
          }
        }
      }

      if (target == MGFX_DEFAULT_VIEW_TARGET) {
        fb = &s_swapchain.framebuffer;
      } else {
        framebuffer_entry *fb_entry;
        HASH_FIND(hh, s_framebuffer_table, &s_view_targets[target],
                  sizeof(mgfx_fbh), fb_entry);

        if (!fb_entry) {
          MX_LOG_ERROR("[MGFX] Invalid frame buffer bound at view target %u",
                       target);
          continue;
        }

        fb = &fb_entry->value;
      }

      for (uint32_t color_attachment_idx = 0;
           color_attachment_idx < fb->color_attachment_count;
           color_attachment_idx++) {
        vk_cmd_transition_image(
            frame->cmd, fb->color_attachments[color_attachment_idx],
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

        if (s_view_clears[target].uint32[3]) {
          vk_cmd_clear_image(frame->cmd,
                             fb->color_attachments[color_attachment_idx],
                             &range, &s_view_clears[target]);
        }
      }

      if (fb->depth_attachment) {
        vk_cmd_transition_image(frame->cmd, fb->depth_attachment,
                                VK_IMAGE_ASPECT_DEPTH_BIT,
                                VK_IMAGE_LAYOUT_GENERAL);
      }

      vk_cmd_begin_rendering(frame->cmd, fb);
    }

    // Check program in view target.
    if (last_program != &program_entry->value) {
      last_program = &program_entry->value;
      vkCmdBindPipeline(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        (VkPipeline)last_program->pipeline);
    }

    flat_ds_count = 0;
    for (uint32_t ds_idx = 0; ds_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET;
         ds_idx++) {
      if (draw->desc_sets[ds_idx].dh_count <= 0) {
        continue;
      }
      uint32_t ds_hash;
      mx_murmur_hash_32(draw->desc_sets[ds_idx].dhs,
                        draw->desc_sets[ds_idx].dh_count * sizeof(mgfx_dh),
                        (uint32_t)draw->ph.idx, &ds_hash);

      descriptor_set_entry *ds_entry;
      HASH_FIND_INT(s_descriptor_set_table, &ds_hash, ds_entry);

      if (ds_entry) {
        flat_ds[flat_ds_count++] = ds_entry->value;
        continue;
      }

      VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .pNext = NULL,
          .descriptorPool = s_ds_pool,
          .descriptorSetCount = 1,
          .pSetLayouts = (VkDescriptorSetLayout *)&last_program->dsls[ds_idx],
      };

      ds_entry = mx_alloc(MX_DEFAULT_ALLOCATOR, sizeof(descriptor_set_entry));
      memset(ds_entry, 0, sizeof(descriptor_set_entry));

      ds_entry->key = ds_hash;
      HASH_ADD_INT(s_descriptor_set_table, key, ds_entry);

      VK_CHECK(vkAllocateDescriptorSets(s_device, &descriptor_set_alloc_info,
                                        &ds_entry->value));

      flat_ds[flat_ds_count++] = ds_entry->value;

      for (uint32_t binding_idx = 0;
           binding_idx < draw->desc_sets[ds_idx].dh_count; binding_idx++) {

        mgfx_dh dh = draw->desc_sets[ds_idx].dhs[binding_idx];
        const descriptor_entry *descriptor_entry;

        HASH_FIND(hh, s_descriptor_table, &dh, sizeof(mgfx_dh),
                  descriptor_entry);
        MX_ASSERT(descriptor_entry != NULL, "Descriptor invalid handle!");

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = ds_entry->value,
            .dstBinding = binding_idx,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = descriptor_entry->value.type,
            .pImageInfo = NULL,
            .pBufferInfo = NULL,
            .pTexelBufferView = NULL,
        };

        switch (descriptor_entry->value.type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          write.pImageInfo = &descriptor_entry->value.image_info;
          MX_ASSERT(descriptor_entry->value.image->layout ==
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
          break;

        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
          write.pBufferInfo = &descriptor_entry->value.buffer_info;
          break;

        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
        case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
        case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
        case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
        case VK_DESCRIPTOR_TYPE_MAX_ENUM:
        default:
          MX_LOG_ERROR("Unsupported descriptor type!");
          break;
        };

        vkUpdateDescriptorSets(s_device, 1, &write, 0, NULL);
      }
    }

    struct {
      uint32_t id;
      uint32_t _padding[3];
    } pc;
    pc.id = draw_idx;

    vkCmdPushConstants(frame->cmd,
                       (VkPipelineLayout)last_program->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    if (flat_ds_count > 0) {
      vkCmdBindDescriptorSets(frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              (VkPipelineLayout)last_program->pipeline_layout,
                              0, flat_ds_count, flat_ds, 0, NULL);
    }

    VkBuffer vb = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;

    if ((VkBuffer)draw->vbh.idx) {
      vb = (VkBuffer)draw->vbh.idx;
      offset = 0;
    }

    if ((VkBuffer)draw->tvb.tb.buffer_handle) {
      vb = (VkBuffer)draw->tvb.tb.buffer_handle;
      offset = draw->tvb.tb.offset;
      transient_vertex_buffer_free(&draw->tvb.tb);
    }

    if (vb != last_vb || last_offset != offset) {
      last_vb = vb;
      last_offset = offset;

      vkCmdBindVertexBuffers(frame->cmd, 0, 1, &vb, &offset);
    }

    if ((VkBuffer)draw->ibh.idx != VK_NULL_HANDLE &&
        (VkBuffer)draw->ibh.idx != last_ib) {
      VkDeviceSize offset = 0;

      const buffer_entry *index_buffer_entry;
      HASH_FIND(hh, s_buffer_table, &draw->ibh, sizeof(mgfx_ibh),
                index_buffer_entry);

      if (index_buffer_entry) {
        VmaAllocationInfo index_buffer_alloc_info = {0};
        vmaGetAllocationInfo(s_allocator, index_buffer_entry->value.allocation,
                             &index_buffer_alloc_info);
        last_idx_count =
            (uint32_t)index_buffer_alloc_info.size / sizeof(uint32_t);
      } else {
        MX_LOG_WARN("Index buffer invalid handle!");
      }

      last_ib = (VkBuffer)draw->ibh.idx;
      vkCmdBindIndexBuffer(frame->cmd, last_ib, 0, VK_INDEX_TYPE_UINT32);
    }

    if ((VkBuffer)draw->tib.buffer_handle != VK_NULL_HANDLE) {
      if ((VkBuffer)draw->tib.buffer_handle != last_ib) {
        last_ib = (VkBuffer)draw->tib.buffer_handle;
      };

      vkCmdBindIndexBuffer(frame->cmd, last_ib, draw->tib.offset,
                           VK_INDEX_TYPE_UINT32);
      last_idx_count = draw->tib.requested_size / sizeof(uint32_t);

      transient_index_buffer_free(&draw->tib);
    }

    if (last_ib) {
      vkCmdDrawIndexed(frame->cmd, last_idx_count, 1, 0, 0, 0);
    } else {
      MX_ASSERT(MX_FALSE, "Unsupported draw method!");
      vkCmdDraw(frame->cmd, last_idx_count, 1, 0, 0);
    }
  }

  // Clear draws
  memset(s_draws, 0, s_draw_count * sizeof(mgfx_draw));
  s_draw_count = 0;

  if (fb != NULL) {
    vk_cmd_end_rendering(frame->cmd);
  }

  vk_cmd_transition_image(frame->cmd, &s_swapchain.images[s_swapchain.free_idx],
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(frame->cmd));

  VkPipelineStageFlags wait_stages =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame->swapchain_semaphore,
      .pWaitDstStageMask = &wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &frame->cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &frame->render_semaphore,
  };
  VK_CHECK(vkQueueSubmit(s_queues[VK_QUEUE_GRAPHICS], 1, &submit_info,
                         frame->render_fence));

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = NULL,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame->render_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &s_swapchain.handle,
      .pImageIndices = &s_swapchain.free_idx,
      .pResults = NULL,
  };

  VkResult present_result =
      vkQueuePresentKHR(s_queues[VK_QUEUE_GRAPHICS], &present_info);
  switch (present_result) {
  case (VK_SUBOPTIMAL_KHR):
    break;
  case (VK_ERROR_OUT_OF_DATE_KHR): {
    s_swapchain.resize = MX_TRUE;
    return MGFX_WARN_SWAPCHAIN_RESIZE;
  } break;

  default:
    VK_CHECK(present_result);
    break;
  }

  ++s_frame_ctr;
  s_frame_idx = (s_frame_idx + 1) % MGFX_FRAME_COUNT;

  return MX_SUCCESS;
};

// Debug tools
#include <stdarg.h>
#include <stdio.h>
void mgfx_debug_draw_text(int32_t x, int32_t y, const char *fmt, ...) {
  // TODO: Add all to single buffer or commands at end of regular draws.
  char word_buffer[MGFX_DEBUG_MAX_TEXT];
  memset(word_buffer, 0, sizeof(word_buffer));

  va_list args;
  va_start(args, fmt);
  vsnprintf(word_buffer, sizeof(word_buffer), fmt, args);
  va_end(args);

  typedef struct glyph_vertex {
    float position[3];
    float uv_x;
    float uv_y;
  } glyph_vertex;

  glyph_vertex vertices[MGFX_DEBUG_MAX_TEXT * 4];
  uint32_t vertex_count = 0;

  uint32_t indices[MGFX_DEBUG_MAX_TEXT * 6];
  uint32_t index_count = 0;

  float cursor_x = 0;
  float cursor_y = 0;

  for (size_t char_idx = 0; char_idx < strlen(word_buffer); char_idx++) {
    // Get the character's metrics: position, size, and offsets
    stbtt_packedchar *c = &chars[word_buffer[char_idx] - 32];

    // Calculate the vertices for the current character
    float x0 = cursor_x + c->xoff;   // Start x position + offset
    float y0 = cursor_y - c->yoff;   // Start y position + offset
    float x1 = x0 + (c->x1 - c->x0); // End x position + offset
    float y1 = y0 - (c->y1 - c->y0); // End y position + offset

    // Uvs flipped form openGL NDCs
    float uv_y0 = ((float)c->y0 / (float)atlas_h);
    float uv_y1 = ((float)c->y1 / (float)atlas_h);

    // Bottom left
    uint32_t vertex_offset = (uint32_t)char_idx * 4;
    vertices[vertex_offset + 0] = (glyph_vertex){
        .position = {x0, y0, 0.0f}, // Position in 2D space
        .uv_x = (float)c->x0 / (float)atlas_w,
        .uv_y = uv_y0,
    };

    // Bottom right
    vertices[vertex_offset + 1] = (glyph_vertex){
        .position = {x1, y0, 0.0f}, // Position in 2D space
        .uv_x = (float)c->x1 / (float)atlas_w,
        .uv_y = uv_y0,
    };

    // Top right
    vertices[vertex_offset + 2] = (glyph_vertex){
        .position = {x1, y1, 0.0f}, // Position in 2D space
        .uv_x = (float)c->x1 / (float)atlas_w,
        .uv_y = uv_y1,
    };

    // Top left
    vertices[vertex_offset + 3] = (glyph_vertex){
        .position = {x0, y1, 0.0f}, // Position in 2D space
        .uv_x = (float)c->x0 / (float)atlas_w,
        .uv_y = uv_y1,
    };

    vertex_count += 4;

    // Counter clickwise vertices
    uint32_t char_indices[6] = {
        0 + vertex_offset, 3 + vertex_offset, 2 + vertex_offset,
        2 + vertex_offset, 1 + vertex_offset, 0 + vertex_offset,
    };

    memcpy(&indices[char_idx * 6], char_indices, sizeof(uint32_t) * 6);
    index_count += 6;

    cursor_x += c->xadvance;
  }

  // Typical ortho
  // TODOL: Get from application
  s_width = 1280;
  s_height = 720;
  const real_t right = (real_t)s_width;
  const real_t top = (real_t)s_height;
  mgfx_set_proj(mx_ortho(0.0f, right, 0.0, top, -100.0f, 100.0f).val);

  mx_vec3 ui_cam_pos = {0, 0, 2};
  mx_vec3 ui_inverse_pos = mx_vec3_scale(ui_cam_pos, -1.0f);

  mx_mat4 view = mx_translate(ui_inverse_pos);
  mgfx_set_view(view.val);

  mx_mat4 model = mx_translate((mx_vec3){(float)x, (float)y, -1.0});
  mgfx_set_transform(model.val);

  mgfx_vertex_layout glyph_vl;
  mgfx_vertex_layout_begin(&glyph_vl);
  mgfx_vertex_layout_add(&glyph_vl, MGFX_VERTEX_ATTRIBUTE_POSITION,
                         sizeof(mx_vec3));
  mgfx_vertex_layout_add(&glyph_vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD0,
                         sizeof(mx_vec2));
  mgfx_vertex_layout_end(&glyph_vl);
  mgfx_transient_vb tvb = {.vl = &glyph_vl};
  mgfx_transient_vertex_buffer_allocate(
      vertices, vertex_count * sizeof(glyph_vertex), &tvb);

  mgfx_transient_buffer tib = {0};
  mgfx_transient_index_buffer_allocate(indices, index_count * sizeof(uint32_t),
                                       &tib);

  mgfx_bind_transient_vertex_buffer(tvb);
  mgfx_bind_transient_index_buffer(tib);
  mgfx_bind_descriptor(dbg_ui_font_atlas_dh);

  mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, dbg_ui_ph);
}

void mgfx_shutdown() {
  // Destroy built in
  mgfx_texture_destroy(MGFX_WHITE_TEXTURE, MX_TRUE);
  mgfx_texture_destroy(MGFX_BLACK_TEXTURE, MX_TRUE);
  mgfx_texture_destroy(MGFX_LOCAL_NORMAL_TEXTURE, MX_TRUE);

  mgfx_buffer_destroy(transform_storage_buffer.idx);
  mgfx_descriptor_destroy(u_transforms);

  // Shutdown Debug UI
  mgfx_buffer_destroy(dbg_quad_vbh.idx);
  mgfx_buffer_destroy(dbg_quad_ibh.idx);

  mgfx_texture_destroy(dbg_ui_font_atlas_th, MX_TRUE);
  mgfx_descriptor_destroy(dbg_ui_font_atlas_dh);

  mgfx_shader_destroy(dbg_ui_vsh);
  mgfx_shader_destroy(dbg_ui_fsh);
  mgfx_program_destroy(dbg_ui_ph);

  // Destroy vulkan renderer
  VK_CHECK(vkDeviceWaitIdle(s_device));

  buffer_destroy(&s_transient_staging_buffer_pool.buffer);
  buffer_destroy(&s_transient_vertex_buffer_pool.buffer);
  buffer_destroy(&s_transient_index_buffer_pool.buffer);

  vkDestroyDescriptorPool(s_device, s_ds_pool, NULL);

  shader_entry *current_entry, *tmp;
  HASH_ITER(hh, s_shader_table, current_entry, tmp) {
    HASH_DEL(s_shader_table, current_entry); // Remove from hashmap
    free(current_entry);                     // Free allocated memory
  }

  for (int i = 0; i < MGFX_FRAME_COUNT; i++) {
    vkDestroyCommandPool(s_device, s_frames[i].cmd_pool, NULL);

    vkDestroySemaphore(s_device, s_frames[i].render_semaphore, NULL);
    vkDestroyFence(s_device, s_frames[i].render_fence, NULL);

    vkDestroySemaphore(s_device, s_frames[i].swapchain_semaphore, NULL);
  }

  vmaDestroyAllocator(s_allocator);
  swapchain_destroy(&s_swapchain);

  vkDestroySurfaceKHR(s_instance, s_surface, NULL);
  vkDestroyDevice(s_device, NULL);

#ifdef MX_DEBUG
  destroy_create_debug_util_messenger_ext(s_instance, s_debug_messenger, NULL);
#endif

  vkDestroyInstance(s_instance, NULL);
}

void mgfx_reset(uint32_t width, uint32_t height) {
  MX_LOG_TRACE("Window resized to (%d, %d)", width, height);
  s_width = width;
  s_height = height;
}
