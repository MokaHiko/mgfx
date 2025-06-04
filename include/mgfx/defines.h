#ifndef MGFX_DEFINES_H_
#define MGFX_DEFINES_H_

#include <mx/mx.h>
#include <mx/mx_string.h>

#define MGFX_SUCCESS ((uint16_t)0)

#ifdef __cplusplus
extern "C" { // Ensure C++ linkage compatibility
#endif

typedef enum mgfx_error_code {
    MGFX_WARN_SWAPCHAIN_RESIZE,
    MGFX_ERROR_MISTMATCHED_UNIFORMS,
} mgfx_error_code;

typedef enum mgfx_format {
    MGFX_FORMAT_D32_SFLOAT = 126,
    MGFX_FORMAT_R16G16B16A16_SFLOAT = 97,
    MGFX_FORMAT_R8G8B8A8_SRGB = 43,
} mgfx_format;

typedef enum mgfx_filter_type {
    MGFX_FILTER_NEAREST = 0,
    MGFX_FILTER_LINEAR = 1,
    MGFX_FILTER_CUBIC_EXT = 1000015000,
    MGFX_FILTER_CUBIC_IMG = MGFX_FILTER_CUBIC_EXT,
    MGFX_FILTER_MAX_ENUM = 0x7FFFFFFF
} mgfx_filter_type;

enum { MGFX_DEFAULT_VIEW_TARGET = 0xFF - 1 };

enum MGFX_SHADER_CONSTANTS {
    MGFX_SHADER_MAX_DESCRIPTOR_SET = 1,
    MGFX_SHADER_MAX_DESCRIPTOR_BINDING = 16,
    MGFX_SHADER_MAX_PUSH_CONSTANTS = 4,
};

typedef enum mgfx_shader_stage {
    MGFX_SHADER_STAGE_VERTEX = 0,
    MGFX_SHADER_STAGE_FRAGMENT,
    MGFX_SHADER_STAGE_COMPUTE,

    MGFX_SHADER_STAGE_COUNT
} mgfx_shader_stage;

typedef enum mgfx_uniform_type {
    MGFX_UNIFORM_TYPE_SAMPLER = 0,
    MGFX_UNIFORM_TYPE_COMBINED_IMAGE_SAMPLER = 1,
    MGFX_UNIFORM_TYPE_SAMPLED_IMAGE = 2,
    MGFX_UNIFORM_TYPE_STORAGE_IMAGE = 3,
    MGFX_UNIFORM_TYPE_UNIFORM_TEXEL_BUFFER = 4,
    MGFX_UNIFORM_TYPE_STORAGE_TEXEL_BUFFER = 5,
    MGFX_UNIFORM_TYPE_UNIFORM_BUFFER = 6,
    MGFX_UNIFORM_TYPE_STORAGE_BUFFER = 7,

    MGFX_UNIFORM_TYPE_REF, // Indicates uniform buffer not owned by the resouce.
} mgfx_uniform_type;

typedef enum mgfx_vertex_attribute {
    MGFX_VERTEX_ATTRIBUTE_POSITION,
    MGFX_VERTEX_ATTRIBUTE_NORMAL,
    MGFX_VERTEX_ATTRIBUTE_TEXCOORD0,
    MGFX_VERTEX_ATTRIBUTE_COLOR,
    MGFX_VERTEX_ATTRIBUTE_TANGENT,
    MGFX_VERTEX_ATTRIBUTE_JOINTS,
    MGFX_VERTEX_ATTRIBUTE_WEIGHTS,
    MGFX_VERTEX_ATTRIBUTE_CUSTOM,

    MGFX_VERTEX_ATTRIBUTE_COUNT,
    MGFX_VERTEX_ATTRIBUTE_MAX = 0xFFFFFFFF
} mgfx_vertex_attribute;

typedef struct mgx_vertex_layout {
    // Offsets and sizes by attribute
    size_t offsets[MGFX_VERTEX_ATTRIBUTE_COUNT];
    size_t sizes[MGFX_VERTEX_ATTRIBUTE_COUNT];

    // Flat attributes given in order
    mgfx_vertex_attribute attributes[MGFX_VERTEX_ATTRIBUTE_COUNT];
    uint32_t attribute_count;

    size_t stride;
} mgfx_vertex_layout;

void mgfx_vertex_layout_begin(mgfx_vertex_layout* vl);

void mgfx_vertex_layout_add(mgfx_vertex_layout* vl,
                            mgfx_vertex_attribute attribute,
                            size_t size);

void mgfx_vertex_layout_end(mgfx_vertex_layout* vl);

typedef MX_API struct mgfx_image_info {
    mx_strv name;
    uint32_t format; // VkFormat

    uint32_t width;
    uint32_t height;

    uint32_t layers;

    mx_bool cube_map;
} mgfx_image_info;

typedef struct MX_API mgfx_transient_buffer {
    uint32_t requested_size; // Size requested on allocation
    uint32_t actual_size;    // Allocation size that may include padding
    uint32_t offset;
    mx_ptr_t buffer_handle; // VkBuffer
} mgfx_transient_buffer;

typedef struct MX_API mgfx_transient_vb {
    mgfx_transient_buffer tb;
    const mgfx_vertex_layout* vl;
} mgfx_transient_vb;

typedef enum mgfx_polygon_mode {
    MGFX_FILL = 0,
    MGFX_LINE,
} mgfx_polygon_mode;

typedef enum mgfx_primitive_topology {
    MGFX_TRIANGLE_LIST = 0,
} mgfx_primitive_topology;

// One to one with Vulkan enums
typedef enum mgfx_cull_mode {
    MGFX_CULL_NONE = 0,
    MGFX_CULL_FRONT = 0x00000001,
    MGFX_CULL_BACK = 0x00000002,
    MGFX_CULL_FRONT_AND_BACK = 0x00000003,
    MGFX_CULL_FLAGS_MAX_ENUM = 0x7FFFFFFF
} mgfx_cull_mode;

typedef struct MX_API mgfx_graphics_create_info {
    mx_str name;

    mgfx_primitive_topology primitive_topology;
    mgfx_polygon_mode polygon_mode;
    mgfx_cull_mode cull_mode;

    mx_bool instanced;
    mx_bool destroy_shaders;

    const mgfx_vertex_layout* vl;
} mgfx_graphics_create_info;

#ifdef __cplusplus
} // End extern "C"
#endif

#endif
