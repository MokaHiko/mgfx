#ifndef MX_MATERIAL_H_
#define MX_MATERIAL_H_

#include <mgfx/mgfx.h>

#include <mx/mx.h>
#include <mx/mx_darray.h>

typedef struct mx_texture {
    mgfx_th th;
    mgfx_dh uth;
} mx_texture;

MX_API MX_NO_DISCARD mx_texture* texture_create(const void* data,
                                                const mgfx_image_info* info);

typedef enum mx_material_constants {
    MX_MATERIAL_MAX_TEXTURES = 16,
} mx_material_constants;

typedef struct mx_material {
    mgfx_ubh properties;
    mgfx_dh u_properties;

    mx_texture textures[MX_MATERIAL_MAX_TEXTURES];
    uint32_t texture_count;
} mx_material;

typedef struct mx_material_create_info {
    mx_texture* textures;
    int texture_count;

    int flags;
} mx_material_create_info;

MX_API MX_NO_DISCARD mx_material* mx_material_create(const mx_material_create_info* info);
void mx_material_destroy(mx_material* material);

#endif
