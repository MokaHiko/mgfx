#ifndef MXGUI_H_
#define MXGUI_H_

#include "mgfx/defines.h"
#include <mx/mx.h>
#include <mx/mx_log.h>
#include <mx/mx_string.h>
#include <vulkan/vulkan_core.h>

typedef struct MX_API mxgui_gui_settings {
    struct {
        uint32_t font;
        uint32_t font_size;
    } styles;

    int width;
    int height;
} mxgui_gui_settings;

typedef struct MX_API mxgui_window_info {
    struct {
        int flags;
    } styles;

} mxgui_window_info;

typedef int32_t mxgui_window_handle;
static const mxgui_window_handle MXGUI_INVALID_CONTAINER_HANDLE = -1;

typedef enum mxgui_container_properties {
    mxgui_CONTAINER_PROPERTY_RESIZABLE,
} mxgui_container_properties;

typedef struct MX_API mxgui_container {
    // local coordinates
    int x, y;
    int content_width;
    int content_height;

    uint32_t children_count;

    mxgui_window_handle parent;
} mxgui_container;

typedef struct MX_API mxgui_image_info {
    int width;
    int height;
} mxgui_image_info;

typedef enum MX_API mxgui_draw_command_type {
    MXGUI_DRAW_TYPE_TEXT,
    MXGUI_DRAW_TYPE_IMAGE,

    MXGUI_DRAW_TYPE_MAX = 0xFF
} mxgui_draw_command_type;

typedef struct MX_API mxgui_draw_command {
    struct {
        int x, y; // local coordinates
        int width;
        int height;
    } rect;

    union {
        struct {
            mx_ptr_t handle;
        } image;
    };

    mxgui_window_handle container;
    mxgui_draw_command_type type;
} mxgui_draw_command;

#define MXGUI_IMPLEMENTATION
#ifdef MXGUI_IMPLEMENTATION

#include <mgfx/mgfx.h>
#include <mx/mx_darray.h>
#include <mx/mx_math_mtx.h>

#include <stdlib.h>

typedef struct mxgui_vertex {
    mx_vec3 position;
    mx_vec2 uv;
} mxgui_vertex;
mgfx_vertex_layout mxgui_vl;

static mx_darray_t(mxgui_container) mxgui_containers;
static mxgui_window_handle mxgui_top = -1;

static mx_darray_t(mxgui_draw_command) mxgui_draws;
static mx_darray_t(mxgui_vertex) mxgui_vertices;
static mx_darray_t(uint32_t) mxgui_indices;

static mgfx_sh mxgui_vsh;
static mgfx_sh mxgui_fsh;
static mgfx_ph mxgui_ph;

// Config
static mxgui_gui_settings mxgui_settings;
static real_t mxgui_invw;
static real_t mxgui_invh;

MX_API static inline void mxgui_init(const mxgui_gui_settings* settings) {
    if (settings->width <= 0 || settings->height <= 0) {
        MX_LOG_ERROR("Cannot initialize mxgui with screen dimensions of %d %d",
                     settings->width,
                     settings->height);
        return;
    }

    mxgui_invw = 1.0 / settings->width;
    mxgui_invh = 1.0 / settings->height;

    mxgui_settings = *settings;

    mxgui_vsh = mgfx_shader_create("mxgui.vert.spv");
    mxgui_fsh = mgfx_shader_create("mxgui.frag.spv");
    mxgui_ph =
        mgfx_program_create_graphics(mxgui_vsh,
                                     mxgui_fsh,
                                     {.name = "mxgui", .cull_mode = MGFX_CULL_FRONT});

    mxgui_containers = MX_DARRAY_CREATE(mxgui_container, MX_DEFAULT_ALLOCATOR);
    mxgui_draws = MX_DARRAY_CREATE(mxgui_draw_command, MX_DEFAULT_ALLOCATOR);
    mxgui_vertices = MX_DARRAY_CREATE(mxgui_vertex, MX_DEFAULT_ALLOCATOR);
    mxgui_indices = MX_DARRAY_CREATE(uint32_t, MX_DEFAULT_ALLOCATOR);

    mgfx_vertex_layout_begin(&mxgui_vl);
    mgfx_vertex_layout_add(&mxgui_vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(mx_vec3));
    mgfx_vertex_layout_add(&mxgui_vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD0, sizeof(mx_vec2));
    mgfx_vertex_layout_end(&mxgui_vl);
};

static inline void mxgui_begin(const char* name) {
    mx_darray_push(&mxgui_containers, mxgui_container, {.parent = mxgui_top});
    ++mxgui_top;
};

static inline void mxgui_text(const char* fmt, ...) {
    mxgui_containers[mxgui_top].content_width += 100;
    mxgui_containers[mxgui_top].content_height += 100;

    mxgui_draw_command cmd = {
        .type = MXGUI_DRAW_TYPE_TEXT,
        .rect = {.width = 32, .height = 32},
    };
}

static inline void mxgui_image(mx_ptr_t image_handle, const mxgui_image_info* info) {
    mx_darray_push(&mxgui_draws,
                   mxgui_draw_command,
                   {.type = MXGUI_DRAW_TYPE_IMAGE,
                    .container = mxgui_top,
                    .rect =
                        {
                            .width = info->width,
                            .height = info->height,
                        },
                    .image = {.handle = image_handle}});
};

static inline void mxgui_end() {
    MX_DARRAY_REMOVE_AT(&mxgui_containers, mxgui_top);
    --mxgui_top;
};

MX_API MX_NO_DISCARD static uint32_t
mxgui_get_draw_commands(const mxgui_draw_command** out) {
    if (mxgui_top < MXGUI_INVALID_CONTAINER_HANDLE) {
        MX_LOG_WARN("[mxgui] : Missing begin.");
        return 0;
    }

    if (mxgui_top > MXGUI_INVALID_CONTAINER_HANDLE) {
        MX_LOG_WARN("[mxgui] : Missing end.");
        return 0;
    }

    mxgui_top = MXGUI_INVALID_CONTAINER_HANDLE;

    *out = mxgui_draws;
    return MX_DARRAY_COUNT(&mxgui_draws);
}

static int mxgui_sort_draws(const void* a, const void* b) {
    const mxgui_draw_command* draw_a = (mxgui_draw_command*)a;
    const mxgui_draw_command* draw_b = (mxgui_draw_command*)b;

    return draw_a->container - draw_b->container;
}

MX_API static inline void mxgui_render() {
    const mxgui_draw_command* commands;
    uint32_t draw_count = mxgui_get_draw_commands(&commands);

    if (draw_count < 0) {
        return;
    }

    const real_t right = (real_t)mxgui_settings.width;
    const real_t top = (real_t)mxgui_settings.height;

    mx_mat4 mxgui_proj =
        mx_ortho(0.0f, mxgui_settings.width, -mxgui_settings.height, 0.0f, -1.0f, 1.0f);
    mgfx_set_proj(mxgui_proj.val);
    mgfx_set_view(MX_MAT4_IDENTITY.val);
    mgfx_set_transform(MX_MAT4_IDENTITY.val);

    // Sort by hierarchy
    qsort(mxgui_draws, draw_count, sizeof(mxgui_draw_command), mxgui_sort_draws);

    mxgui_container* parent = NULL;
    for (uint32_t i = 0; i < draw_count; i++) {
        if (parent != &mxgui_containers[mxgui_draws[i].container]) {
            parent = &mxgui_containers[mxgui_draws[i].container];
        }

        // Resize to fit
        parent->content_width += mxgui_draws[i].rect.width;
        parent->content_height += mxgui_draws[i].rect.height;

        parent->children_count++;
    };

    parent = NULL;

    mx_vec3 offset = {0, 0, 0};
    uint32_t child_idx = 0;
    for (uint32_t i = 0; i < draw_count; i++) {
        if (parent != &mxgui_containers[mxgui_draws[i].container]) {
            parent = &mxgui_containers[mxgui_draws[i].container];
            child_idx = 0;
        }

        // Calculate chained transforms

        // Size to fit

        // Container in pixels
        float child_width = ((float)parent->content_width / parent->children_count);
        mx_vec3 scale = {mxgui_draws[i].rect.width, mxgui_draws[i].rect.height, 1.0f};

        // offset = (mx_vec3){parent->x, parent->y, 0};
        offset = (mx_vec3){0, 0, 0};
        offset.x += child_width * child_idx;
        child_idx++;

        mx_darray_clear(&mxgui_vertices);
        mx_darray_clear(&mxgui_indices);

        switch (mxgui_draws[i].type) {
        case MXGUI_DRAW_TYPE_TEXT:
        case MXGUI_DRAW_TYPE_IMAGE: {
            // NDC ALLIGNED TOP LEFT 0: Bottom-left
            mx_darray_push(&mxgui_vertices,
                           mxgui_vertex,
                           {.position = {0.0f, -1.0f, 1.0f}, .uv = {0.0f, 1.0f}});

            // NDC ALLIGNED TOP LEFT 1: Bottom-right
            mx_darray_push(&mxgui_vertices,
                           mxgui_vertex,
                           {.position = {1.0f, -1.0f, 1.0f}, .uv = {1.0f, 1.0f}});

            // NDC ALLIGNED TOP LEFT 2: Top-right
            mx_darray_push(&mxgui_vertices,
                           mxgui_vertex,
                           {.position = {1.0f, 0.0f, 1.0f}, .uv = {1.0f, 0.0f}});

            // NDC ALLIGNED TOP LEFT 3: Top-left
            mx_darray_push(&mxgui_vertices,
                           mxgui_vertex,
                           {.position = {0.0f, 0.0f, 1.0f}, .uv = {0.0f, 0.0f}});

            for (size_t vertidx = 0; vertidx < MX_DARRAY_COUNT(&mxgui_vertices);
                 vertidx++) {
                mxgui_vertices[vertidx].position =
                    mx_vec3_mul(mxgui_vertices[vertidx].position, scale);

                mxgui_vertices[vertidx].position =
                    mx_vec3_add(mxgui_vertices[vertidx].position, offset);
            }

            mgfx_transient_vb tvb = {.vl = &mxgui_vl};
            mgfx_transient_vertex_buffer_allocate(mxgui_vertices,
                                                  MX_DARRAY_COUNT(&mxgui_vertices) *
                                                      sizeof(mxgui_vertex),
                                                  &tvb);

            mgfx_transient_buffer tib = {0};

            mx_darray_push(&mxgui_indices, uint32_t, {0});
            mx_darray_push(&mxgui_indices, uint32_t, {3});
            mx_darray_push(&mxgui_indices, uint32_t, {2});
            mx_darray_push(&mxgui_indices, uint32_t, {2});
            mx_darray_push(&mxgui_indices, uint32_t, {1});
            mx_darray_push(&mxgui_indices, uint32_t, {0});

            mgfx_transient_index_buffer_allocate(mxgui_indices,
                                                 MX_DARRAY_COUNT(&mxgui_indices) *
                                                     sizeof(uint32_t),
                                                 &tib);

            mgfx_bind_transient_vertex_buffer(tvb);
            mgfx_bind_transient_index_buffer(tib);
            mgfx_bind_descriptor((mgfx_dh){mxgui_draws[i].image.handle});
            mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, mxgui_ph);
        } break;
        case MXGUI_DRAW_TYPE_MAX:
            break;
        }
    }

    mx_darray_clear(&mxgui_draws);
}

static inline void mxgui_shutdown() {
    mgfx_shader_destroy(mxgui_vsh);
    mgfx_shader_destroy(mxgui_fsh);
    mgfx_program_destroy(mxgui_ph);

    MX_DARRAY_DESTROY(&mxgui_containers, MX_DEFAULT_ALLOCATOR);
}

#endif

#endif
