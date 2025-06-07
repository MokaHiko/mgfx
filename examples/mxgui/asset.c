#include "asset.h"

#include <mx/mx_hash.h>
#include <mx/mx_string.h>

#include <mx/mx_hash.h>

mx_map_t(uint32_t, mx_asset, 1000) s_assets;
mx_map_find_fn(asset_find, uint32_t, mx_asset);
mx_map_insert_fn(asset_insert, uint32_t, mx_asset);
mx_map_remove_fn(asset_remove, uint32_t, mx_asset);

mx_asset* mx_asset_create(const char* name, const asset_load_options* options) {
    mx_str name_str = {name};

    uint32_t id = 0;
    mx_murmur_hash_32(name, mx_strlen(name_str), 0, &id);

    return asset_insert(&s_assets,
                        id,
                        &(mx_asset){.id = id, .type = options->type, .ref_count = 1});
}

void mx_asset_add_ref(mx_asset* asset) { ++asset->ref_count; };

// @returns true if asset reference count;
uint32_t mx_asset_release(mx_asset* asset) {
    asset->ref_count--;
    asset_remove(&s_assets, asset->id);

    if (asset->ref_count < 0) {
        MX_LOG_INFO("[Texture] released more times than references!");
    }

    return asset->ref_count;
}

mx_asset mx_asset_find(const char* path, const asset_load_options* options) {
    mx_str path_string = {path};

    uint32_t id = 0;
    mx_murmur_hash_32(path, mx_strlen(path_string), 0, &id);

    return (mx_asset){
        .id = id,
        .type = options->type,
    };
}
