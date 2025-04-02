#include "mgfx/mgfx.h"
#include "mgfx/defines.h"

#include "renderer_vk.h"

#include <mx/mx_log.h>
#include <mx/mx_math.h>

#include <mx/mx.h>
#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_hash.h>
#include <mx/mx_memory.h>

#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef MX_MACOS
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_metal.h>
#elif defined(MX_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

#include <spirv_reflect/spirv_reflect.h>
#include <vma/vk_mem_alloc.h>

typedef struct mgfx_texture {
    mgfx_imgh imgh;        // VkImage
    uint64_t address_mode; // VkSamplerAddressMode
    uint64_t sampler;      // VkSampler
} mgfx_texture;

typedef struct mgfx_program {
    mgfx_sh shaders[MGFX_SHADER_STAGE_COUNT];
    uint64_t dsls[MGFX_SHADER_MAX_DESCRIPTOR_SET]; // VkPipelineLayout

    uint64_t pipeline;        // VkPipeline
    uint64_t pipeline_layout; // VkPipelineLayout
} mgfx_program;

typedef enum queues_vk : uint16_t {
    MGFX_QUEUE_GRAPHICS = 0,
    MGFX_QUEUE_PRESENT,

    MGFX_QUEUE_COUNT,
} queues_vk;

// Extension function pointers.
PFN_vkCmdBeginRenderingKHR vk_cmd_begin_rendering_khr;
PFN_vkCmdEndRenderingKHR vk_cmd_end_rendering_khr;

const char* k_req_exts[] = {VK_KHR_SURFACE_EXTENSION_NAME,
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
const int k_req_ext_count = sizeof(k_req_exts) / sizeof(const char*);

const char* k_req_device_ext_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef MX_MACOS
                                        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
};
const uint32_t k_req_device_ext_count = (uint32_t)(sizeof(k_req_device_ext_names) / sizeof(const char*));

const VkFormat k_surface_fmt = VK_FORMAT_B8G8R8A8_SRGB;
const VkColorSpaceKHR k_surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
const VkPresentModeKHR k_present_mode = VK_PRESENT_MODE_FIFO_KHR;

const VkDescriptorPoolSize k_ds_pool_sizes[] = {
    {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 128},
    {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 32},

    {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 128},
};

const uint32_t k_ds_pool_sizes_count = sizeof(k_ds_pool_sizes) / sizeof(VkDescriptorPoolSize);

#ifdef MX_DEBUG
VkResult create_debug_util_messenger_ext(VkInstance instance,
                                         const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                         const VkAllocationCallbacks* pAllocator,
                                         VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void destroy_create_debug_util_messenger_ext(VkInstance instance,
                                             VkDebugUtilsMessengerEXT debugMessenger,
                                             const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT msg_severity,
                  VkDebugUtilsMessageTypeFlagsEXT msg_type,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void* pUserData) {

    static const char* msg_type_strings[0x9] = {
        [0x00000001] = "VK_GENERAL",
        [0x00000002] = "VK_VALIDATION",
        [0x00000004] = "VK_PERFORMANCE",
        [0x00000008] = "VK_DEVICE_ADDRESS_BINDING",
    };

    switch (msg_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        MX_LOG_DEBUG("[%s]: %s", msg_type_strings[msg_type], pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        MX_LOG_DEBUG("[%s]: %s", msg_type_strings[msg_type], pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        MX_LOG_WARN("%s", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        MX_LOG_ERROR("[%s]: %s", msg_type_strings[msg_type], pCallbackData->pMessage);
        break;
    default:
        break;
    };

    MX_ASSERT(0);
    return VK_FALSE;
}

static VkDebugUtilsMessengerEXT s_debug_messenger;
#endif

static VkInstance s_instance = VK_NULL_HANDLE;

static VkPhysicalDevice s_phys_device = VK_NULL_HANDLE;
static VkPhysicalDeviceProperties s_phys_device_props;

static VkDevice s_device = VK_NULL_HANDLE;
static VmaAllocator s_allocator;

static VkQueue s_queues[MGFX_QUEUE_COUNT];
static uint32_t s_queue_indices[MGFX_QUEUE_COUNT];

static VkSurfaceKHR s_surface = VK_NULL_HANDLE;
static VkSurfaceCapabilitiesKHR s_surface_caps;
static swapchain_vk s_swapchain;

static frame_vk s_frames[MGFX_FRAME_COUNT];
static uint32_t s_frame_idx = 0;

static VkDescriptorPool s_ds_pool;

enum { MGFX_MAX_FRAME_BUFFER_COPIES = 256 };
static buffer_to_buffer_copy_vk s_buffer_to_buffer_copy_queue[MGFX_MAX_FRAME_BUFFER_COPIES];
static uint32_t s_buffer_to_buffer_copy_count = 0;

static buffer_to_image_copy_vk s_buffer_to_image_copy_queue[MGFX_MAX_FRAME_BUFFER_COPIES];
static uint32_t s_buffer_to_image_copy_count = 0;

static buffer_pool_vk s_vb_staging_pool;
static buffer_pool_vk s_ib_staging_pool;

static buffer_vk s_image_staging_buffer;
static size_t s_image_staging_buffer_offset;

// TODO: Change param to image create info
void image_create(const mgfx_image_info* info,
                  VkImageUsageFlags usage,
                  VkImageCreateFlags flags,
                  image_vk* image) {
    image->format = info->format;
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
        .pQueueFamilyIndices = &s_queue_indices[MGFX_QUEUE_GRAPHICS],
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VK_CHECK(vmaCreateImage(s_allocator, &image_info, &alloc_info, &image->handle, &image->allocation, NULL));
}

void image_create_view(const image_vk* image,
                       VkImageViewType type,
                       VkImageAspectFlags aspect,
                       VkImageView* view) {
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

void image_update(const void* data, size_t size, image_vk* image) {
    void* bound_memory;
    vmaMapMemory(s_allocator, s_image_staging_buffer.allocation, &bound_memory);
    memcpy(bound_memory + s_image_staging_buffer_offset, data, size);
    vmaUnmapMemory(s_allocator, s_image_staging_buffer.allocation);

    s_buffer_to_image_copy_queue[s_buffer_to_image_copy_count++] = (buffer_to_image_copy_vk){
        .copy =
            {
                .bufferOffset = s_image_staging_buffer_offset,
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
        .src = &s_image_staging_buffer,
        .dst = image,
    };

    s_image_staging_buffer_offset += size;
};

void image_destroy(image_vk* image) {
    vmaDestroyImage(s_allocator, image->handle, image->allocation);

    image->handle = VK_NULL_HANDLE;
    image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

int swapchain_create(
    VkSurfaceKHR surface, uint32_t width, uint32_t height, swapchain_vk* swapchain, mx_arena* memory_arena) {
    mx_arena* local_arena = memory_arena;

    mx_arena temp_arena;
    if (memory_arena == NULL) {
        temp_arena = mx_arena_alloc(MX_KB);
        local_arena = &temp_arena;
    }

    swapchain->extent.width = width;
    swapchain->extent.height = height;

    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.pNext = NULL;
    swapchain_info.flags = 0;
    swapchain_info.surface = surface;

    int image_count = s_surface_caps.minImageCount + 1;
    if (s_surface_caps.maxImageCount > 0 && image_count > s_surface_caps.maxImageCount) {
        image_count = s_surface_caps.maxImageCount;
    }
    swapchain_info.minImageCount = image_count;

    swapchain_info.imageFormat = k_surface_fmt;
    swapchain_info.imageColorSpace = k_surface_color_space;

    swapchain_info.imageExtent.width = swapchain->extent.width;
    swapchain_info.imageExtent.height = swapchain->extent.height;

    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (s_queue_indices[MGFX_QUEUE_GRAPHICS] != s_queue_indices[MGFX_QUEUE_PRESENT]) {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = &s_queue_indices[MGFX_QUEUE_PRESENT];
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

    VK_CHECK(vkCreateSwapchainKHR(s_device, &swapchain_info, NULL, &swapchain->handle));

    vkGetSwapchainImagesKHR(s_device, swapchain->handle, &swapchain->image_count, NULL);

    MX_ASSERT(swapchain->image_count < K_SWAPCHAIN_MAX_IMAGES,
              "Requested swapchain image count (%d) larger than max capacity! %d",
              swapchain->image_count,
              K_SWAPCHAIN_MAX_IMAGES);

    VkImage* images = mx_arena_push(local_arena, swapchain->image_count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(s_device, swapchain->handle, &swapchain->image_count, images);

    VkImageViewCreateInfo sc_img_view_info = {};
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

    for (uint32_t i = 0; i < swapchain->image_count; i++) {
        swapchain->images[i] = (image_vk){
            .handle = images[i],
            .format = k_surface_fmt,
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .extent = (VkExtent3D){width, height, 1.0f},
        };

        sc_img_view_info.image = swapchain->images[i].handle;
        vkCreateImageView(s_device, &sc_img_view_info, NULL, &swapchain->image_views[i]);
    }

    swapchain->framebuffer = (framebuffer_vk){
        .color_attachments = &s_swapchain.images[0],
        .color_attachment_views = s_swapchain.image_views[0],
        .color_attachment_count = 1,

        .depth_attachment = NULL,
        .depth_attachment_view = NULL,
    };

    if (memory_arena == NULL) {
        mx_arena_free(local_arena);
    }

    return MGFX_SUCCESS;
}

void swapchain_destroy(swapchain_vk* swapchain) {
    for (uint32_t i = 0; i < swapchain->image_count; i++) {
        vkDestroyImageView(s_device, swapchain->image_views[i], NULL);
    }

    vkDestroySwapchainKHR(s_device, swapchain->handle, NULL);
}

void buffer_create(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags, buffer_vk* buffer) {
    buffer->usage = usage;

    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = size,
        .usage = buffer->usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo alloc_info = {
        .flags = flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VK_CHECK(vmaCreateBuffer(s_allocator, &info, &alloc_info, &buffer->handle, &buffer->allocation, NULL));

    {
        VmaAllocationInfo alloc_info = {};
        vmaGetAllocationInfo(s_allocator, buffer->allocation, &alloc_info);
    }
};

void buffer_update(buffer_vk* buffer, size_t buffer_offset, size_t size, const void* data) {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(s_allocator, buffer->allocation, &alloc_info);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(s_phys_device, &mem_props);
    uint32_t memory_type_idx = alloc_info.memoryType;

    VkMemoryPropertyFlags memory_flags = mem_props.memoryTypes[memory_type_idx].propertyFlags;

    if ((memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
        void* staging_data;
        vmaMapMemory(s_allocator, buffer->allocation, &staging_data);
        memcpy(staging_data + buffer_offset, data, size);
        vmaUnmapMemory(s_allocator, buffer->allocation);
        return;
    }

    MX_LOG_ERROR("COPY COMMAND!");
}

void buffer_destroy(buffer_vk* buffer) {
    vmaDestroyBuffer(s_allocator, buffer->handle, buffer->allocation);

    buffer->handle = VK_NULL_HANDLE;
    buffer->allocation = VK_NULL_HANDLE;
}

void vertex_buffer_create(size_t size, const void* data, vertex_buffer_vk* buffer) {
    buffer_create(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0, buffer);

    // TODO: Recover
    if (s_buffer_to_buffer_copy_count >= MGFX_MAX_FRAME_BUFFER_COPIES) {
        MX_LOG_ERROR("Exceeded buffer copies this frame");
        MX_ASSERT(0);
    };

    buffer_slice_vk slice = {0};
    buffer_slice_allocate(&s_vb_staging_pool, size, &slice);
    buffer_slice_update(&slice, size, data);

    s_buffer_to_buffer_copy_queue[s_buffer_to_buffer_copy_count++] = (buffer_to_buffer_copy_vk){
        .copy =
            {
                .srcOffset = slice.offset,
                .dstOffset = 0,
                .size = size,
            },
        .src = &slice.buffer_pool->buffer,
        .dst = buffer,
    };
};

void index_buffer_create(size_t size, const void* data, index_buffer_vk* buffer) {
    buffer_create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0, buffer);

    buffer_slice_vk slice;
    buffer_slice_allocate(&s_ib_staging_pool, size, &slice);
    buffer_slice_update(&slice, size, data);

    s_buffer_to_buffer_copy_queue[s_buffer_to_buffer_copy_count++] = (buffer_to_buffer_copy_vk){
        .copy =
            {
                .srcOffset = slice.offset,
                .dstOffset = 0,
                .size = size,
            },
        .src = &slice.buffer_pool->buffer,
        .dst = buffer,
    };
};

void uniform_buffer_create(size_t size, const void* data, uniform_buffer_vk* buffer) {
    buffer_create(size,
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                  buffer);

    void* mapped_data;
    vmaMapMemory(s_allocator, buffer->allocation, &mapped_data);
    memcpy(mapped_data, data, size);
    vmaUnmapMemory(s_allocator, buffer->allocation);
};

void buffer_pool_create(size_t size,
                        VkBufferUsageFlags usage,
                        VmaAllocationCreateFlags allocation_flags,
                        buffer_pool_vk* pool) {
    buffer_create(size, usage, allocation_flags, &pool->buffer);
    pool->head = 0;
}

void buffer_slice_allocate(buffer_pool_vk* pool, size_t size, buffer_slice_vk* slice) {
    // TODO: Buffer info
    VmaAllocationInfo alloc_info = {};
    vmaGetAllocationInfo(s_allocator, pool->buffer.allocation, &alloc_info);

    size_t free_size = alloc_info.size - pool->head;
    if (free_size < size) {
        MX_LOG_WARN("[buffer_slice] allocation failed. Reallocating...");

        // TODO: copy and reallocate.
        return;
    }

    slice->buffer_pool = pool;
    slice->offset = pool->head;
    slice->size = size;

    pool->head += size;
};

void buffer_slice_update(buffer_slice_vk* slice, size_t size, const void* data) {
    buffer_update(&slice->buffer_pool->buffer, slice->offset, size, data);
}

void buffer_slice_deallocate(buffer_pool_vk* pool, buffer_slice_vk* slice) {}

void buffer_pool_destroy(buffer_pool_vk* pool) {
    buffer_destroy(&pool->buffer);
    pool->head = UINT32_MAX;
}

void framebuffer_create(uint32_t color_attachment_count,
                        image_vk* color_attachments,
                        image_vk* depth_attachment,
                        framebuffer_vk* framebuffer) {
    framebuffer->color_attachment_count = color_attachment_count;
    for (int i = 0; i < color_attachment_count; i++) {
        framebuffer->color_attachments[i] = &color_attachments[i];
        image_create_view(framebuffer->color_attachments[i],
                          VK_IMAGE_VIEW_TYPE_2D,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          &framebuffer->color_attachment_views[i]);
    }

    if (depth_attachment) {
        framebuffer->depth_attachment = depth_attachment;
        image_create_view(framebuffer->depth_attachment,
                          VK_IMAGE_VIEW_TYPE_2D,
                          VK_IMAGE_ASPECT_DEPTH_BIT,
                          &framebuffer->depth_attachment_view);
    }
}

void framebuffer_destroy(framebuffer_vk* fb) {
    // TODO: Should be api agonstic level
    for (int i = 0; i < fb->color_attachment_count; i++) {
        fb->color_attachments[i] = NULL;
        vkDestroyImageView(s_device, fb->color_attachment_views[i], NULL);
    }

    if (fb->depth_attachment) {
        fb->depth_attachment = NULL;
        vkDestroyImageView(s_device, fb->depth_attachment_view, NULL);
    }
}

void shader_create(size_t length, const char* code, shader_vk* shader) {
    mx_arena shader_arena = mx_arena_alloc(10 * MX_KB);

    if (length % sizeof(uint32_t) != 0) {
        MX_LOG_ERROR("Shader code size must be a multiple of 4!");
        return;
    }

    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(length, code, &module);
    MX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    if (module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) {
        uint32_t input_count = 0;
        spvReflectEnumerateInputVariables(&module, &input_count, NULL);
        if (input_count > 0) {
            SpvReflectInterfaceVariable** input_variables =
                mx_arena_push(&shader_arena, input_count * sizeof(SpvReflectInterfaceVariable));
            spvReflectEnumerateInputVariables(&module, &input_count, input_variables);

            // Sort by location and filter out built in variables.
            const SpvReflectInterfaceVariable* sorted_iv[input_count] = {};
            uint32_t sorted_iv_count = 0;
            for (uint32_t i = 0; i < input_count; i++) {
                const SpvReflectInterfaceVariable* input_variable = input_variables[i];

                if (input_variable->built_in != -1) {
                    continue;
                }

                sorted_iv[input_variable->location] = input_variable;
                ++sorted_iv_count;
            }
            shader->vertex_attribute_count = sorted_iv_count;

            uint32_t input_offset = 0;
            for (uint32_t i = 0; i < sorted_iv_count; i++) {
                const SpvReflectInterfaceVariable* input_variable = sorted_iv[i];
                shader->vertex_attributes[i] = (VkVertexInputAttributeDescription){
                    .location = input_variable->location,
                    .binding = 0,
                    .format = (VkFormat)input_variable->format,
                    .offset = input_offset,
                };

                input_offset += vk_format_size((VkFormat)input_variable->format);
                MX_LOG_TRACE("input variable: %s\n", input_variable->name);
                MX_LOG_TRACE("\tlocation: %u\n", input_variable->location);
                MX_LOG_TRACE("\toffset: %u\n", input_variable->word_offset.location);
                MX_LOG_TRACE("\tformat: %u (%d bytes)\n",
                             input_variable->format,
                             vk_format_size((VkFormat)input_variable->format));
            }

            if (shader->vertex_attribute_count > 0) {
                // TODO: Support multiple vertex buffer bindings.
                shader->vertex_binding_count = 1;
                shader->vertex_bindings[0] = (VkVertexInputBindingDescription){
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
        SpvReflectDescriptorBinding** bindings =
            mx_arena_push(&shader_arena, binding_count * sizeof(SpvReflectDescriptorBinding*));
        spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings);

        for (uint32_t i = 0; i < binding_count; i++) {
            descriptor_set_info_vk* ds = &shader->ds_infos[bindings[i]->set];

            ds->bindings[bindings[i]->binding] = (VkDescriptorSetLayoutBinding){
                .binding = bindings[i]->binding,
                .descriptorType = (VkDescriptorType)bindings[i]->descriptor_type,
                .descriptorCount = 1,
                .stageFlags = module.shader_stage,
                .pImmutableSamplers = NULL,
            };

            ds->binding_count++;
        }
    }

    uint32_t pc_count = 0;
    result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, NULL);
    MX_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    if (pc_count > 0) {
        SpvReflectBlockVariable** push_constants =
            mx_arena_push(&shader_arena, pc_count * sizeof(SpvReflectBlockVariable));
        spvReflectEnumeratePushConstantBlocks(&module, &pc_count, push_constants);
        for (uint32_t block_idx = 0; block_idx < pc_count; block_idx++) {
            const SpvReflectBlockVariable* pc_block = push_constants[block_idx];

            for (int member_idx = 0; member_idx < push_constants[block_idx]->member_count; member_idx++) {
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

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = length;
    info.pCode = (uint32_t*)(void*)code;

    VK_CHECK(vkCreateShaderModule(s_device, &info, NULL, &shader->module));

    mx_arena_free(&shader_arena);
};

void shader_destroy(shader_vk* shader) { vkDestroyShaderModule(s_device, shader->module, NULL); }

void pipeline_create_graphics(const shader_vk* vs,
                              const shader_vk* fs,
                              const framebuffer_vk* fb,
                              mgfx_program* program) {
    MX_ASSERT(vs != NULL, "Graphics program requires at least a valid vertex shader!");

    const MGFX_SHADER_STAGE gfx_stages[] = {MGFX_SHADER_STAGE_VERTEX, MGFX_SHADER_STAGE_FRAGMENT};
    uint32_t shader_stage_count = fs != NULL ? 2 : 1;

    VkGraphicsPipelineCreateInfo info = {};
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

    VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = vs->vertex_binding_count,
        .pVertexBindingDescriptions = vs->vertex_bindings,
        .vertexAttributeDescriptionCount = vs->vertex_attribute_count,
        .pVertexAttributeDescriptions = vs->vertex_attributes,
    };
    info.pVertexInputState = &vertex_input_state_info;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info = {};
    input_assembly_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_info.pNext = NULL;
    input_assembly_state_info.flags = 0;
    input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
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
        .rasterizationSamples = 1,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };
    info.pMultisampleState = &multisample_state_info;

    if (fb->depth_attachment) {
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };
        info.pDepthStencilState = &depth_stencil_state_info;
    } else {
        info.pDepthStencilState = NULL;
    }

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };
    info.pColorBlendState = &color_blend_state_info;

    const VkDynamicState k_dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const uint32_t k_dynamic_state_count = sizeof(k_dynamic_states) / sizeof(VkDynamicState);

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = k_dynamic_state_count,
        .pDynamicStates = k_dynamic_states,
    };
    info.pDynamicState = &dynamic_state_info;

    int ds_count = 0;

    VkDescriptorSetLayoutBinding flat_bindings[MGFX_SHADER_MAX_DESCRIPTOR_SET * MGFX_SHADER_MAX_DESCRIPTOR_BINDING];
    memset(flat_bindings, 0, sizeof(VkDescriptorSetLayoutBinding) * MGFX_SHADER_MAX_DESCRIPTOR_SET * MGFX_SHADER_MAX_DESCRIPTOR_BINDING);

    uint32_t max_bindings[MGFX_SHADER_MAX_DESCRIPTOR_SET * MGFX_SHADER_MAX_DESCRIPTOR_BINDING];
    memset(max_bindings, 0, sizeof(uint32_t) * MGFX_SHADER_MAX_DESCRIPTOR_SET * MGFX_SHADER_MAX_DESCRIPTOR_BINDING);

    VkDescriptorSetLayout ds_layouts[MGFX_SHADER_MAX_DESCRIPTOR_SET];
    memset(ds_layouts, 0, sizeof(VkDescriptorSetLayout) * MGFX_SHADER_MAX_DESCRIPTOR_SET);

    VkPushConstantRange flat_pc_ranges[MGFX_SHADER_MAX_PUSH_CONSTANTS];
    memset(flat_pc_ranges, 0, sizeof(VkPushConstantRange) * MGFX_SHADER_MAX_PUSH_CONSTANTS);
    int flat_pc_range_count = 0;

    // Resolve multi stage descriptor bindings
    for (int stage_idx = 0; stage_idx < 2; stage_idx++) {
        const shader_vk* shader = stage_idx == 0 ? vs : fs;

        if (shader == NULL) {
            continue;
        }

        ds_count = shader->ds_count > ds_count ? shader->ds_count : ds_count;

        for (int ds_idx = 0; ds_idx < shader->ds_count; ds_idx++) {
            const descriptor_set_info_vk* ds = &shader->ds_infos[ds_idx];

            if (ds->binding_count <= 0) {
                continue;
            };

            for (uint32_t binding = 0; binding < ds->binding_count; binding++) {
                max_bindings[ds_idx] = binding > max_bindings[ds_idx] ? binding : max_bindings[ds_idx];

                VkDescriptorSetLayoutBinding* ds_bindings = &flat_bindings[ds_idx * MGFX_SHADER_MAX_DESCRIPTOR_BINDING + binding];

                if (ds_bindings->stageFlags != 0) {
                    ds_bindings->stageFlags |= stage_idx == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
                } else {
                    *ds_bindings = ds->bindings[binding];
                }
            }
        }

        for (int pc_index = 0; pc_index < shader->pc_count; pc_index++) {
            flat_pc_ranges[flat_pc_range_count] = shader->pc_ranges[flat_pc_range_count];
            ++flat_pc_range_count;
        }
    }

    for (int ds_idx = 0; ds_idx < ds_count; ds_idx++) {
        VkDescriptorSetLayoutCreateInfo dsl_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = max_bindings[ds_idx] + 1,
            .pBindings = &flat_bindings[ds_idx * MGFX_SHADER_MAX_DESCRIPTOR_BINDING],
        };

        VK_CHECK(vkCreateDescriptorSetLayout(s_device, &dsl_info, NULL, (VkDescriptorSetLayout*)&program->dsls[ds_idx]));
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

    VK_CHECK(vkCreatePipelineLayout(s_device, &pipeline_layout_info, NULL, (VkPipelineLayout*)&program->pipeline_layout));
    info.layout = (VkPipelineLayout)program->pipeline_layout;

    // Dynamic Rendering.
    info.renderPass = NULL;

    VkFormat color_attachment_formats[fb->color_attachment_count] = {};
    for (int i = 0; i < fb->color_attachment_count; i++) {
        color_attachment_formats[i] = fb->color_attachments[i]->format;
    }

    VkPipelineRenderingCreateInfoKHR rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .pNext = NULL,
        .viewMask = 0,
        .colorAttachmentCount = fb->color_attachment_count,
        .pColorAttachmentFormats = color_attachment_formats,
        .depthAttachmentFormat = fb->depth_attachment ? fb->depth_attachment->format : VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
    info.pNext = &rendering_create_info;

    VK_CHECK(vkCreateGraphicsPipelines(s_device, NULL, 1, &info, NULL, (VkPipeline*)&program->pipeline));
}

void pipeline_destroy(mgfx_program* program) {
    for (uint32_t descriptor_idx = 0; descriptor_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET; descriptor_idx++) {
        if ((VkDescriptorSetLayout)program->dsls[descriptor_idx] == VK_NULL_HANDLE) {
            continue;
        }

        vkDestroyDescriptorSetLayout(s_device, (VkDescriptorSetLayout)program->dsls[descriptor_idx], NULL);
    }

    vkDestroyPipelineLayout(s_device, (VkPipelineLayout)program->pipeline_layout, NULL);
    vkDestroyPipeline(s_device, (VkPipeline)program->pipeline, NULL);
}

// TODO: Remove
void descriptor_buffer_create(const buffer_vk* buffer, descriptor_info_vk* descriptor) {
    *descriptor =
        (descriptor_info_vk){.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             .buffer_info = {.buffer = buffer->handle, .offset = 0, .range = VK_WHOLE_SIZE}};
}

// TODO: Remove
void descriptor_image_create(const VkImageView* image_view, descriptor_info_vk* descriptor) {
    *descriptor = (descriptor_info_vk){.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       .image_info = {
                                           .sampler = NULL,
                                           .imageView = *image_view,
                                           .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                                       }};
}

mx_bool swapchain_update(const frame_vk* frame, int width, int height, swapchain_vk* sc) {
    if (sc->resize == MX_TRUE) {
        vkDeviceWaitIdle(s_device);

        swapchain_destroy(&s_swapchain);

        if (s_surface_caps.currentExtent.width == UINT32_MAX) {
            width = clampf(width, s_surface_caps.minImageExtent.width, s_surface_caps.maxImageExtent.width);
            height =
                clampf(height, s_surface_caps.minImageExtent.height, s_surface_caps.maxImageExtent.height);
        }

        swapchain_create(s_surface, width, height, &s_swapchain, NULL);
        sc->resize = MX_FALSE;
    }

    // Wait for available next swapchain image.
    VkResult acquire_sc_image_result = vkAcquireNextImageKHR(
        s_device, s_swapchain.handle, UINT64_MAX, frame->swapchain_semaphore, VK_NULL_HANDLE, &sc->free_idx);

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

typedef struct buffer_entry {
    VkBuffer key;
    buffer_vk value;
    UT_hash_handle hh;
} buffer_entry;
static buffer_entry* s_buffer_table;

typedef struct image_entry {
    VkImage key;
    image_vk value;
    UT_hash_handle hh;
} image_entry;
static image_entry* s_image_table;

typedef struct shader_entry {
    VkShaderModule key;
    shader_vk value;
    UT_hash_handle hh;
} shader_entry;
static shader_entry* s_shader_table;

typedef struct texture_entry {
    mgfx_th key;
    mgfx_texture value;
    UT_hash_handle hh;
} texture_entry;
static texture_entry* s_texture_table;

typedef struct program_entry {
    mgfx_ph key;
    mgfx_program value;
    UT_hash_handle hh;
} program_entry;
static program_entry* s_program_table;

typedef struct descriptor_entry {
    mgfx_dh key;
    descriptor_info_vk value;
    UT_hash_handle hh;
} descriptor_entry;
static descriptor_entry* s_descriptor_table;

typedef struct descriptor_set_entry {
    uint32_t key;
    VkDescriptorSet value;
    UT_hash_handle hh;
} descriptor_set_entry;
static descriptor_set_entry* s_descriptor_set_table;

typedef struct framebuffer_entry {
    mgfx_fbh key;
    framebuffer_vk value;
    UT_hash_handle hh;
} framebuffer_entry;
static framebuffer_entry* s_framebuffer_table;

typedef struct mgfx_draw {
    struct descriptor_sets {
        mgfx_dh dhs[MGFX_SHADER_MAX_DESCRIPTOR_BINDING];
        uint32_t dh_count;
    } descriptor_sets[MGFX_SHADER_MAX_DESCRIPTOR_SET];

    struct {
        float model[16];
        float view[16];
        float proj[16];
        float view_inv[16];
    } draw_pc;

    mgfx_vbh vbhs[4];
    uint32_t vbh_count;

    mgfx_ibh ibh;
    uint32_t index_count;

    mgfx_ph ph;
    uint8_t view_target;

    uint64_t sort_key;
} mgfx_draw;

static int draw_compare_fn(const void* a, const void* b) {
    const mgfx_draw* draw_a = (mgfx_draw*)a;
    const mgfx_draw* draw_b = (mgfx_draw*)b;

    return draw_a->sort_key > draw_b->sort_key;
}

mgfx_fbh s_view_targets[0xFF];

uint32_t s_width;
uint32_t s_height;

static float s_current_transform[16];
static float s_current_view[16];
static float s_current_proj[16];

static mgfx_draw s_draws[256];
static uint32_t s_draw_count = 0;

int mgfx_init(const mgfx_init_info* info) {
    mx_arena vk_init_arena = mx_arena_alloc(MX_MB);

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = info->name;
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "MGFX";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);

    uint32_t api_version;
    vkEnumerateInstanceVersion(&api_version);
    MX_LOG_INFO("Instance ApiVersion: %d.%d.%d", VK_VERSION_MAJOR(api_version),
                VK_VERSION_MINOR(api_version), VK_VERSION_PATCH(api_version));
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.pApplicationInfo = &app_info;
#ifdef MX_MACOS
    instance_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // Check if required extensions are available.
    uint32_t avail_prop_count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count, NULL));
    VkExtensionProperties* avail_props =
        mx_arena_push(&vk_init_arena, avail_prop_count * sizeof(VkExtensionProperties));
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count, avail_props));

    for (uint32_t req_index = 0; req_index < k_req_ext_count; req_index++) {
        int validated = -1;
        for (uint32_t avail_index = 0; avail_index < avail_prop_count; avail_index++) {
            if (strcmp(k_req_exts[req_index], avail_props[avail_index].extensionName) == 0) {
                validated = MX_SUCCESS;
                continue;
            }
        }

        if (validated != MX_SUCCESS) {
            MX_LOG_ERROR("Extension required not supported: %s!\n", k_req_exts[req_index]);
            mx_arena_free(&vk_init_arena);
            return -1;
        }
    }

    instance_info.enabledExtensionCount = k_req_ext_count;
    instance_info.ppEnabledExtensionNames = k_req_exts;

#ifdef MX_DEBUG
    // Check and enable validation layers.
    uint32_t layer_prop_count;
    vkEnumerateInstanceLayerProperties(&layer_prop_count, NULL);
    VkLayerProperties* layer_props =
        mx_arena_push(&vk_init_arena, sizeof(VkLayerProperties) * layer_prop_count);
    vkEnumerateInstanceLayerProperties(&layer_prop_count, layer_props);

    const char* validationLayers = {"VK_LAYER_KHRONOS_validation"};

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
        (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(s_instance, "vkCmdBeginRenderingKHR");
    vk_cmd_end_rendering_khr =
        (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(s_instance, "vkCmdEndRenderingKHR");

#ifdef MX_DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
    debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = vk_debug_callback;
    debug_messenger_info.pUserData = NULL;

    create_debug_util_messenger_ext(s_instance, &debug_messenger_info, NULL, &s_debug_messenger);
#endif
    if (choose_physical_device_vk(s_instance,
                                  k_req_device_ext_count,
                                  k_req_device_ext_names,
                                  &s_phys_device_props,
                                  &s_phys_device,
                                  &vk_init_arena) != VK_SUCCESS) {
        MX_LOG_ERROR("Failed to find suitable physical device!");
        mx_arena_free(&vk_init_arena);
        return -1;
    }
    MX_LOG_INFO("Device : %s (%d)", s_phys_device_props.deviceName, s_phys_device_props.deviceType);
    MX_LOG_INFO("Vulkan ApiVersion : %d.%d.%d",
                VK_API_VERSION_MAJOR(s_phys_device_props.apiVersion),
                VK_API_VERSION_MINOR(s_phys_device_props.apiVersion),
                VK_API_VERSION_PATCH(s_phys_device_props.apiVersion));

    VK_CHECK(get_window_surface_vk(s_instance, info->nwh, &s_surface));

    uint32_t queue_family_props_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device, &queue_family_props_count, NULL);
    VkQueueFamilyProperties* queue_family_props =
        mx_arena_push(&vk_init_arena, sizeof(VkQueueFamilyProperties) * queue_family_props_count);

    vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device, &queue_family_props_count, queue_family_props);

    int unique_queue_count = 0;
    for (uint32_t i = 0; i < queue_family_props_count; i++) {
        if ((queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) {
            s_queue_indices[MGFX_QUEUE_GRAPHICS] = i;
        }

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(s_phys_device, i, s_surface, &present_supported);
        if (present_supported == VK_TRUE) {
            s_queue_indices[MGFX_QUEUE_PRESENT] = i;
        }

        ++unique_queue_count;

        // Check if complete.
        int complete = MGFX_SUCCESS;
        for (uint16_t j = 0; j < unique_queue_count; j++) {
            if (s_queue_indices[MGFX_QUEUE_PRESENT] == -1) {
                complete = !MGFX_SUCCESS;
            }
        }

        if (complete == MGFX_SUCCESS) {
            break;
        }
    }

    for (uint16_t i = 0; i < MGFX_QUEUE_COUNT; i++) {
        if (s_queue_indices[MGFX_QUEUE_PRESENT] == -1) {
            MX_LOG_ERROR("Failed to find graphics queue!");
            mx_arena_free(&vk_init_arena);
            break;
        }
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_infos[MGFX_QUEUE_COUNT] = {};
    for (uint16_t i = 0; i < unique_queue_count; i++) {
        queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[i].pNext = NULL;
        queue_infos[i].flags = 0;
        queue_infos[i].queueFamilyIndex = s_queue_indices[i];
        queue_infos[i].queueCount = 1;
        queue_infos[i].pQueuePriorities = &queue_priority;
    }

    VkPhysicalDeviceFeatures phys_device_features = {};
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

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys_device, s_surface, &s_surface_caps));

    uint32_t surface_fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys_device, s_surface, &surface_fmt_count, NULL);
    VkSurfaceFormatKHR* surface_fmts =
        mx_arena_push(&vk_init_arena, surface_fmt_count * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys_device, s_surface, &surface_fmt_count, surface_fmts);

    int surface_format_found = -1;
    for (uint32_t i = 0; i < surface_fmt_count; i++) {
        if (surface_fmts[i].format == k_surface_fmt && surface_fmts[i].colorSpace == k_surface_color_space) {
            surface_format_found = MGFX_SUCCESS;
            break;
        }
    }

    MX_ASSERT(surface_format_found == MGFX_SUCCESS, "Required surface format not found!");

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys_device, s_surface, &present_mode_count, NULL);
    VkPresentModeKHR* present_modes =
        mx_arena_push(&vk_init_arena, surface_fmt_count * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys_device, s_surface, &present_mode_count, present_modes);
    for (uint32_t i = 0; i < present_mode_count; i++) {
    }

    VkExtent2D swapchain_extent;
    choose_swapchain_extent_vk(&s_surface_caps, info->nwh, &swapchain_extent);

    swapchain_create(
        s_surface, swapchain_extent.width, swapchain_extent.height, &s_swapchain, &vk_init_arena);

    VmaAllocatorCreateInfo allocator_info = {
        .instance = s_instance, .physicalDevice = s_phys_device, .device = s_device,
        /*.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,*/
    };
    VK_CHECK(vmaCreateAllocator(&allocator_info, &s_allocator));

    for (int i = 0; i < MGFX_FRAME_COUNT; i++) {
        VkCommandPoolCreateInfo gfx_cmd_pool = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = s_queue_indices[MGFX_QUEUE_GRAPHICS],
        };
        VK_CHECK(vkCreateCommandPool(s_device, &gfx_cmd_pool, NULL, &s_frames[i].cmd_pool));

        VkCommandBufferAllocateInfo buffer_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = s_frames[i].cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(s_device, &buffer_alloc_info, &s_frames[i].cmd));

        VkSemaphoreCreateInfo swapchain_semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };
        VK_CHECK(
            vkCreateSemaphore(s_device, &swapchain_semaphore_info, NULL, &s_frames[i].swapchain_semaphore));

        VkSemaphoreCreateInfo render_semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };
        VK_CHECK(vkCreateSemaphore(s_device, &render_semaphore_info, NULL, &s_frames[i].render_semaphore));

        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VK_CHECK(vkCreateFence(s_device, &fence_info, NULL, &s_frames[i].render_fence));
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
    enum { MGFX_MAX_IMAGE_SIZE = MX_MB * 500 };
    buffer_create(MGFX_MAX_IMAGE_SIZE,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                  &s_image_staging_buffer);

    enum { MGFX_MAX_STAGING_BUFFER_SIZE = MX_MB * 20 };
    buffer_pool_create(MGFX_MAX_STAGING_BUFFER_SIZE,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                       &s_vb_staging_pool);

    buffer_pool_create(MGFX_MAX_STAGING_BUFFER_SIZE,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                       &s_ib_staging_pool);

    mx_arena_free(&vk_init_arena);

    memset(s_view_targets, 0, sizeof(mgfx_fbh) * 0xFF);

    MX_LOG_SUCCESS("MGFX Initialized!");
    return MGFX_SUCCESS;
}

mgfx_vbh mgfx_vertex_buffer_create(const void* data, size_t len) {
    buffer_entry* entry = mx_alloc(sizeof(buffer_entry), 0);
    vertex_buffer_create(len, data, &entry->value);

    entry->key = entry->value.handle;
    HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_vbh), entry);

    return (mgfx_vbh){.idx = (uint64_t)entry->key};
}

mgfx_ibh mgfx_index_buffer_create(const void* data, size_t len) {
    buffer_entry* entry = mx_alloc(sizeof(buffer_entry), 0);
    index_buffer_create(len, data, &entry->value);

    entry->key = entry->value.handle;
    HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_ibh), entry);

    return (mgfx_ibh){.idx = (uint64_t)entry->key};
}

mgfx_ubh mgfx_uniform_buffer_create(const void* data, size_t len) {
    buffer_entry* entry = mx_alloc(sizeof(buffer_entry), 0);
    uniform_buffer_create(len, data, &entry->value);

    entry->key = entry->value.handle;
    HASH_ADD(hh, s_buffer_table, key, sizeof(mgfx_ubh), entry);

    return (mgfx_ubh){.idx = (uint64_t)entry->key};
}

void mgfx_buffer_update(uint64_t buffer_idx, const void* data, size_t size, size_t offset) {
    buffer_entry* entry = mx_alloc(sizeof(buffer_entry), 0);
    HASH_FIND(hh, s_buffer_table, &buffer_idx, sizeof(uint64_t), entry);
    MX_ASSERT(entry != NULL, "Buffer invalid handle!");

    buffer_update(&entry->value, offset, size, data);
}

void mgfx_buffer_destroy(uint64_t idx) {
    buffer_entry* entry;
    HASH_FIND(hh, s_buffer_table, &idx, sizeof(idx), entry);

    MX_ASSERT(entry != NULL, "Buffer invalid handle!");
    buffer_destroy(&entry->value);
}

mgfx_sh mgfx_shader_create(const char* path) {
    size_t size;
    if (mx_read_file(path, &size, NULL) != MX_SUCCESS) {
        MX_LOG_ERROR("Failed to load shader: %s!", path);
        exit(-1);
    }

    mx_arena arena = mx_arena_alloc(MX_MB);

    char* shader_code = mx_arena_push(&arena, size);
    mx_read_file(path, &size, shader_code);

    shader_entry* entry = mx_alloc(sizeof(shader_entry), 0);
    memset(entry, 0, sizeof(shader_entry));

    shader_create(size, shader_code, &entry->value);

    entry->key = entry->value.module;
    HASH_ADD(hh, s_shader_table, key, sizeof(VkShaderModule), entry);

    mx_arena_free(&arena);
    return (mgfx_sh){.idx = (uint64_t)entry->key};
}

void mgfx_shader_destroy(mgfx_sh sh) {
    shader_entry* entry;
    HASH_FIND(hh, s_shader_table, &sh, sizeof(sh), entry);

    MX_ASSERT(entry != NULL, "Shader invalid handle!");

    shader_destroy(&entry->value);

    HASH_DEL(s_shader_table, entry);
    mx_free(entry);
}

mgfx_ph mgfx_program_create_graphics(mgfx_sh vsh, mgfx_sh fsh) {
    program_entry* entry = mx_alloc(sizeof(program_entry), 0);
    memset(entry, 0, sizeof(program_entry));
    entry->key.idx = (uint64_t)entry;

    entry->value.shaders[MGFX_SHADER_STAGE_VERTEX] = vsh;
    entry->value.shaders[MGFX_SHADER_STAGE_FRAGMENT] = fsh;

    HASH_ADD(hh, s_program_table, key, sizeof(entry->key), entry);

    return entry->key;
}

mgfx_ph mgfx_program_create_compute(mgfx_sh csh) {
    program_entry* entry = mx_alloc(sizeof(program_entry), 0);
    memset(entry, 0, sizeof(program_entry));

    entry->key.idx = (uint64_t)entry;

    entry->value.shaders[MGFX_SHADER_STAGE_COMPUTE] = csh;

    HASH_ADD(hh, s_program_table, key, sizeof(entry->key), entry);

    return entry->key;
}

void mgfx_program_destroy(mgfx_ph ph) {
    program_entry* entry;
    HASH_FIND(hh, s_program_table, &ph, sizeof(ph), entry);

    pipeline_destroy(&entry->value);

    HASH_DEL(s_program_table, entry);
    mx_free(entry);
}

mgfx_imgh mgfx_image_create(const mgfx_image_info* info, uint32_t usage) {
    image_entry* entry = mx_alloc(sizeof(image_entry), 0);
    memset(entry, 0, sizeof(image_entry));

    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    image_create(info, usage, 0, &entry->value);

    entry->key = entry->value.handle;
    HASH_ADD(hh, s_image_table, key, sizeof(VkImage), entry);

    return (mgfx_imgh){.idx = (uint64_t)entry->key};
}

void mgfx_image_destroy(mgfx_imgh imgh) {
    image_entry* entry;
    HASH_FIND(hh, s_image_table, &imgh, sizeof(mgfx_imgh), entry);

    if (!entry) {
        MX_LOG_WARN("Attempting to destroy invalid image handle!");
        return;
    }

    image_destroy(&entry->value);

    HASH_DEL(s_image_table, entry);
    mx_free(entry);
}

mgfx_th
mgfx_texture_create_from_memory(const mgfx_image_info* info, uint32_t filter, void* data, size_t len) {
    texture_entry* entry = mx_alloc(sizeof(texture_entry), 0);
    memset(entry, 0, sizeof(texture_entry));

    entry->value.imgh = mgfx_image_create(info, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    image_entry* image_entry;
    HASH_FIND(hh, s_image_table, &entry->value.imgh, sizeof(mgfx_imgh), image_entry);

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
        .borderColor = {},
        .unnormalizedCoordinates = {},
    };

    VK_CHECK(vkCreateSampler(s_device, &sampler_info, NULL, (VkSampler*)&entry->value.sampler));

    image_create_view(
        &image_entry->value, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, (VkImageView*)(&entry->key));
    HASH_ADD(hh, s_texture_table, key, sizeof(uint64_t), entry);

    return entry->key;
};

mgfx_th mgfx_texture_create_from_image(mgfx_imgh img, const uint32_t filter) {
    texture_entry* entry = mx_alloc(sizeof(texture_entry), 0);
    memset(entry, 0, sizeof(texture_entry));

    entry->value.imgh = img;

    image_entry* image_entry;
    HASH_FIND(hh, s_image_table, &entry->value.imgh, sizeof(mgfx_imgh), image_entry);

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
        .borderColor = {},
        .unnormalizedCoordinates = {},
    };

    // TODO: Make aspect argument
    if (image_entry->value.format == VK_FORMAT_D32_SFLOAT) {
        image_create_view(&image_entry->value,
                          VK_IMAGE_VIEW_TYPE_2D,
                          VK_IMAGE_ASPECT_DEPTH_BIT,
                          (VkImageView*)(&entry->key));
        HASH_ADD(hh, s_texture_table, key, sizeof(uint64_t), entry);
    } else {
        image_create_view(&image_entry->value,
                          VK_IMAGE_VIEW_TYPE_2D,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          (VkImageView*)(&entry->key));
        HASH_ADD(hh, s_texture_table, key, sizeof(uint64_t), entry);
    }

    VK_CHECK(vkCreateSampler(s_device, &sampler_info, NULL, (VkSampler*)&entry->value.sampler));

    return entry->key;
}

void mgfx_texture_destroy(mgfx_th th, mx_bool release_image) {
    VK_CHECK(vkDeviceWaitIdle(s_device));

    texture_entry* entry;
    HASH_FIND(hh, s_texture_table, &th, sizeof(th), entry);
    MX_ASSERT(entry != NULL, "Texture invalid handle!");

    // TODO: Move to texture_destroy
    vkDestroyImageView(s_device, (VkImageView)entry->key.idx, NULL);
    vkDestroySampler(s_device, (VkSampler)entry->value.sampler, NULL);

    HASH_DEL(s_texture_table, entry);
    if (release_image) {
        mgfx_image_destroy(entry->value.imgh);
    }

    mx_free(entry);
}

mgfx_dh mgfx_descriptor_create(const char* name, uint32_t type) {
    descriptor_entry* entry = mx_alloc(sizeof(descriptor_entry), 0);
    memset(entry, 0, sizeof(descriptor_entry));

    entry->key = (mgfx_dh){.idx = (uint64_t)(entry)};
    entry->value = (descriptor_info_vk){.type = type};

    MX_ASSERT(strlen(name) <= sizeof(entry->value.name));
    strcpy(entry->value.name, name);

    HASH_ADD(hh, s_descriptor_table, key, sizeof(uint64_t), entry);

    return entry->key;
}

void mgfx_set_buffer(mgfx_dh dh, mgfx_ubh ubh) {
    descriptor_entry* entry;
    HASH_FIND(hh, s_descriptor_table, &dh, sizeof(dh), entry);
    MX_ASSERT(entry != NULL, "Descriptor invalid handle!");

    buffer_entry* buffer_entry;
    HASH_FIND(hh, s_buffer_table, &ubh, sizeof(ubh), buffer_entry);
    MX_ASSERT(buffer_entry != NULL, "buffer invalid handle!");

    entry->value.buffer_info.buffer = (VkBuffer)ubh.idx;
    entry->value.buffer_info.offset = 0;
    entry->value.buffer_info.range = VK_WHOLE_SIZE;

    entry->value.buffer = &buffer_entry->value;
}

void mgfx_set_texture(mgfx_dh dh, mgfx_th th) {
    descriptor_entry* entry;
    HASH_FIND(hh, s_descriptor_table, &dh, sizeof(dh), entry);
    MX_ASSERT(entry != NULL, "Descriptor invalid handle!");

    texture_entry* texture_entry;
    HASH_FIND(hh, s_texture_table, &th, sizeof(th), texture_entry);
    MX_ASSERT(texture_entry != NULL, "Texture invalid handle!");

    entry->value.image_info.imageView = (VkImageView)th.idx;
    entry->value.image_info.sampler = (VkSampler)texture_entry->value.sampler;
    entry->value.image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_entry* image_entry;
    HASH_FIND(hh, s_image_table, &texture_entry->value.imgh, sizeof(mgfx_imgh), image_entry);
    MX_ASSERT(image_entry != NULL, "Image invalid handle!");
    entry->value.image = &image_entry->value;
}

void mgfx_descriptor_destroy(mgfx_dh dh) {}

mgfx_fbh mgfx_framebuffer_create(mgfx_imgh* color_attachments,
                                 uint32_t color_attachment_count,
                                 mgfx_imgh depth_attachment) {
    // TODO: Framebuffer should be an api agonstic concept.
    framebuffer_entry* entry = mx_alloc(sizeof(framebuffer_entry), 0);
    memset(entry, 0, sizeof(framebuffer_entry));

    framebuffer_vk* fb = &entry->value;
    fb->color_attachment_count = color_attachment_count;
    for (int i = 0; i < color_attachment_count; i++) {
        image_entry* color_entry;
        HASH_FIND(hh, s_image_table, &color_attachments[i], sizeof(mgfx_imgh), color_entry);

        fb->color_attachments[i] = &color_entry->value;
        image_create_view(fb->color_attachments[i],
                          VK_IMAGE_VIEW_TYPE_2D,
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          &fb->color_attachment_views[i]);
    }

    image_entry* depth_entry;
    HASH_FIND(hh, s_image_table, &depth_attachment, sizeof(mgfx_imgh), depth_entry);
    if (depth_entry) {
        fb->depth_attachment = &depth_entry->value;
        image_create_view(fb->depth_attachment,
                          VK_IMAGE_VIEW_TYPE_2D,
                          VK_IMAGE_ASPECT_DEPTH_BIT,
                          &fb->depth_attachment_view);
    }

    entry->key.idx = (uint64_t)fb;
    HASH_ADD(hh, s_framebuffer_table, key, sizeof(mgfx_fbh), entry);

    return entry->key;
}

void mgfx_framebuffer_destroy(mgfx_fbh fbh) {
    framebuffer_entry* framebuffer_entry;
    HASH_FIND(hh, s_framebuffer_table, &fbh, sizeof(mgfx_fbh), framebuffer_entry);

    if (!framebuffer_entry) {
        MX_LOG_WARN("Attempting to destroy invalid framebuffer handle!");
        return;
    }

    framebuffer_destroy(&framebuffer_entry->value);

    HASH_DEL(s_framebuffer_table, framebuffer_entry);
    mx_free(framebuffer_entry);
}

void mgfx_bind_vertex_buffer(mgfx_vbh vbh) {
    mgfx_draw* current_draw = &s_draws[s_draw_count];
    current_draw->vbhs[current_draw->vbh_count] = vbh;

    ++current_draw->vbh_count;
}

void mgfx_bind_index_buffer(mgfx_ibh ibh) {
    mgfx_draw* current_draw = &s_draws[s_draw_count];
    current_draw->ibh = ibh;
}

void mgfx_bind_descriptor(uint32_t ds_idx, mgfx_dh dh) {
    MX_ASSERT(ds_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET);

    mgfx_draw* current_draw = &s_draws[s_draw_count];

    uint32_t descriptor_idx = current_draw->descriptor_sets[ds_idx].dh_count;
    MX_ASSERT(descriptor_idx < MGFX_SHADER_MAX_DESCRIPTOR_BINDING);

    current_draw->descriptor_sets[ds_idx].dhs[descriptor_idx] = dh;
    ++current_draw->descriptor_sets[ds_idx].dh_count;
}

void mgfx_set_view_target(uint8_t target, mgfx_fbh fb) { s_view_targets[target] = fb; }

void mgfx_set_transform(const float* mtx) { memcpy(s_current_transform, mtx, sizeof(float) * 16); }

void mgfx_set_view(const float* mtx) { memcpy(s_current_view, mtx, sizeof(float) * 16); }

void mgfx_set_proj(const float* mtx) { memcpy(s_current_proj, mtx, sizeof(float) * 16); }

void mgfx_submit(uint8_t target, mgfx_ph ph) {
    program_entry* entry;
    HASH_FIND(hh, s_program_table, &ph, sizeof(ph), entry);
    MX_ASSERT(entry != NULL, "Program invalid handle!");

    if ((VkPipeline)entry->value.pipeline == VK_NULL_HANDLE) {
        mgfx_program* program = &entry->value;

        shader_entry* vs_entry;
        HASH_FIND(hh,
                  s_shader_table,
                  &program->shaders[MGFX_SHADER_STAGE_VERTEX],
                  sizeof(VkShaderModule),
                  vs_entry);
        const shader_vk* vs = vs_entry ? &vs_entry->value : NULL;

        shader_entry* fs_entry;
        HASH_FIND(hh,
                  s_shader_table,
                  &program->shaders[MGFX_SHADER_STAGE_FRAGMENT],
                  sizeof(VkShaderModule),
                  fs_entry);
        const shader_vk* fs = fs_entry ? &fs_entry->value : NULL;

        if (target == MGFX_DEFAULT_VIEW_TARGET) {
            pipeline_create_graphics(vs, fs, &s_swapchain.framebuffer, program);
        } else {
            framebuffer_entry* fb_entry;
            HASH_FIND(hh, s_framebuffer_table, &s_view_targets[target], sizeof(mgfx_fbh), fb_entry);

            if (!fb_entry) {
                MX_LOG_ERROR("Submitting to unknown view target '%d'! Please call mgfx_set_view_target().",
                             target);
            }

            pipeline_create_graphics(vs, fs, &fb_entry->value, program);
        }
    }

    mgfx_draw* current_draw = &s_draws[s_draw_count];
    current_draw->view_target = target;
    current_draw->ph = ph;

    memcpy(current_draw->draw_pc.model, s_current_transform, sizeof(float) * 16);
    memcpy(current_draw->draw_pc.view, s_current_view, sizeof(float) * 16);
    memcpy(current_draw->draw_pc.proj, s_current_proj, sizeof(float) * 16);

    current_draw->sort_key = ((uint64_t)(target) << 56) | // highest priority (view)
                             ((uint64_t)(ph.idx) << 40);  // then shader program
                                                          /*| ((uint64_t)(materialhash) << 8);*/
    ++s_draw_count;
}

void mgfx_frame() {
    // Get current frame.
    frame_vk* frame = &s_frames[s_frame_idx];

    // Wait and reset render fence of current frame idx.
    VK_CHECK(vkWaitForFences(s_device, 1, &frame->render_fence, VK_TRUE, UINT64_MAX));

    if (!swapchain_update(frame, s_width, s_height, &s_swapchain)) {
        return;
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
        VkMemoryBarrier vb_copy_barriers[MGFX_MAX_FRAME_BUFFER_COPIES];
        uint32_t vb_copy_barrier_count = 0;

        for (int i = 0; i < s_buffer_to_buffer_copy_count; i++) {
            vkCmdCopyBuffer(frame->cmd,
                            s_buffer_to_buffer_copy_queue[i].src->handle,
                            s_buffer_to_buffer_copy_queue[i].dst->handle,
                            1,
                            &s_buffer_to_buffer_copy_queue[i].copy);

            if ((s_buffer_to_buffer_copy_queue[i].dst->usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ==
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
                vb_copy_barriers[vb_copy_barrier_count] =
                    (VkMemoryBarrier){.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                      .pNext = NULL,
                                      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                      .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT};

                ++vb_copy_barrier_count;
                continue;
            };

            if ((s_buffer_to_buffer_copy_queue[i].dst->usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ==
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
                vb_copy_barriers[vb_copy_barrier_count] =
                    (VkMemoryBarrier){.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                      .pNext = NULL,
                                      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                      .dstAccessMask = VK_ACCESS_INDEX_READ_BIT};

                ++vb_copy_barrier_count;
                continue;
            };
        };

        // Vertex buffer memory barriers
        vkCmdPipelineBarrier(frame->cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                             0,
                             vb_copy_barrier_count,
                             vb_copy_barriers,
                             0,
                             NULL,
                             0,
                             NULL);
        s_buffer_to_buffer_copy_count = 0;
    }

    // Buffer to image copy queue
    if (s_buffer_to_image_copy_count > 0) {
        VkImageMemoryBarrier sampled_barriers[MGFX_MAX_FRAME_BUFFER_COPIES];
        uint32_t sampled_copy_count = 0;

        VkImageMemoryBarrier pre_copy_barriers[MGFX_MAX_FRAME_BUFFER_COPIES];
        uint32_t pre_copy_count = 0;

        for (int i = 0; i < s_buffer_to_image_copy_count; i++) {
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
                        .aspectMask = s_buffer_to_image_copy_queue[i].copy.imageSubresource.aspectMask,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer =
                            s_buffer_to_image_copy_queue[i].copy.imageSubresource.baseArrayLayer,
                        .layerCount = s_buffer_to_image_copy_queue[i].copy.imageSubresource.layerCount,
                    },
            };
            s_buffer_to_image_copy_queue[i].dst->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

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
                        .aspectMask = s_buffer_to_image_copy_queue[i].copy.imageSubresource.aspectMask,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer =
                            s_buffer_to_image_copy_queue[i].copy.imageSubresource.baseArrayLayer,
                        .layerCount = s_buffer_to_image_copy_queue[i].copy.imageSubresource.layerCount,
                    },
            };

            s_buffer_to_image_copy_queue[i].dst->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        // Fragment Sampled image memory barriers
        vkCmdPipelineBarrier(frame->cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             NULL,
                             0,
                             NULL,
                             pre_copy_count,
                             pre_copy_barriers);

        for (int i = 0; i < s_buffer_to_image_copy_count; i++) {
            vkCmdCopyBufferToImage(frame->cmd,
                                   s_buffer_to_image_copy_queue[i].src->handle,
                                   s_buffer_to_image_copy_queue[i].dst->handle,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &s_buffer_to_image_copy_queue[i].copy);
        }

        // Fragment Sampled image memory barriers
        vkCmdPipelineBarrier(frame->cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             NULL,
                             0,
                             NULL,
                             sampled_copy_count,
                             sampled_barriers);

        s_buffer_to_image_copy_count = 0;
    }

    vk_cmd_transition_image(frame->cmd,
                            &s_swapchain.images[s_swapchain.free_idx],
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clear = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Sort draws by view target and program
    qsort(s_draws, s_draw_count, sizeof(mgfx_draw), draw_compare_fn);

    mgfx_program* current_program = NULL;

    framebuffer_vk* fb = NULL;
    uint8_t target = 10;

    VkDescriptorSet flat_ds[MGFX_SHADER_MAX_DESCRIPTOR_SET];
    uint32_t flat_ds_count = 0;

    for (uint32_t draw_idx = 0; draw_idx < s_draw_count; draw_idx++) {
        const mgfx_draw* draw = &s_draws[draw_idx];

        if (draw->view_target != target) {
            if (fb != NULL) {
                vk_cmd_end_rendering(frame->cmd);
            }

            target = draw->view_target;

            if (target == MGFX_DEFAULT_VIEW_TARGET) {
                fb = &s_swapchain.framebuffer;
            } else {
                framebuffer_entry* fb_entry;
                HASH_FIND(hh, s_framebuffer_table, &s_view_targets[target], sizeof(mgfx_fbh), fb_entry);
                fb = &fb_entry->value;
            }

            program_entry* program_entry;
            HASH_FIND(hh, s_program_table, &draw->ph, sizeof(mgfx_ph), program_entry);

            if (current_program != &program_entry->value) {
                current_program = &program_entry->value;
                vkCmdBindPipeline(
                    frame->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)current_program->pipeline);
            }

            for (uint32_t ds_idx = 0; ds_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET; ds_idx++) {
                if (draw->descriptor_sets[ds_idx].dh_count <= 0) {
                    continue;
                }
                // TODO: Pre pass resource barriers and transitions
                for (uint32_t binding_idx = 0; binding_idx < draw->descriptor_sets[ds_idx].dh_count;
                     binding_idx++) {
                    mgfx_dh dh = draw->descriptor_sets[ds_idx].dhs[binding_idx];
                    const descriptor_entry* descriptor_entry;

                    HASH_FIND(hh, s_descriptor_table, &dh, sizeof(mgfx_dh), descriptor_entry);
                    MX_ASSERT(descriptor_entry != NULL, "Descriptor invalid handle!");

                    switch (descriptor_entry->value.type) {
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                        if (descriptor_entry->value.image->layout !=
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                            switch (descriptor_entry->value.image->format) {
                            case (VK_FORMAT_D32_SFLOAT):
                                vk_cmd_transition_image(frame->cmd,
                                                        descriptor_entry->value.image,
                                                        VK_IMAGE_ASPECT_DEPTH_BIT,
                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                                break;
                            default:
                                vk_cmd_transition_image(frame->cmd,
                                                        descriptor_entry->value.image,
                                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                                break;
                            }
                        }
                        break;
                    case VK_DESCRIPTOR_TYPE_MAX_ENUM:
                    default:
                        break;
                    }
                }
            }

            for (uint32_t color_attachment_idx = 0; color_attachment_idx < fb->color_attachment_count;
                 color_attachment_idx++) {
                vk_cmd_transition_image(frame->cmd,
                                        fb->color_attachments[color_attachment_idx],
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL);

                vk_cmd_clear_image(frame->cmd, fb->color_attachments[0], &range, &clear);
            }

            if (fb->depth_attachment) {
                vk_cmd_transition_image(
                    frame->cmd, fb->depth_attachment, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_GENERAL);
            }

            vk_cmd_begin_rendering(frame->cmd, fb);
        }

        flat_ds_count = 0;
        for (uint32_t ds_idx = 0; ds_idx < MGFX_SHADER_MAX_DESCRIPTOR_SET; ds_idx++) {
            if (draw->descriptor_sets[ds_idx].dh_count <= 0) {
                continue;
            }
            uint32_t ds_hash;
            mx_murmur_hash_32(draw->descriptor_sets[ds_idx].dhs,
                              draw->descriptor_sets[ds_idx].dh_count * sizeof(mgfx_dh),
                              (uint32_t)draw->ph.idx,
                              &ds_hash);

            descriptor_set_entry* ds_entry;
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
                .pSetLayouts = (VkDescriptorSetLayout*)&current_program->dsls[ds_idx],
            };

            ds_entry = mx_alloc(sizeof(descriptor_set_entry), 0);
            memset(ds_entry, 0, sizeof(descriptor_set_entry));

            ds_entry->key = ds_hash;
            HASH_ADD_INT(s_descriptor_set_table, key, ds_entry);

            VK_CHECK(vkAllocateDescriptorSets(s_device, &descriptor_set_alloc_info, &ds_entry->value));
            flat_ds[flat_ds_count++] = ds_entry->value;

            for (uint32_t binding_idx = 0; binding_idx < draw->descriptor_sets[ds_idx].dh_count;
                 binding_idx++) {

                mgfx_dh dh = draw->descriptor_sets[ds_idx].dhs[binding_idx];
                const descriptor_entry* descriptor_entry;

                HASH_FIND(hh, s_descriptor_table, &dh, sizeof(mgfx_dh), descriptor_entry);
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

        if (flat_ds_count > 0) {
            vkCmdBindDescriptorSets(frame->cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    (VkPipelineLayout)current_program->pipeline_layout,
                                    0,
                                    flat_ds_count,
                                    flat_ds,
                                    0,
                                    NULL);
        }

        VkDeviceSize offsets = 0;
        if (draw->vbh_count > 0) {
            vkCmdBindVertexBuffers(frame->cmd, 0, draw->vbh_count, (VkBuffer*)draw->vbhs, &offsets);
        }

        if (draw->ibh.idx != (uint64_t)VK_NULL_HANDLE) {
            vkCmdBindIndexBuffer(frame->cmd, (VkBuffer)draw->ibh.idx, 0, VK_INDEX_TYPE_UINT32);
        }

        vkCmdPushConstants(frame->cmd,
                           (VkPipelineLayout)current_program->pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(draw->draw_pc),
                           &draw->draw_pc);

        if (draw->ibh.idx != (uint64_t)VK_NULL_HANDLE) {
            const buffer_entry* index_buffer_entry;
            HASH_FIND(hh, s_buffer_table, &draw->ibh, sizeof(mgfx_ibh), index_buffer_entry);

            if (index_buffer_entry) {
                VmaAllocationInfo index_buffer_alloc_info = {};
                vmaGetAllocationInfo(
                    s_allocator, index_buffer_entry->value.allocation, &index_buffer_alloc_info);

                uint32_t count = index_buffer_alloc_info.size / sizeof(uint32_t);
                vkCmdDrawIndexed(frame->cmd, count, 1, 0, 0, 0);
            } else {
                MX_LOG_WARN("Index buffer invlaid handle!");
            }

        } else {
            const buffer_entry* vertex_buffer_entry;
            HASH_FIND(hh, s_buffer_table, &draw->vbhs[0], sizeof(mgfx_vbh), vertex_buffer_entry);

            if (vertex_buffer_entry) {
                VmaAllocationInfo vertex_buffer_alloc_info = {};
                vmaGetAllocationInfo(
                    s_allocator, vertex_buffer_entry->value.allocation, &vertex_buffer_alloc_info);

                uint32_t count = vertex_buffer_alloc_info.size / sizeof(uint32_t);
                vkCmdDraw(frame->cmd, count, 1, 0, 0);
            } else {
                MX_LOG_WARN("Invalid vertex buffer handle!");
            }
        }
    }

    // Clear draws
    memset(s_draws, 0, s_draw_count * sizeof(mgfx_draw));
    s_draw_count = 0;

    if (fb != NULL) {
        vk_cmd_end_rendering(frame->cmd);
    }

    vk_cmd_transition_image(frame->cmd,
                            &s_swapchain.images[s_swapchain.free_idx],
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(frame->cmd));

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
    VK_CHECK(vkQueueSubmit(s_queues[MGFX_QUEUE_GRAPHICS], 1, &submit_info, frame->render_fence));

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

    VkResult present_result = vkQueuePresentKHR(s_queues[MGFX_QUEUE_GRAPHICS], &present_info);
    switch (present_result) {
    case (VK_SUBOPTIMAL_KHR):
        break;
    case (VK_ERROR_OUT_OF_DATE_KHR): {
        s_swapchain.resize = MX_TRUE;
        return;
    } break;

    default:
        VK_CHECK(present_result);
        break;
    }

    s_frame_idx = (s_frame_idx + 1) % MGFX_FRAME_COUNT;
};

void mgfx_shutdown() {
    VK_CHECK(vkDeviceWaitIdle(s_device));

    buffer_pool_destroy(&s_vb_staging_pool);
    buffer_pool_destroy(&s_ib_staging_pool);
    buffer_destroy(&s_image_staging_buffer);

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
    s_width = width;
    s_height = height;
}

void vertex_layout_begin(mgfx_vertex_layout* vl) { memset(vl, 0, sizeof(mgfx_vertex_layout)); }

void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size) {
    vl->attribute_offsets[attribute] = vl->stride;
    vl->attribute_sizes[attribute] = size;
    vl->stride += size;
};

void vertex_layout_end(mgfx_vertex_layout* vl) {}
