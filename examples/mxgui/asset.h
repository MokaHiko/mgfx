#ifndef MX_ASSET_H_
#define MX_ASSET_H_

#include <mx/mx.h>
#include <mx/mx_string.h>

typedef struct MX_API mx_asset {
    char name[256];
    uint32_t id;

    uint32_t type;

    uint32_t ref_count;
    void* raw;
} mx_asset;

typedef struct MX_API asset_load_options {
    uint32_t type;
    mx_bool preload;
} asset_load_options;

MX_API MX_NO_DISCARD mx_asset mx_asset_find(const char* path,
                                            const asset_load_options* options);

MX_API MX_NO_DISCARD mx_asset* mx_asset_create(const char* path,
                                               const asset_load_options* options);

MX_API void mx_asset_add_ref(mx_asset* asset);

// @returns true if asset reference count;
MX_API uint32_t mx_asset_release(mx_asset* asset);

#endif
