#ifndef MGFX_H_
#define MGFX_H_

#include "defines.h"
#include <mx/mx.h>
#include <vulkan/vulkan_core.h>

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
MX_API int mgfx_init(const mgfx_init_info* info);

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
 * @brief Shader handle
 * @note VkShaderModule
 * */
MGFX_HANDLE(mgfx_sh)

/**
 * @brief Program handle.
 * @note Contains structure used to create a VkPipeline.
 * */
MGFX_HANDLE(mgfx_ph)

/**
 * @@brief Handle for Vertex buffer
 * @note Equivalent to VKBuffer.
 */
MGFX_HANDLE(mgfx_vbh)

/**
 * @@brief Handle for Index buffer
 * @note Equivalent to VKBuffer.
 */
MGFX_HANDLE(mgfx_ibh)

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

void vertex_layout_begin(mgfx_vertex_layout* vl);

void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size);

void vertex_layout_end(mgfx_vertex_layout* vl);

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
    } graphics_pc;

    mgfx_vbh vbhs[4];
    uint32_t vbh_count;

    mgfx_ibh ibh;
    uint32_t index_count;

    mgfx_ph ph;

    uint32_t hash;
} mgfx_draw;

[[nodiscard]] mgfx_vbh mgfx_vertex_buffer_create(void* data, size_t len);

[[nodiscard]] mgfx_ibh mgfx_index_buffer_create(void* data, size_t len);

void mgfx_buffer_destroy(uint64_t buffer_idx);

[[nodiscard]] mgfx_sh mgfx_shader_create(const char* path);
void mgfx_shader_destroy(mgfx_sh sh);

[[nodiscard]] mgfx_ph mgfx_program_create_compute(mgfx_sh csh);
[[nodiscard]] mgfx_ph mgfx_program_create_graphics(mgfx_sh vsh, mgfx_sh fsh);
void mgfx_program_destroy(mgfx_ph ph);

[[nodiscard]] mgfx_imgh mgfx_image_create(const mgfx_image_info* info, uint32_t usage);
void mgfx_image_destroy(mgfx_imgh imgh);

mgfx_th mgfx_texture_create(const mgfx_image_info* info, const uint32_t filter, uint32_t usage);
void mgfx_texture_destroy(mgfx_th th, mx_bool release_image);

[[nodiscard]] mgfx_fbh mgfx_framebuffer_create(mgfx_imgh* color_attachments,
                                               uint32_t color_attachment_count,
                                               mgfx_imgh depth_attachment);

[[nodiscard]] mgfx_dh mgfx_descriptor_create(const char* name, uint32_t type);

void mgfx_set_texture(mgfx_dh dh, mgfx_th th);

void mgfx_descriptor_destroy(mgfx_dh dh);

void mgfx_bind_vertex_buffer(mgfx_vbh vbh);

void mgfx_bind_index_buffer(mgfx_ibh ibh);

void mgfx_bind_descriptor(uint32_t descriptor_set, mgfx_dh dh);

void mgfx_set_transform(float* mtx);

void mgfx_set_view(float* mtx);

void mgfx_set_proj(float* mtx);

void mgfx_submit(uint32_t target, mgfx_ph ph);

#endif
