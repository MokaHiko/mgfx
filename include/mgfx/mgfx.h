#ifndef MGFX_H_
#define MGFX_H_

#include "defines.h"
#include <mx/mx.h>

#include <vulkan/vulkan_core.h>

typedef struct MX_API mgfx_built_in_vertex {
    float position[3];
    float uv_x;
    float normal[3];
    float uv_y;
    float color[4];
} mgfx_built_in_vertex;

// OpenGL-style NDC fullscreen quad (for blitting to screen)
//
//   ^ Y+
//   |
//   |    (-1,+1)           (+1,+1)
//   |     3 ┌──────────────┐ 2
//   |       │              │
//   |       │    Screen    │
//   |       │     Quad     │
//   |       │              │
//   |     0 └──────────────┘ 1
//   |   (-1,-1)           (+1,-1)
//   └────────────────────────────> X+

// Full screen quad
extern const mgfx_built_in_vertex MGFX_FS_QUAD_VERTICES[4];
extern const uint32_t MGFX_FS_QUAD_INDICES[6];

#ifdef __cplusplus
extern "C" { // Ensure C++ linkage compatibility
#endif

typedef MX_API struct {
    char name[256];
    void* nwh;
} mgfx_init_info;

/**
 * @brief initializes the mgfx graphics system.
 *
 * @param info pointer to an `mgfx_init_info` struct containing initialization parameters.
 * @return mgfx_success (0) if successful, or an error code otherwise.
 */
MX_API MX_NO_DISCARD int mgfx_init(const mgfx_init_info* info);

MX_API void mgfx_frame();

MX_API void mgfx_shutdown();

MX_API void mgfx_reset(uint32_t width, uint32_t height);

static const uint16_t mgfx_invalid_handle = UINT16_MAX;
#define MGFX_INVALID_HANDLE ((uint16_t)mgfx_invalid_handle)

#define MGFX_HANDLE(name)                                                                                    \
    typedef MX_API struct name {                                                                             \
        uint64_t idx;                                                                                        \
    } name;

/**
 * @brief Shader handle.
 * @note Equivalent to VkShaderModule.
 * */
MGFX_HANDLE(mgfx_sh)

/**
 * @brief Program handle.
 * @note Contains structure used to create a VkPipeline.
 * */
MGFX_HANDLE(mgfx_ph)

/**
 * @@brief Handle for Vertex buffer.
 * @note Equivalent to VKBuffer.
 */
MGFX_HANDLE(mgfx_vbh)

/**
 * @brief Handle for Index buffer.
 * @note Equivalent to VKBuffer. */
MGFX_HANDLE(mgfx_ibh)

/**
 * @@brief Handle for Unifrom buffer.
 * @note Equivalent to VkBuffer.
 */
MGFX_HANDLE(mgfx_ubh)

/**
 * @@brief Handle for Transient buffer.
 */
/*MGFX_HANDLE(mgfx_tbh)*/

/** @brief Descriptor handle */
MGFX_HANDLE(mgfx_dh)

/**
 * @brief Handle for an image resource
 * @details This handle represents an image in the MGFX system.
 * @note Equivalent to VkImage.
 */
MGFX_HANDLE(mgfx_imgh)

/**
 * @brief Handle for an framebuffer resource.
 */
MGFX_HANDLE(mgfx_fbh)

/** @brief Texture handle */

/**
 * @brief Handle for an Texture resource
 * @note Equivalent to VkImageView.
 */
MGFX_HANDLE(mgfx_th)

// Built in textures
extern mgfx_th MGFX_WHITE_TEXTURE;
extern mgfx_th MGFX_BLACK_TEXTURE;
extern mgfx_th MGFX_LOCAL_NORMAL_TEXTURE;

/**
 * @brief Renders debug text on the backbuffer.
 * @note Must be called on main draw loop.
 */
MX_API void mgfx_debug_draw_text(int32_t x, int32_t y, const char* fmt, ...);

MX_API MX_NO_DISCARD mgfx_vbh mgfx_vertex_buffer_create(const void* data, size_t len);

MX_API MX_NO_DISCARD mgfx_ibh mgfx_index_buffer_create(const void* data, size_t len);
MX_API MX_NO_DISCARD mgfx_ubh mgfx_uniform_buffer_create(const void* data, size_t len);

MX_API void mgfx_buffer_update(uint64_t buffer_idx, const void* data, size_t size, size_t offset);
MX_API void mgfx_buffer_destroy(uint64_t buffer_idx);

MX_API MX_NO_DISCARD mgfx_sh mgfx_shader_create(const char* path);
MX_API void mgfx_shader_destroy(mgfx_sh sh);

MX_API MX_NO_DISCARD mgfx_ph mgfx_program_create_compute(mgfx_sh csh);
MX_API MX_NO_DISCARD mgfx_ph mgfx_program_create_graphics_ex(mgfx_sh vsh,
                                                             mgfx_sh fsh,
                                                             const mgfx_graphics_ex_create_info* ex_info);
MX_API MX_NO_DISCARD mgfx_ph mgfx_program_create_graphics(mgfx_sh vsh, mgfx_sh fsh);
MX_API void mgfx_program_destroy(mgfx_ph ph);

MX_API MX_NO_DISCARD mgfx_imgh mgfx_image_create(const mgfx_image_info* info, uint32_t usage);
MX_API void mgfx_image_destroy(mgfx_imgh imgh);

MX_API MX_NO_DISCARD mgfx_th mgfx_texture_create_from_memory(const mgfx_image_info* info,
                                                             uint32_t filter,
                                                             void* data,
                                                             size_t len);
MX_API MX_NO_DISCARD mgfx_th mgfx_texture_create_from_image(mgfx_imgh img, const uint32_t filter);
MX_API void mgfx_texture_destroy(mgfx_th th, mx_bool release_image);

MX_API MX_NO_DISCARD mgfx_fbh mgfx_framebuffer_create(mgfx_imgh* color_attachments,
                                                      uint32_t color_attachment_count,
                                                      mgfx_imgh depth_attachment);
MX_API void mgfx_framebuffer_destroy(mgfx_fbh fbh);

MX_API MX_NO_DISCARD mgfx_dh mgfx_descriptor_create(const char* name, uint32_t type);
MX_API void mgfx_descriptor_destroy(mgfx_dh dh);

MX_API void mgfx_set_buffer(mgfx_dh dh, mgfx_ubh ubh);
MX_API void mgfx_set_texture(mgfx_dh dh, mgfx_th th);

MX_API void mgfx_set_view_clear(uint8_t target, float* color_4);
MX_API void mgfx_set_view_target(uint8_t target, mgfx_fbh fb);

MX_API void mgfx_set_transform(const float* mtx);
MX_API void mgfx_set_view(const float* mtx);
MX_API void mgfx_set_proj(const float* mtx);

MX_API void mgfx_bind_vertex_buffer(mgfx_vbh vbh);
MX_API void mgfx_bind_index_buffer(mgfx_ibh ibh);
MX_API void mgfx_bind_descriptor(uint32_t ds_idx, mgfx_dh dh);

MX_API void mgfx_submit(uint8_t target, mgfx_ph ph);

#ifdef __cplusplus
} // End extern "C"
#endif

#endif
