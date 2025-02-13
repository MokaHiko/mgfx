#include "mgfx/mgfx.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#ifdef MX_MACOS
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_metal.h>
#endif

#include <mx/mx_memory.h>

#include <stdio.h>
#include <string.h>

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

#ifdef MX_DEBUG
VkResult create_debug_util_messenger_ext(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void destroy_create_debug_util_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    printf("[VkValidationLayer]: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

VkDebugUtilsMessengerEXT s_debug_messenger;
#endif

VkInstance s_instance = VK_NULL_HANDLE;
VkPhysicalDevice s_phys_device = VK_NULL_HANDLE;
VkDevice s_device = VK_NULL_HANDLE;

uint32_t s_queue_family_index = -1;
VkQueue s_graphics_queue = VK_NULL_HANDLE;

VkSurfaceKHR s_surface = VK_NULL_HANDLE;

int mgfx_init(const mgfx_init_info* info) {
  mx_arena vk_init_arena = mx_arena_alloc(MX_MB);

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = NULL;
  app_info.pApplicationName = info->name; 
  app_info.applicationVersion = VK_MAKE_VERSION(0,0,1);
  app_info.pEngineName = "MGFX";
  app_info.engineVersion = VK_MAKE_VERSION(0,0,1);

#ifndef MGFX_MACOS
  app_info.apiVersion = VK_API_VERSION_1_3;
#else
  app_info.apiVersion = VK_API_VERSION_1_3;
#endif

  VkInstanceCreateInfo instance_info = {};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pNext = NULL;
  instance_info.pApplicationInfo = &app_info;

  const char* req_exts[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef MX_DEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
#ifdef MX_MACOS
    VK_EXT_METAL_SURFACE_EXTENSION_NAME,
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#endif
  };
  const int req_ext_count = sizeof(req_exts) / sizeof(const char*);

#ifdef MX_MACOS
  instance_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  // Check if required extensions are available.
  uint32_t avail_prop_count = 0;
  VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count, NULL));

  VkExtensionProperties* avail_props = mx_arena_push(&vk_init_arena, avail_prop_count * sizeof(VkExtensionProperties));
  VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count, avail_props));

  int validated_ext_count = 0;
  for(uint32_t req_index = 0; req_index < req_ext_count; req_index++) {
    for(uint32_t avail_index = 0; avail_index < avail_prop_count; avail_index++) {
      if(strcmp(req_exts[req_index], avail_props[avail_index].extensionName) == 0) {
        validated_ext_count++;
        continue;
      }
    }
  }

  if(validated_ext_count != req_ext_count) {
    printf("[Error]: Extensions required not supported!\n");
    mx_arena_free(&vk_init_arena);
    return -1;
  }

  instance_info.enabledExtensionCount = req_ext_count;
  instance_info.ppEnabledExtensionNames = req_exts;

#ifdef MX_DEBUG
  // Check and enable validation layers.
  uint32_t layer_prop_count;
  vkEnumerateInstanceLayerProperties(&layer_prop_count, NULL);
  VkLayerProperties* layer_props = mx_arena_push(&vk_init_arena, sizeof(VkLayerProperties) * layer_prop_count);
  vkEnumerateInstanceLayerProperties(&layer_prop_count, layer_props);

  const char* validationLayers = {
      "VK_LAYER_KHRONOS_validation"
  };

  for(uint32_t i = 0; i < layer_prop_count; i++)  {
    if(strcmp(validationLayers, layer_props[i].layerName) == 0) {
      printf("[Warn]: Validation layers enabled. \n");
      instance_info.enabledLayerCount = 1;
      instance_info.ppEnabledLayerNames = &validationLayers;
      break;
    }
  }
#endif

  VK_CHECK(vkCreateInstance(&instance_info, NULL, &s_instance));

#ifdef MX_DEBUG
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};
  debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_messenger_info.pfnUserCallback = vk_debug_callback;
  debug_messenger_info.pUserData = NULL;

  create_debug_util_messenger_ext(s_instance, &debug_messenger_info, NULL, &s_debug_messenger);
#endif

  uint32_t physical_device_count = 0;
  vkEnumeratePhysicalDevices(s_instance, &physical_device_count, NULL);
  VkPhysicalDevice* physical_devices = mx_arena_push(&vk_init_arena, sizeof(VkPhysicalDevice) * physical_device_count);
  vkEnumeratePhysicalDevices(s_instance, &physical_device_count, physical_devices);

  VkPhysicalDeviceProperties physical_device_props = {};
  VkPhysicalDeviceFeatures physical_device_features = {};

  if(physical_device_count < 0) {
    printf("Failed to find suitable physical device!");
    mx_arena_free(&vk_init_arena);
    return -1;
  }

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
    printf("\t\t]viewportBoundsRange: %f x %.f\n", physical_device_props.limits.viewportBoundsRange[0], physical_device_props.limits.viewportBoundsRange[1]);
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
  }

  s_phys_device = physical_devices[0];

  uint32_t queue_family_props_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device, &queue_family_props_count, NULL);
  VkQueueFamilyProperties* queue_family_props = mx_arena_push(&vk_init_arena,
                                                              sizeof(VkQueueFamilyProperties) * queue_family_props_count);

  vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device, &queue_family_props_count, queue_family_props);
  for(uint32_t i = 0; i < queue_family_props_count; i++) {
    if((queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) {
      s_queue_family_index = i;
      break;
    }
  }

  if(s_queue_family_index == -1) {
    printf("Failed to find graphics queue!");
    mx_arena_free(&vk_init_arena);
    return -1;
  }

  VkDeviceQueueCreateInfo queue_info = {};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.pNext = NULL;
  queue_info.flags = 0;
  queue_info.queueFamilyIndex = s_queue_family_index;
  queue_info.queueCount = 1;

  float queue_priority = 1.0f;
  queue_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = NULL;
  device_info.flags = 0;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;

  const char* const device_extension_names[] = {
#ifdef MX_MACOS
    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
  };
  device_info.enabledExtensionCount = sizeof(device_extension_names) / sizeof(const char*);
  device_info.ppEnabledExtensionNames = device_extension_names;

  VkPhysicalDeviceFeatures phys_device_features = {};
  device_info.pEnabledFeatures = &phys_device_features;

  VK_CHECK(vkCreateDevice(s_phys_device, &device_info, NULL, &s_device));
  vkGetDeviceQueue(s_device, s_queue_family_index, 0, &s_graphics_queue);

  //VK_CHECK(mgfx_create_surface_vk(s_instance, info->nwh, &s_surface));

  mx_arena_free(&vk_init_arena);
  return 0;
}

void mgfx_shutdown() {
#ifdef MX_DEBUG
  destroy_create_debug_util_messenger_ext(s_instance, s_debug_messenger, NULL);
#endif

  vkDestroyDevice(s_device, NULL);
  vkDestroyInstance(s_instance, NULL);
}
