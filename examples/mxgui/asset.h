#ifndef MX_ASSET_H_
#define MX_ASSET_H_

#include <mx/mx.h>
#include <mx/mx_string.h>

typedef enum mx_asset_type {
    MX_ASSET_TYPE_MESH,
    MX_ASSET_TYPE_TEXTURE,
    MX_ASSET_TYPE_MATERIAL,

    MX_ASSET_TYPE_SCENE,
} mx_asset_type;

typedef struct MX_API mx_asset {
    char name[32];
    mx_str path;
    mx_asset_type type;

    uint32_t handle;
    void* raw;
} mx_asset;

#define MX_ASSET(type)                                                                   \
    char name[32];                                                                       \
    mx_str path;                                                                         \
    mx_asset_type type;

typedef struct MX_API asset_load_options {
    uint32_t type;
    mx_bool preload;
} asset_load_options;

MX_API MX_NO_DISCARD mx_asset mx_asset_find(const char* path,
                                            const asset_load_options* options);

#endif
