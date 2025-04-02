#ifndef MGFX_DEFINES_H_
#define MGFX_DEFINES_H_

#include <mx/mx.h>

#define MGFX_SUCCESS ((uint16_t)0)

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

typedef struct MX_API mgfx_image_info {
    uint32_t format; // VkFormat

    uint32_t width;
    uint32_t height;

    uint32_t layers;

    mx_bool cube_map;
} mgfx_image_info;

enum { MGFX_SHADER_MAX_DESCRIPTOR_SET = 4 };
enum { MGFX_SHADER_MAX_DESCRIPTOR_BINDING = 8 };
enum { MGFX_SHADER_MAX_PUSH_CONSTANTS = 4 };
enum { K_SHADER_MAX_VERTEX_BINDINGS = 4 };
enum { K_SHADER_MAX_VERTEX_ATTRIBUTES = 16 };
typedef struct mgfx_descriptor {
} mgfx_descriptor;

#endif
