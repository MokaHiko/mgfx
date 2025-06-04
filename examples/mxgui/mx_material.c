#include "mx_material.h"
#include "mgfx/defines.h"
#include "mgfx/mgfx.h"

#include <mx/mx_memory.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

mx_texture* texture_create(const void* data, const mgfx_image_info* info) {
    mx_texture* out = mx_alloc(MX_DEFAULT_ALLOCATOR, sizeof(mx_material));

    size_t size = info->width * info->height * mgfx_format_size(info->format);
    out->th = mgfx_texture_create_from_memory(info, MGFX_FILTER_LINEAR, data, size);
    out->uth = mgfx_descriptor_create("texture_sampler",
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    return out;
}

void mx_texture_destroy(mx_texture* texture) {
    mgfx_texture_destroy(texture->th, MX_FALSE);
    mgfx_descriptor_destroy(texture->uth);
    mx_free(MX_DEFAULT_ALLOCATOR, texture);
}

mx_material* mx_material_create(const mx_material_create_info* info) {
    mx_material* out = mx_alloc(MX_DEFAULT_ALLOCATOR, sizeof(mx_material));

    memcpy(out->textures, info->textures, info->texture_count * sizeof(mx_texture));

    return out;
}

void mx_material_destroy(mx_material* material) {
    mx_free(MX_DEFAULT_ALLOCATOR, material);
}
