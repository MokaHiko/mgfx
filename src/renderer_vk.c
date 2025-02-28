#include "renderer_vk.h"

#include "mgfx/defines.h"
#include <vulkan/vulkan_core.h>

#ifdef MX_MACOS
#include <GLFW/glfw3.h>
#endif

#ifdef MX_WIN32
#include <GLFW/glfw3.h>
#endif

#include <mx/mx_math.h>
#include <mx/mx_memory.h>

#include <string.h>

uint32_t vk_format_size(VkFormat format) {
  uint32_t result = 0;
  switch (format) {
    case VK_FORMAT_UNDEFINED:
      result = 0;
      break;
    case VK_FORMAT_R4G4_UNORM_PACK8:
      result = 1;
      break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
      result = 2;
      break;
    case VK_FORMAT_R8_UNORM:
      result = 1;
      break;
    case VK_FORMAT_R8_SNORM:
      result = 1;
      break;
    case VK_FORMAT_R8_USCALED:
      result = 1;
      break;
    case VK_FORMAT_R8_SSCALED:
      result = 1;
      break;
    case VK_FORMAT_R8_UINT:
      result = 1;
      break;
    case VK_FORMAT_R8_SINT:
      result = 1;
      break;
    case VK_FORMAT_R8_SRGB:
      result = 1;
      break;
    case VK_FORMAT_R8G8_UNORM:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SNORM:
      result = 2;
      break;
    case VK_FORMAT_R8G8_USCALED:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SSCALED:
      result = 2;
      break;
    case VK_FORMAT_R8G8_UINT:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SINT:
      result = 2;
      break;
    case VK_FORMAT_R8G8_SRGB:
      result = 2;
      break;
    case VK_FORMAT_R8G8B8_UNORM:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SNORM:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_USCALED:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SSCALED:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_UINT:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SINT:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8_SRGB:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_UNORM:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SNORM:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_USCALED:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SSCALED:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_UINT:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SINT:
      result = 3;
      break;
    case VK_FORMAT_B8G8R8_SRGB:
      result = 3;
      break;
    case VK_FORMAT_R8G8B8A8_UNORM:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SNORM:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_USCALED:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SSCALED:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_UINT:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SINT:
      result = 4;
      break;
    case VK_FORMAT_R8G8B8A8_SRGB:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_UNORM:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SNORM:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_USCALED:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SSCALED:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_UINT:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SINT:
      result = 4;
      break;
    case VK_FORMAT_B8G8R8A8_SRGB:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_R16_UNORM:
      result = 2;
      break;
    case VK_FORMAT_R16_SNORM:
      result = 2;
      break;
    case VK_FORMAT_R16_USCALED:
      result = 2;
      break;
    case VK_FORMAT_R16_SSCALED:
      result = 2;
      break;
    case VK_FORMAT_R16_UINT:
      result = 2;
      break;
    case VK_FORMAT_R16_SINT:
      result = 2;
      break;
    case VK_FORMAT_R16_SFLOAT:
      result = 2;
      break;
    case VK_FORMAT_R16G16_UNORM:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SNORM:
      result = 4;
      break;
    case VK_FORMAT_R16G16_USCALED:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SSCALED:
      result = 4;
      break;
    case VK_FORMAT_R16G16_UINT:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SINT:
      result = 4;
      break;
    case VK_FORMAT_R16G16_SFLOAT:
      result = 4;
      break;
    case VK_FORMAT_R16G16B16_UNORM:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SNORM:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_USCALED:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SSCALED:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_UINT:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SINT:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16_SFLOAT:
      result = 6;
      break;
    case VK_FORMAT_R16G16B16A16_UNORM:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SNORM:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_USCALED:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SSCALED:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_UINT:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SINT:
      result = 8;
      break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      result = 8;
      break;
    case VK_FORMAT_R32_UINT:
      result = 4;
      break;
    case VK_FORMAT_R32_SINT:
      result = 4;
      break;
    case VK_FORMAT_R32_SFLOAT:
      result = 4;
      break;
    case VK_FORMAT_R32G32_UINT:
      result = 8;
      break;
    case VK_FORMAT_R32G32_SINT:
      result = 8;
      break;
    case VK_FORMAT_R32G32_SFLOAT:
      result = 8;
      break;
    case VK_FORMAT_R32G32B32_UINT:
      result = 12;
      break;
    case VK_FORMAT_R32G32B32_SINT:
      result = 12;
      break;
    case VK_FORMAT_R32G32B32_SFLOAT:
      result = 12;
      break;
    case VK_FORMAT_R32G32B32A32_UINT:
      result = 16;
      break;
    case VK_FORMAT_R32G32B32A32_SINT:
      result = 16;
      break;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      result = 16;
      break;
    case VK_FORMAT_R64_UINT:
      result = 8;
      break;
    case VK_FORMAT_R64_SINT:
      result = 8;
      break;
    case VK_FORMAT_R64_SFLOAT:
      result = 8;
      break;
    case VK_FORMAT_R64G64_UINT:
      result = 16;
      break;
    case VK_FORMAT_R64G64_SINT:
      result = 16;
      break;
    case VK_FORMAT_R64G64_SFLOAT:
      result = 16;
      break;
    case VK_FORMAT_R64G64B64_UINT:
      result = 24;
      break;
    case VK_FORMAT_R64G64B64_SINT:
      result = 24;
      break;
    case VK_FORMAT_R64G64B64_SFLOAT:
      result = 24;
      break;
    case VK_FORMAT_R64G64B64A64_UINT:
      result = 32;
      break;
    case VK_FORMAT_R64G64B64A64_SINT:
      result = 32;
      break;
    case VK_FORMAT_R64G64B64A64_SFLOAT:
      result = 32;
      break;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
      result = 4;
      break;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
      result = 4;
      break;

    default:
      break;
  }
  return result;
}

VkResult get_window_surface_vk(VkInstance instance, void* nwh, VkSurfaceKHR* surface) {
  GLFWwindow* window = (GLFWwindow*)nwh;
  return glfwCreateWindowSurface(instance, window, NULL, surface);
}

int choose_physical_device_vk(VkInstance instance, uint32_t device_ext_count, const char** device_exts, VkPhysicalDevice* phys_device, MxArena* arena) {
  uint32_t physical_device_count = 0;
  vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);

  if(physical_device_count <= 0) {
    return -1;
  }

  VkPhysicalDevice* physical_devices = mx_arena_push(arena, sizeof(VkPhysicalDevice) * physical_device_count);
  vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);

  VkPhysicalDeviceProperties physical_device_props = {};
  VkPhysicalDeviceFeatures physical_device_features = {};

  printf("Device: \n");
  for(uint32_t i = 0; i < physical_device_count; i++) {
    vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_props);

    printf("\tdeviceName : %s\n", physical_device_props.deviceName);
    printf("\tapiVersion : %d.%d.%d\n", VK_API_VERSION_MAJOR(physical_device_props.apiVersion),
                                        VK_API_VERSION_MINOR(physical_device_props.apiVersion),
                                        VK_API_VERSION_PATCH(physical_device_props.apiVersion)),
    printf("\tdriverVersion : %d.%d.%d\n", VK_API_VERSION_MAJOR(physical_device_props.driverVersion),
                                           VK_API_VERSION_MINOR(physical_device_props.driverVersion),
                                           VK_API_VERSION_PATCH(physical_device_props.driverVersion)),

    printf("\tvendorID: 0x%X\n", physical_device_props.vendorID);
    printf("\tdeviceID: 0x%X\n", physical_device_props.deviceID);
    printf("\tdeviceType: %d\n", physical_device_props.deviceType);

    printf("\tLimits:\n");
    printf("\t\tmaxImageDimension1D: %d \n", physical_device_props.limits.maxImageDimension1D);
    printf("\t\tmaxImageDimension2D: %d \n", physical_device_props.limits.maxImageDimension2D);
    printf("\t\tmaxImageDimension3D: %d \n", physical_device_props.limits.maxImageDimension3D);
    printf("\t\tmaxImageDimensionCube: %d \n", physical_device_props.limits.maxImageDimensionCube);
    printf("\t\tmaxImageArrayLayers: %d \n", physical_device_props.limits.maxImageArrayLayers);
    printf("\t\tmaxTexelBufferElements: %d \n", physical_device_props.limits.maxTexelBufferElements);
    printf("\t\tmaxUniformBufferRange: %d \n", physical_device_props.limits.maxUniformBufferRange);
    printf("\t\tmaxStorageBufferRange: %d \n", physical_device_props.limits.maxStorageBufferRange);

    printf("\t\tmaxPushConstantsSize: %d \n", physical_device_props.limits.maxPushConstantsSize);
    printf("\t\tmaxMemoryAllocationCount: %d \n", physical_device_props.limits.maxMemoryAllocationCount);
    printf("\t\tmaxSamplerAllocationCount: %d \n", physical_device_props.limits.maxSamplerAllocationCount);
    printf("\t\tmaxBoundDescriptorSets: %d \n", physical_device_props.limits.maxBoundDescriptorSets);
    printf("\t\tmaxPerStageDescriptorSamplers: %d \n", physical_device_props.limits.maxPerStageDescriptorSamplers);
    printf("\t\tmaxPerStageDescriptorUniformBuffers: %d \n", physical_device_props.limits.maxPerStageDescriptorUniformBuffers);
    printf("\t\tmaxPerStageDescriptorStorageBuffers: %d \n", physical_device_props.limits.maxPerStageDescriptorStorageBuffers);
    printf("\t\tmaxPerStageDescriptorSampledImages: %d \n", physical_device_props.limits.maxPerStageDescriptorSampledImages);
    printf("\t\tmaxPerStageDescriptorStorageImages: %d \n", physical_device_props.limits.maxPerStageDescriptorStorageImages);
    printf("\t\tmaxPerStageDescriptorInputAttachments: %d \n", physical_device_props.limits.maxPerStageDescriptorInputAttachments);
    printf("\t\tmaxPerStageResources: %d \n", physical_device_props.limits.maxPerStageResources);
    printf("\t\tmaxDescriptorSetSamplers: %d \n", physical_device_props.limits.maxDescriptorSetSamplers);
    printf("\t\tmaxDescriptorSetUniformBuffers: %d \n", physical_device_props.limits.maxDescriptorSetUniformBuffers);
    printf("\t\tmaxDescriptorSetUniformBuffersDynamic: %d \n", physical_device_props.limits.maxDescriptorSetUniformBuffersDynamic);
    printf("\t\tmaxDescriptorSetStorageBuffers: %d \n", physical_device_props.limits.maxDescriptorSetStorageBuffers);
    printf("\t\tmaxDescriptorSetStorageBuffersDynamic: %d \n", physical_device_props.limits.maxDescriptorSetStorageBuffersDynamic);
    printf("\t\tmaxDescriptorSetSampledImages: %d \n", physical_device_props.limits.maxDescriptorSetSampledImages);
    printf("\t\tmaxDescriptorSetStorageImages: %d \n", physical_device_props.limits.maxDescriptorSetStorageImages);
    printf("\t\tmaxDescriptorSetInputAttachments: %d \n", physical_device_props.limits.maxDescriptorSetInputAttachments);
    printf("\t\tmaxVertexInputAttributes: %d \n", physical_device_props.limits.maxVertexInputAttributes);
    printf("\t\tmaxVertexInputBindings: %d \n", physical_device_props.limits.maxVertexInputBindings);
    printf("\t\tmaxVertexOutputComponents: %d \n", physical_device_props.limits.maxVertexOutputComponents);
    printf("\t\tmaxFragmentInputComponents: %d \n", physical_device_props.limits.maxFragmentInputComponents);
    printf("\t\tmaxFragmentOutputAttachments: %d \n", physical_device_props.limits.maxFragmentOutputAttachments);
    printf("\t\tmaxFragmentCombinedOutputResources: %d \n", physical_device_props.limits.maxFragmentCombinedOutputResources);
    printf("\t\tmaxComputeSharedMemorySize: %d \n", physical_device_props.limits.maxComputeSharedMemorySize);
    printf("\t\tmaxComputeWorkGroupInvocations: %d \n", physical_device_props.limits.maxComputeWorkGroupInvocations);
    printf("\t\tmaxDrawIndexedIndexValue: %d \n", physical_device_props.limits.maxDrawIndexedIndexValue);
    printf("\t\tmaxDrawIndirectCount: %d \n", physical_device_props.limits.maxDrawIndirectCount);
    printf("\t\tmaxSamplerLodBias: %f \n", physical_device_props.limits.maxSamplerLodBias);
    printf("\t\tmaxSamplerAnisotropy: %f \n", physical_device_props.limits.maxSamplerAnisotropy);
    printf("\t\tmaxViewports: %d \n", physical_device_props.limits.maxViewports);
    printf("\t\tmaxViewportDimensions: %d x %d \n", physical_device_props.limits.maxViewportDimensions[0], physical_device_props.limits.maxViewportDimensions[1]);
    printf("\t\tviewportBoundsRange: %f x %.f\n", physical_device_props.limits.viewportBoundsRange[0], physical_device_props.limits.viewportBoundsRange[1]);
    printf("\t\tviewportSubPixelBits: %d \n", physical_device_props.limits.viewportSubPixelBits);
    printf("\t\tminMemoryMapAlignment: %zu \n", physical_device_props.limits.minMemoryMapAlignment);
    printf("\t\tminTexelBufferOffsetAlignment: %llu \n", physical_device_props.limits.minTexelBufferOffsetAlignment);
    printf("\t\tminUniformBufferOffsetAlignment: %llu \n", physical_device_props.limits.minUniformBufferOffsetAlignment);
    printf("\t\tminStorageBufferOffsetAlignment: %llu \n", physical_device_props.limits.minStorageBufferOffsetAlignment);
    printf("\t\tmaxFramebufferWidth: %d \n", physical_device_props.limits.maxFramebufferWidth);
    printf("\t\tmaxFramebufferHeight: %d \n", physical_device_props.limits.maxFramebufferHeight);
    printf("\t\tmaxFramebufferLayers: %d \n", physical_device_props.limits.maxFramebufferLayers);
    printf("\t\tframebufferColorSampleCounts: %d \n", physical_device_props.limits.framebufferColorSampleCounts);
    printf("\t\tframebufferDepthSampleCounts: %d \n", physical_device_props.limits.framebufferDepthSampleCounts);
    printf("\t\tmaxColorAttachments: %d \n", physical_device_props.limits.maxColorAttachments);
    printf("\t\tsampledImageColorSampleCounts: %d \n", physical_device_props.limits.sampledImageColorSampleCounts);
    printf("\t\tsampledImageIntegerSampleCounts: %d \n", physical_device_props.limits.sampledImageIntegerSampleCounts);
    printf("\t\tsampledImageDepthSampleCounts: %d \n", physical_device_props.limits.sampledImageDepthSampleCounts);
    printf("\t\tsampledImageStencilSampleCounts: %d \n", physical_device_props.limits.sampledImageStencilSampleCounts);
    printf("\t\tstorageImageSampleCounts: %d \n", physical_device_props.limits.storageImageSampleCounts);
    printf("\t\tmaxSampleMaskWords: %d \n", physical_device_props.limits.maxSampleMaskWords);
    printf("\t\tmaxCombinedClipAndCullDistances: %d \n", physical_device_props.limits.maxCombinedClipAndCullDistances);
    printf("\t\toptimalBufferCopyOffsetAlignment: %llu \n", physical_device_props.limits.optimalBufferCopyOffsetAlignment);

    vkGetPhysicalDeviceFeatures(physical_devices[i], &physical_device_features);

    uint32_t avail_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &avail_ext_count, NULL);
    VkExtensionProperties* avail_ext_props = mx_arena_push(arena,
                                                          avail_ext_count * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &avail_ext_count, avail_ext_props);

    int validated_device_ext_count = 0;
    for(uint32_t req_ext_idx = 0; req_ext_idx < device_ext_count; req_ext_idx++) {
      for(int avail_ext_idx = 0; avail_ext_idx < avail_ext_count; avail_ext_idx++) {
        if(strcmp(device_exts[req_ext_idx], avail_ext_props[avail_ext_idx].extensionName) == 0) {
          ++validated_device_ext_count;
          continue;
        }
      }
    }

    if(validated_device_ext_count != device_ext_count) {
      return -1;
    }
  }


  *phys_device = physical_devices[0];
  return MGFX_SUCCESS;
}

void choose_swapchain_extent_vk(const VkSurfaceCapabilitiesKHR* surface_caps,
                                void* nwh,
                                VkExtent2D* extent) {
  if(surface_caps->currentExtent.width != UINT32_MAX) {
      *extent = surface_caps->currentExtent;
  } else {
    GLFWwindow* window = (GLFWwindow*)nwh;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    width = clampf(width, surface_caps->minImageExtent.width, surface_caps->maxImageExtent.width);
    height = clampf(height, surface_caps->minImageExtent.height, surface_caps->maxImageExtent.height);

    extent->width = width;
    extent->height = height;
  }
};

void vk_cmd_transition_image(VkCommandBuffer cmd,
                             image_vk* image,
                             VkImageAspectFlags aspect_flags,
                             VkImageLayout new_layout) {
  VkImageMemoryBarrier img_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = NULL,
    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,

    .oldLayout = image->layout,
    .newLayout = new_layout,

    /*// Assuming single queue*/
    /*.srcQueueFamilyIndex,*/
    /*.dstQueueFamilyIndex,*/

    .image = image->handle,
    .subresourceRange = {
      .aspectMask = aspect_flags,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    },
  };

  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // src stage mask
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dst stage mask
                       0,                                  // dependency flag
                       0, NULL,
                       0, NULL,
                       1, &img_barrier);

  image->layout = new_layout;
}

void vk_cmd_copy_image_to_image(VkCommandBuffer cmd,
                                const image_vk* src,
                                VkImageAspectFlags aspect,
                                image_vk* dst) {
  VkImageBlit blit = {
    .srcSubresource = {
      .aspectMask = aspect,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .srcOffsets = {{}, {src->extent.width, src->extent.height, 1.0f}},
    .dstSubresource = {
      .aspectMask = aspect,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    {{}, {dst->extent.width, dst->extent.height, 1.0f}},
  }; vkCmdBlitImage(cmd, src->handle, src->layout, dst->handle, dst->layout, 1, &blit, VK_FILTER_LINEAR);
};

void vk_cmd_clear_image(VkCommandBuffer cmd,
                        image_vk* target,
                        const VkImageSubresourceRange *range,
                        const VkClearColorValue *clear) {
  vkCmdClearColorImage(cmd,
                       target->handle,
                       target->layout,
                       clear,
                       1,
                       range);
}

void vk_cmd_begin_rendering(VkCommandBuffer cmd, framebuffer_vk* fb) {
  int width = 0;
  int height = 0;

  if(fb->color_attachment_count > 0)  {
    width = fb->color_attachments[0]->image.extent.width;
    height = fb->color_attachments[0]->image.extent.height;
  } else if(fb->depth_attachment) {
    width = fb->depth_attachment->image.extent.width;
    height = fb->depth_attachment->image.extent.height;
  } else {
  }

  for(uint32_t i = 0; i < fb->color_attachment_count; i++) {
    vk_cmd_transition_image(cmd,
                    &fb->color_attachments[i]->image,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_GENERAL);
  }

  if(fb->depth_attachment) {
    vk_cmd_transition_image(cmd, &fb->depth_attachment->image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }

  VkRenderingAttachmentInfo color_attachment_infos[fb->color_attachment_count] = {};

  for(uint32_t i = 0; i < fb->color_attachment_count; i++) {
    color_attachment_infos[i] = (VkRenderingAttachmentInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext = NULL,
            .imageView = fb->color_attachment_views[i],
            .imageLayout = fb->color_attachments[i]->image.layout,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .resolveImageView = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
  }

  VkRenderingAttachmentInfo depth_attachment_info = {
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .pNext = NULL,
          .imageView = fb->depth_attachment_view,
          .imageLayout = fb->depth_attachment ? fb->depth_attachment->image.layout : VK_IMAGE_LAYOUT_UNDEFINED,
          .resolveMode = VK_RESOLVE_MODE_NONE,
          .resolveImageView = VK_NULL_HANDLE,
          .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .clearValue = { .depthStencil = { 1.0f, 0 } }
  };

  VkRenderingInfo rendering_info = {
          .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .pNext = NULL,
          .flags = 0,
          .renderArea = {.offset = {0, 0}, .extent = {width, height}},
          .layerCount = 1,
          .viewMask = 0,
          .colorAttachmentCount = fb->color_attachment_count,
          .pColorAttachments = color_attachment_infos,
          .pDepthAttachment = &depth_attachment_info,
          .pStencilAttachment = NULL,
  }; vk_cmd_begin_rendering_khr(cmd, &rendering_info);
}

void vk_cmd_end_rendering(VkCommandBuffer cmd) {
	vk_cmd_end_rendering_khr(cmd);
}
