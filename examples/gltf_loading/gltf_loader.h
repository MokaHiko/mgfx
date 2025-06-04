#ifndef MGFX_EXAMPLE_GLTF_H_
#define MGFX_EXAMPLE_GLTF_H_

#include <mgfx/mgfx.h>
#include <mx/mx.h>
#include <mx/mx_math_mtx.h>
#include <mx/mx_math_types.h>
#include <mx/mx_memory.h>

typedef enum mgfx_material_flags {
    mgfx_material_flag_casts_none = 0,

    mgfx_material_flag_casts_shadows = 1 << 0,
    mgfx_material_flag_receive_shadows = 1 << 1,

    mgfx_material_flag_instanced = 1 << 2,
    mgfx_material_flag_max_enum = 0xFF,
} mgfx_material_flags;

typedef struct mgfx_material {
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
    mgfx_dh u_properties_buffer;

    mgfx_th albedo_texture;
    mgfx_dh u_albedo_texture;

    mgfx_th metallic_roughness_texture;
    mgfx_dh u_metallic_roughness_texture;

    mgfx_th normal_texture;
    mgfx_dh u_normal_texture;

    mgfx_th occlusion_texture;
    mgfx_dh u_occlusion_texture;

    mgfx_th emissive_texture;
    mgfx_dh u_emissive_texture;

    mgfx_material_flags flags;
} mgfx_material;

enum { MGFX_MESH_MAX_PRIMITIVES = 128 };
typedef struct mgfx_mesh {
    struct primitive {
        mgfx_vbh vbh;
        mgfx_ibh ibh;

        uint8_t* vertices;
        uint32_t vertex_count;

        uint32_t* indices;
        uint32_t index_count;

        const mgfx_material* material;
        const mgfx_vertex_layout* vl;
    } primitives[MGFX_MESH_MAX_PRIMITIVES];

    uint32_t primitive_count;
} mgfx_mesh;

enum { MGFX_NODE_MAX_PRIMITIVES = 32 };
typedef struct mgfx_node {
    // global matrix
    mx_mat4 matrix;
    const mgfx_mesh* mesh;
} mgfx_node;

enum { MGFX_SCENE_MAX_NODES = 128 };
enum { MGFX_SCENE_MAX_ROOTS = 32 };
enum { MGFX_SCENE_MAX_MATERIALS = 128 };
enum { MGFX_SCENE_MAX_TEXTURES = MGFX_SCENE_MAX_MATERIALS * 4 };
enum { MGFX_SCENE_MAX_MESHES = 128 };
typedef struct mgfx_scene {
    mgfx_node roots[MGFX_SCENE_MAX_ROOTS];
    uint32_t root_count;

    mgfx_node nodes[MGFX_SCENE_MAX_NODES];
    uint32_t node_count;

    mgfx_th textures[MGFX_SCENE_MAX_TEXTURES];
    uint32_t texture_count;

    mgfx_material materials[MGFX_SCENE_MAX_MATERIALS];
    uint32_t material_count;

    mgfx_mesh meshes[MGFX_SCENE_MAX_MESHES];
    uint32_t mesh_count;

    const mgfx_vertex_layout* vl;
} mgfx_scene;

typedef enum gltf_loader_flags {
    gltf_loader_flag_textures = 1 << 0,
    gltf_loader_flag_materials = 1 << 1,
    gltf_loader_flag_meshes = 1 << 2,

    gltf_loader_flag_default =
        gltf_loader_flag_textures | gltf_loader_flag_materials | gltf_loader_flag_meshes,

    gltf_loader_flag_flip_winding = 1 << 3,

    gltf_loader_flag_max_enum = 0xFF
} gltf_loader_flags;

void load_scene_from_path(const char* path, gltf_loader_flags flags, mgfx_scene* scene);
void scene_destroy(mgfx_scene* scene);

#define LOAD_GLTF_MODEL(model_name, gltf_flags, gltf_ptr)                                \
    load_scene_from_path(GLTF_MODELS_PATH model_name "/glTF/" model_name ".gltf",        \
                         gltf_flags,                                                     \
                         gltf_ptr)

#endif
