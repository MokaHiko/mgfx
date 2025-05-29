#ifndef MGFX_DEFINES_H_
#define MGFX_DEFINES_H_

#include "mx/mx_string.h"
#include <mx/mx.h>

#define MGFX_SUCCESS ((uint16_t)0)

#ifdef __cplusplus
extern "C" { // Ensure C++ linkage compatibility
#endif

enum { MGFX_DEFAULT_VIEW_TARGET = 0xFF - 1 };

enum MGFX_SHADER_CONSTANTS {
    MGFX_SHADER_MAX_DESCRIPTOR_SET = 4,
    MGFX_SHADER_MAX_DESCRIPTOR_BINDING = 16,
    MGFX_SHADER_MAX_PUSH_CONSTANTS = 4,
};

typedef enum MGFX_SHADER_STAGE {
    MGFX_SHADER_STAGE_VERTEX = 0,
    MGFX_SHADER_STAGE_FRAGMENT,
    MGFX_SHADER_STAGE_COMPUTE,

    MGFX_SHADER_STAGE_COUNT
} MGFX_SHADER_STAGE;

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

typedef struct MX_API mgfx_graphics_ex_create_info {
    mx_str name;

    mgfx_primitive_topology primitive_topology;
    mgfx_polygon_mode polygon_mode;
    mgfx_cull_mode cull_mode;

    mx_bool instanced;

    const mgfx_vertex_layout* vl;
} mgfx_graphics_ex_create_info;

#ifdef __cplusplus
} // End extern "C"
#endif

#endif
