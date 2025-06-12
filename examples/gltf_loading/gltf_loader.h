#ifndef MGFX_EXAMPLE_GLTF_H_
#define MGFX_EXAMPLE_GLTF_H_

#include <mgfx/mgfx.h>
#include <mx/mx.h>
#include <mx/mx_math_mtx.h>
#include <mx/mx_math_types.h>
#include <mx/mx_memory.h>

typedef enum gltf_material_flags {
    mgfx_material_flag_casts_none = 0,

    mgfx_material_flag_casts_shadows = 1 << 0,
    mgfx_material_flag_receive_shadows = 1 << 1,

    mgfx_material_flag_instanced = 1 << 2,
    mgfx_material_flag_max_enum = 0xFF,
} gltf_material_flags;

typedef struct gltf_material {
    struct properties {
        mx_vec3 albedo;
        float metallic;

        float roughness;
        float ao;
        float normal_factor;
        float padding;

        mx_vec3 emissive;
        float emissive_strength;
    } properties;

    mgfx_ubh properties_buffer;
    mgfx_uh u_properties_buffer;

    mgfx_th albedo_texture;
    mgfx_uh u_albedo_texture;

    mgfx_th metallic_roughness_texture;
    mgfx_uh u_metallic_roughness_texture;

    mgfx_th normal_texture;
    mgfx_uh u_normal_texture;

    mgfx_th occlusion_texture;
    mgfx_uh u_occlusion_texture;

    mgfx_th emissive_texture;
    mgfx_uh u_emissive_texture;

    gltf_material_flags flags;
} gltf_material;

enum { GLTF_MESH_MAX_PRIMITIVES = 128 };
typedef struct gltf_mesh {
    struct primitive {
        mgfx_vbh vbh;
        mgfx_ibh ibh;

        uint8_t* vertices;
        uint32_t vertex_count;

        uint32_t* indices;
        uint32_t index_count;

        const gltf_material* material;
        const mgfx_vertex_layout* vl;
    } primitives[GLTF_MESH_MAX_PRIMITIVES];

    uint32_t primitive_count;
} gltf_mesh;

enum { MGFX_NODE_MAX_PRIMITIVES = 32 };
typedef struct mgfx_node {
    // global matrix
    const gltf_mesh* mesh;
    mx_vec3 position;
    mx_quat rotation;
    mx_vec3 scale;
} mgfx_node;

enum { MGFX_SCENE_MAX_NODES = 128 };
enum { MGFX_SCENE_MAX_ROOTS = 32 };
enum { MGFX_SCENE_MAX_MATERIALS = 128 };
enum { MGFX_SCENE_MAX_TEXTURES = MGFX_SCENE_MAX_MATERIALS * 4 };
enum { MGFX_SCENE_MAX_MESHES = 128 };

typedef struct gltf {
    mgfx_node roots[MGFX_SCENE_MAX_ROOTS];
    uint32_t root_count;

    mgfx_node nodes[MGFX_SCENE_MAX_NODES];
    uint32_t node_count;

    mgfx_th textures[MGFX_SCENE_MAX_TEXTURES];
    uint32_t texture_count;

    gltf_material materials[MGFX_SCENE_MAX_MATERIALS];
    uint32_t material_count;

    gltf_mesh meshes[MGFX_SCENE_MAX_MESHES];
    uint32_t mesh_count;

    const mgfx_vertex_layout* vl;
} gltf;

typedef enum gltf_load_flags {
    gltf_loader_flag_textures = 1 << 0,
    gltf_loader_flag_materials = 1 << 1,
    gltf_loader_flag_meshes = 1 << 2,

    gltf_loader_flag_default =
        gltf_loader_flag_textures | gltf_loader_flag_materials | gltf_loader_flag_meshes,

    gltf_loader_flag_flip_winding = 1 << 3,

    gltf_loader_flag_max_enum = 0xFF
} gltf_load_flags;

void gltf_create_from_path(const char* path, gltf_load_flags flags, gltf* scene);
void gltf_destroy(gltf* scene);

#define LOAD_GLTF_MODEL(model_name, gltf_flags, gltf_ptr)                                \
    gltf_create_from_path(GLTF_MODELS_PATH model_name "/glTF/" model_name ".gltf",       \
                          gltf_flags,                                                    \
                          gltf_ptr)

#endif
