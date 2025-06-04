#include "asset.h"

#include <mx/mx_hash.h>
#include <mx/mx_string.h>

mx_map_t(uint32_t, mx_asset, 100) scenes;

mx_asset mx_asset_find(const char* path, const asset_load_options* options) {
    mx_str path_string = {path};

    uint32_t id = 0;
    mx_murmur_hash_32(path, mx_strlen(path_string), 0, &id);

    switch (options->type) {
    case MX_ASSET_TYPE_MESH:
    case MX_ASSET_TYPE_TEXTURE:
        break;
    }

    return (mx_asset){
        .handle = id,
        .type = options->type,
    };
}
