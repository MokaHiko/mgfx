#include "mgfx/mgfx.h"
#include "mgfx/defines.h"

#include "renderer_vk.h"
#include "vma/vk_mem_alloc.h"

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

#include <mx/mx_memory.h>

#include <string.h>

enum QueuesVk : uint16_t {
  MGFX_QUEUE_GRAPHICS = 0,
  MGFX_QUEUE_PRESENT,

  MGFX_QUEUE_COUNT,
};

const char* k_req_exts[] = {
  VK_KHR_SURFACE_EXTENSION_NAME,
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

const char* k_req_device_ext_names[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef MX_MACOS
  VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
};
const uint32_t k_req_device_ext_count = (uint32_t)(sizeof(k_req_device_ext_names) / sizeof(const char*));

const VkFormat k_surface_fmt = VK_FORMAT_B8G8R8A8_SRGB;
const VkColorSpaceKHR k_surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
const VkPresentModeKHR k_present_mode = VK_PRESENT_MODE_FIFO_KHR;

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

static VkDebugUtilsMessengerEXT s_debug_messenger;
#endif

static VkInstance s_instance = VK_NULL_HANDLE;
static VkPhysicalDevice s_phys_device = VK_NULL_HANDLE;
static VkDevice s_device = VK_NULL_HANDLE;
static VmaAllocator s_allocator;

static VkQueue s_queues[MGFX_QUEUE_COUNT];
static uint32_t s_queue_indices[MGFX_QUEUE_COUNT];

static VkSurfaceKHR s_surface = VK_NULL_HANDLE;
static VkSurfaceCapabilitiesKHR s_surface_caps;
static SwapchainVk s_swapchain;

enum {MGFX_MAX_FRAME_OVERLAP = 2};
static FrameVk s_frames[MGFX_MAX_FRAME_OVERLAP];
static uint32_t s_frame_idx = 0;

int swapchain_create(VkSurfaceKHR surface, uint32_t width, uint32_t height, SwapchainVk* swapchain, mx_arena* memory_arena) {
  mx_arena* local_arena = memory_arena;

  if(memory_arena == NULL) {
    mx_arena temp_arena = mx_arena_alloc(MX_KB);
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
  if(s_surface_caps.maxImageCount > 0 && image_count > s_surface_caps.maxImageCount) {
    image_count = s_surface_caps.maxImageCount;
  }
  swapchain_info.minImageCount = image_count;

  swapchain_info.imageFormat = k_surface_fmt;
  swapchain_info.imageColorSpace = k_surface_color_space;

  swapchain_info.imageExtent.width = swapchain->extent.width;
  swapchain_info.imageExtent.height = swapchain->extent.height;

  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if(s_queue_indices[MGFX_QUEUE_GRAPHICS] != s_queue_indices[MGFX_QUEUE_PRESENT]) {
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
  if(swapchain->image_count > K_MAX_SWAPCHAIN_IMAGES) {
    printf("Requested swapchain images larger than max capacity!");
    return -1;
  }

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

  sc_img_view_info.subresourceRange = (VkImageSubresourceRange) {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount= 1,
  };

  for(uint32_t i = 0; i < swapchain->image_count; i++) {
    swapchain->images[i].handle = images[i];
    swapchain->images[i].format = k_surface_fmt;
    swapchain->images[i].extent = (VkExtent3D) {width, height, 1.0f };
    swapchain->images[i].format = k_surface_fmt;
    sc_img_view_info.image = swapchain->images[i].handle;

    vkCreateImageView(s_device, &sc_img_view_info, NULL, &swapchain->image_views[i]);
  }

  if(memory_arena == NULL) {
    mx_arena_free(local_arena);
  }

  return MGFX_SUCCESS;
}

void swapchain_destroy(SwapchainVk* swapchain) {
  for(uint32_t i = 0; i < swapchain->image_count; i++) {
    vkDestroyImageView(s_device, swapchain->image_views[i], NULL);
  }

  vkDestroySwapchainKHR(s_device, swapchain->handle, NULL);
}

void shader_create(size_t length, const char* code, ShaderVk* shader) {
  if (length % sizeof(uint32_t) != 0) {
    printf("Shader code size must be a multiple of 4!\n");
    return;
  }

  VkShaderModuleCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.codeSize = length;
  info.pCode = (uint32_t*)(void*)code;

  VK_CHECK(vkCreateShaderModule(s_device, &info, NULL, &shader->module));
};

void shader_destroy(ShaderVk* shader) {
  vkDestroyShaderModule(s_device, shader->module, NULL);
}

void texture_create_2d(uint32_t width,
                    uint32_t height,
                    VkFormat format,
                    VkImageUsageFlags usage,
                    TextureVk* texture) {

  texture->image.format = format;
  texture->image.extent = (VkExtent3D) {width, height, 1};

  VkImageCreateInfo image_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = texture->image.format,
    .extent = texture->image.extent,
    .mipLevels = 1,
    .arrayLayers = 1,
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

  VK_CHECK(vmaCreateImage(s_allocator,
                 &image_info,
                 &alloc_info,
                 &texture->image.handle,
                 &texture->allocation,
                  NULL));
}

void texture_create_view_2d(const TextureVk* texture, const VkImageAspectFlags aspect, VkImageView* view) {
  VkImageViewCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .image = texture->image.handle,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = texture->image.format,
    .components = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
    },
    .subresourceRange = {
      .aspectMask = aspect,
      .baseMipLevel =0,
      .levelCount = 1,
      .baseArrayLayer =0 ,
      .layerCount = 1,
    },
  };

  VK_CHECK(vkCreateImageView(s_device, &info, NULL, view));
}

void texture_destroy(TextureVk *texture) {
  vmaDestroyImage(s_allocator, texture->image.handle, texture->allocation);
}

void program_create_compute(const ShaderVk* cs, ProgramVk* program) {
  VkComputePipelineCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = cs->module,
      .pName = "main",
      .pSpecializationInfo = NULL,
    }
    /*.layout = ,*/
    /*.basePipelineHandle,*/
    /*.basePipelineIndex,*/
  };
}

void program_create_graphics(const ShaderVk* vs, const ShaderVk* fs, ProgramVk* program) {
  VkGraphicsPipelineCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.stageCount = 2;

  VkPipelineShaderStageCreateInfo shader_stage_infos[2] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vs->module,
      .pName = "main",
      .pSpecializationInfo = NULL,
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fs->module,
      .pName = "main",
      .pSpecializationInfo = NULL,
    }
  }; info.pStages = shader_stage_infos;

  VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = NULL,
  }; info.pVertexInputState = &vertex_input_state_info;

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
  }; info.pTessellationState = &tesselation_state_info;

  VkPipelineViewportStateCreateInfo viewport_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = NULL, // Part of dynamic state.
    .scissorCount = 1,
    .pScissors = NULL,  // Part of dynamic state.
  }; info.pViewportState = &viewport_state_info;

  VkPipelineRasterizationStateCreateInfo rasterization_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_TRUE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f,
  }; info.pRasterizationState = &rasterization_state_info;

  VkPipelineMultisampleStateCreateInfo multisample_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .rasterizationSamples = 1,
    .sampleShadingEnable = VK_TRUE,
    .minSampleShading = 1.0,
    .pSampleMask = NULL,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable= VK_FALSE,
  }; info.pMultisampleState = &multisample_state_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info = {
    /*.sType;*/
    /*.pNext;*/
    /*.flags;*/
    /*.depthTestEnable;*/
    /*.depthWriteEnable;*/
    /*.depthCompareOp;*/
    /*.depthBoundsTestEnable;*/
    /*.stencilTestEnable;*/
    /*.front;*/
    /*.back;*/
    /*.minDepthBounds;*/
    /*.maxDepthBounds;*/
  }; info.pDepthStencilState = NULL;

  VkPipelineColorBlendAttachmentState color_attachment = {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                      VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT |
                      VK_COLOR_COMPONENT_A_BIT ,
  };

  VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .logicOpEnable = VK_TRUE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments = &color_attachment,
    /*.blendConstants[4], */  // Set to Zero
  }; info.pColorBlendState = &color_blend_state_info;

  const VkDynamicState k_dynamic_states[] = { 
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  }; const uint32_t k_dynamic_state_count = sizeof(k_dynamic_states) / sizeof(VkDynamicState);

  VkPipelineDynamicStateCreateInfo dynamic_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .dynamicStateCount = k_dynamic_state_count,
    .pDynamicStates = k_dynamic_states,
  }; info.pDynamicState = &dynamic_state_info;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .setLayoutCount = 0,
    .pSetLayouts = NULL,
    .pushConstantRangeCount = 0,
    .pPushConstantRanges= NULL,
  };
  VK_CHECK(vkCreatePipelineLayout(s_device, &pipeline_layout_info, NULL, &program->layout));
  info.layout = program->layout;

  // Dynamic Rendering.
  info.renderPass = NULL;
  VkPipelineRenderingCreateInfoKHR rendering_create_pipeline = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .pNext = NULL,
    .viewMask = 1,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &k_surface_fmt,
    /*.depthAttachmentFormat,*/
    /*.stencilAttachmentFormat,*/
  }; info.pNext = &rendering_create_pipeline;

  VK_CHECK(vkCreateGraphicsPipelines(s_device, NULL, 1, &info, NULL, &program->pipeline));
}

int mgfx_init(const MgfxInitInfo* info) {
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
#elif define(MX_WIN32)
  app_info.apiVersion = VK_API_VERSION_1_3;
#endif

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
  VkExtensionProperties* avail_props = mx_arena_push(&vk_init_arena, avail_prop_count * sizeof(VkExtensionProperties));
  VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &avail_prop_count, avail_props));

  int validated_ext_count = 0;
  for(uint32_t req_index = 0; req_index < k_req_ext_count; req_index++) {
    for(uint32_t avail_index = 0; avail_index < avail_prop_count; avail_index++) {
      if(strcmp(k_req_exts[req_index], avail_props[avail_index].extensionName) == 0) {
        validated_ext_count++;
        continue;
      }
    }
  }

  if(validated_ext_count != k_req_ext_count) {
    printf("[Error]: Extensions required not supported!\n");
    mx_arena_free(&vk_init_arena);
    return -1;
  }

  instance_info.enabledExtensionCount = k_req_ext_count;
  instance_info.ppEnabledExtensionNames = k_req_exts;

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
  if(choose_physical_device_vk(s_instance,
                           k_req_device_ext_count,
                           k_req_device_ext_names,
                           &s_phys_device, 
                           &vk_init_arena) != VK_SUCCESS) {
    printf("Failed to find suitable physical device!");
    mx_arena_free(&vk_init_arena);
    return -1;
  }
  VK_CHECK(get_window_surface_vk(s_instance, info->nwh, &s_surface));

  uint32_t queue_family_props_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device, &queue_family_props_count, NULL);
  VkQueueFamilyProperties* queue_family_props = mx_arena_push(&vk_init_arena,
                                                              sizeof(VkQueueFamilyProperties) * queue_family_props_count);

  vkGetPhysicalDeviceQueueFamilyProperties(s_phys_device, &queue_family_props_count, queue_family_props);

  int unique_queue_count = 0;
  for(uint32_t i = 0; i < queue_family_props_count; i++) {
    if((queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) {
      s_queue_indices[MGFX_QUEUE_GRAPHICS] = i;
    }

    VkBool32 present_supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(s_phys_device, i, s_surface, &present_supported);
    if(present_supported == VK_TRUE) {
      s_queue_indices[MGFX_QUEUE_PRESENT] = i;
    }

    ++unique_queue_count;

    // Check if complete.
    int complete = MGFX_SUCCESS;
    for(uint16_t j = 0; j < unique_queue_count; j++) {
      if(s_queue_indices[MGFX_QUEUE_PRESENT] == -1) {
        complete = !MGFX_SUCCESS;
      }
    }

    if(complete == MGFX_SUCCESS) {
        break;
    }
  }

  for(uint16_t i = 0; i < MGFX_QUEUE_COUNT; i++) {
    if(s_queue_indices[MGFX_QUEUE_PRESENT] == -1) {
      printf("Failed to find graphics queue!");
      mx_arena_free(&vk_init_arena);
      break;
    }
  }

  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_infos[MGFX_QUEUE_COUNT] = {};
  for(uint16_t i = 0; i < unique_queue_count; i++) {
    queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[i].pNext = NULL;
    queue_infos[i].flags = 0;
    queue_infos[i].queueFamilyIndex = s_queue_indices[i];
    queue_infos[i].queueCount = 1;
    queue_infos[i].pQueuePriorities = &queue_priority;
  }

  VkPhysicalDeviceFeatures phys_device_features = {};
  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .queueCreateInfoCount = unique_queue_count,
    .pQueueCreateInfos = queue_infos,
    .enabledExtensionCount = k_req_device_ext_count,
    .ppEnabledExtensionNames = k_req_device_ext_names,
    .pEnabledFeatures = &phys_device_features,
  }; VK_CHECK(vkCreateDevice(s_phys_device, &device_info, NULL, &s_device));

  for(uint16_t i = 0; i < unique_queue_count; i++) {
    vkGetDeviceQueue(s_device, s_queue_indices[i], 0, &s_queues[i]);
  }

  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys_device, s_surface, &s_surface_caps));

  uint32_t surface_fmt_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys_device, s_surface, &surface_fmt_count, NULL);
  VkSurfaceFormatKHR* surface_fmts = mx_arena_push(&vk_init_arena, surface_fmt_count * sizeof(VkSurfaceFormatKHR));
  vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys_device, s_surface, &surface_fmt_count, surface_fmts);

  int surface_format_found = -1;
  for(uint32_t i = 0; i < surface_fmt_count; i++) {
    if(surface_fmts[i].format == k_surface_fmt &&
      surface_fmts[i].colorSpace == k_surface_color_space) {
      surface_format_found = MGFX_SUCCESS;
      break;
    }
  }

  if(surface_format_found != MGFX_SUCCESS) {
    printf("Required surface format not found!\n");
    return -1;
  }

  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys_device, s_surface, &present_mode_count, NULL);
  VkPresentModeKHR* present_modes = mx_arena_push(&vk_init_arena, surface_fmt_count * sizeof(VkPresentModeKHR));
  vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys_device, s_surface, &present_mode_count, present_modes);
  for(uint32_t i = 0; i < present_mode_count; i++) {
  }

  VkExtent2D swapchain_extent;
  choose_swapchain_extent_vk(&s_surface_caps,
                          info->nwh,
                          &swapchain_extent);

  swapchain_create(s_surface,
                   swapchain_extent.width,
                   swapchain_extent.height,
                   &s_swapchain,
                   &vk_init_arena);

  VmaAllocatorCreateInfo allocator_info = {
    .instance = s_instance,
    .physicalDevice = s_phys_device,
    .device = s_device,
    /*.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,*/
  }; VK_CHECK(vmaCreateAllocator(&allocator_info, &s_allocator));

  for(int i = 0; i < MGFX_MAX_FRAME_OVERLAP; i++) {
    VkCommandPoolCreateInfo gfx_cmd_pool = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = s_queue_indices[MGFX_QUEUE_GRAPHICS],
    }; VK_CHECK(vkCreateCommandPool(s_device, &gfx_cmd_pool, NULL, &s_frames[i].cmd_pool));

    VkCommandBufferAllocateInfo buffer_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = NULL,
      .commandPool = s_frames[i].cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
    }; VK_CHECK(vkAllocateCommandBuffers(s_device, &buffer_alloc_info, &s_frames[i].cmd));

    VkSemaphoreCreateInfo swapchain_semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
    }; VK_CHECK(vkCreateSemaphore(s_device, &swapchain_semaphore_info, NULL, &s_frames[i].swapchain_semaphore));

    VkSemaphoreCreateInfo render_semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
    }; VK_CHECK(vkCreateSemaphore(s_device, &render_semaphore_info, NULL, &s_frames[i].render_semaphore));

    VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    }; VK_CHECK(vkCreateFence(s_device, &fence_info, NULL, &s_frames[i].render_fence));
  }

  mx_arena_free(&vk_init_arena);
  return MGFX_SUCCESS;
}

void mgfx_frame() {
  // Get current frame.
  const FrameVk* frame = &s_frames[s_frame_idx];

  // Wait and reset render fence of current frame idx.
  VK_CHECK(vkWaitForFences(s_device, 1, &frame->render_fence, VK_TRUE, UINT64_MAX));
  VK_CHECK(vkResetFences(s_device, 1, &frame->render_fence));

  // Wait for available next swapchain image.
  uint32_t sc_img_idx;
  VK_CHECK(vkAcquireNextImageKHR(s_device,
                        s_swapchain.handle,
                        UINT64_MAX,
                        frame->swapchain_semaphore,
                        NULL,
                        &sc_img_idx));

  vkResetCommandBuffer(frame->cmd, 0);
  VkCommandBufferBeginInfo cmd_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = NULL,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = NULL,
  }; VK_CHECK(vkBeginCommandBuffer(frame->cmd, &cmd_begin_info));

  vk_cmd_transition_image(frame->cmd,
                          &s_swapchain.images[sc_img_idx],
                          VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_GENERAL);

  DrawCtx ctx = {
    .cmd = frame->cmd,
    .frame_target = &s_swapchain.images[sc_img_idx],
  }; mgfx_example_updates(&ctx);

  vk_cmd_transition_image(frame->cmd,
                          &s_swapchain.images[sc_img_idx],
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
  }; VK_CHECK(vkQueueSubmit(s_queues[MGFX_QUEUE_GRAPHICS], 1, &submit_info, frame->render_fence));

  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = NULL,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &frame->render_semaphore,
    .swapchainCount = 1,
    .pSwapchains = &s_swapchain.handle,
    .pImageIndices = &sc_img_idx,
    .pResults = NULL,
  }; VK_CHECK(vkQueuePresentKHR(s_queues[MGFX_QUEUE_GRAPHICS], &present_info));

  s_frame_idx = (s_frame_idx + 1) % MGFX_MAX_FRAME_OVERLAP;
};
 
void mgfx_shutdown() {
  VK_CHECK(vkDeviceWaitIdle(s_device));

  for(int i = 0; i < MGFX_MAX_FRAME_OVERLAP; i++) {
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
