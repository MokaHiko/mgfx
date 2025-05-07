#ifndef MGFX_DEFINES_H_
#define MGFX_DEFINES_H_

#include <mx/mx.h>

#define MGFX_SUCCESS ((uint16_t)0)

#ifdef __cplusplus
extern "C" { // Ensure C++ linkage compatibility
#endif

// TODO: Remove
typedef enum mgfx_allocation_type {
    MGFX_ALLOCATION_TYPE_UNKNOWN = 0,

    MGFX_ALLOCATION_TYPE_PRIMARY = 1 << 0,
    MGFX_ALLOCATION_TYPE_POOL = 1 << 1,
    MGFX_ALLOCATION_TYPE_SLICE = 1 << 2,
    MGFX_ALLOCATION_TYPE_TRANSIENT = 1 << 3,
} mgfx_allocation_type;

enum { MGFX_SHADER_MAX_DESCRIPTOR_SET = 4 };
enum { MGFX_SHADER_MAX_DESCRIPTOR_BINDING = 8 };
enum { MGFX_SHADER_MAX_PUSH_CONSTANTS = 4 };
enum { MGFX_SHADER_MAX_VERTEX_BINDING = 4 };
enum { MGFX_SHADER_MAX_VERTEX_ATTRIBUTES = 16 };
typedef enum MGFX_SHADER_STAGE {
    MGFX_SHADER_STAGE_VERTEX = 0,
    MGFX_SHADER_STAGE_FRAGMENT,
    MGFX_SHADER_STAGE_COMPUTE,

    MGFX_SHADER_STAGE_COUNT
} MGFX_SHADER_STAGE;

typedef enum mgfx_vertex_attribute {
    MGFX_VERTEX_ATTRIBUTE_INVALID = 0,
    MGFX_VERTEX_ATTRIBUTE_POSITION,
    MGFX_VERTEX_ATTRIBUTE_NORMAL,
    MGFX_VERTEX_ATTRIBUTE_TANGENT,
    MGFX_VERTEX_ATTRIBUTE_TEXCOORD,
    MGFX_VERTEX_ATTRIBUTE_COLOR,
    MGFX_VERTEX_ATTRIBUTE_JOINTS,
    MGFX_VERTEX_ATTRIBUTE_WEIGHTS,
    MGFX_VERTEX_ATTRIBUTE_CUSTOM,

    MGFX_VERTEX_ATTRIBUTE_COUNT
} mgfx_vertex_attribute;

enum { MGFX_MAX_VERTEX_BINDINGS = 16 };
typedef struct mgx_vertex_layout {
    size_t attribute_sizes[MGFX_VERTEX_ATTRIBUTE_COUNT];
    size_t attribute_offsets[MGFX_VERTEX_ATTRIBUTE_COUNT];

    size_t stride;
} mgfx_vertex_layout;

void vertex_layout_begin(mgfx_vertex_layout* vl);

void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size);

void vertex_layout_end(mgfx_vertex_layout* vl);

typedef MX_API struct mgfx_image_info {
    uint32_t format; // VkFormat

    uint32_t width;
    uint32_t height;

    uint32_t layers;

    mx_bool cube_map;
} mgfx_image_info;

enum { MGFX_DEFAULT_VIEW_TARGET = 0xFF - 1};

typedef struct mgfx_transient_buffer {
    uint32_t size;
    uint32_t offset;
    mx_ptr_t buffer_handle; // VkBuffer
} mgfx_transient_buffer;

typedef struct mgfx_graphics_ex_create_info {
    int32_t primitive_topology; // VkPrimitiveTopology
    int32_t polygon_mode;       // VkPolygonMode
    int32_t cull_mode;          // VkCullModeFlags

    mx_bool instanced;
} mgfx_graphics_ex_create_info;

#ifdef __cplusplus
} // End extern "C"
#endif

#endif
