#ifndef MGFX_EXAMPLE_GLTF_H_
#define MGFX_EXAMPLE_GLTF_H_

#include <cglm/cglm.h>

#include <mgfx/mgfx.h>
#include <mx/mx_memory.h>

typedef enum mgfx_vertex_attribute {
    MGFX_VERTEX_ATTRIBUTE_INVALID = 0,
    MGFX_VERTEX_ATTRIBUTE_POSITION,
    MGFX_VERTEX_ATTRIBUTE_NORMAL,
    MGFX_VERTEX_ATTRIBUTE_TANGENT,
    MGFX_VERTEX_ATTRIBUTE_TEXCOORD,
    MGFX_VERTEX_ATTRIBUTE_COLOR,
    MGFX_VERTEX_ATTRIBUTE_JOINTS,
    MGFX_VERTEX_ATTRIBUTE_WEIGHTS,
    MGFX_VERTEX_ATTRIBUTE_CUSTOM,

    MGFX_VERTEX_ATTRIBUTE_COUNT
} mgfx_vertex_attribute;

enum { MGFX_MAX_VERTEX_BINDINGS = 16 };
typedef struct mgx_vertex_layout {
    size_t attribute_sizes[MGFX_VERTEX_ATTRIBUTE_COUNT];
    size_t attribute_offsets[MGFX_VERTEX_ATTRIBUTE_COUNT];

    size_t stride;
} mgfx_vertex_layout;

void vertex_layout_begin(mgfx_vertex_layout* vl);
void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size);
void vertex_layout_end(mgfx_vertex_layout* vl);

typedef enum mgfx_material_flags : uint8_t {
    mgfx_material_flag_casts_none = 0,

    mgfx_material_flag_casts_shadows = 1 << 0,
    mgfx_material_flag_receive_shadows = 1 << 1,

    mgfx_material_flag_instanced = 1 << 2,

    mgfx_material_flag_max_enum = 0xFF,
} mgfx_material_flags;

typedef struct mgfx_material {
    struct properties {
        vec3 albedo;
        float metallic;
        float roughness;
        float ao;
        float emissive;
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

        void* vertices;
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
    mat4 matrix;
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
    mx_arena allocator;
} mgfx_scene;

typedef enum gltf_loader_flags : uint8_t {
    gltf_loader_flag_textures = 1 << 0,
    gltf_loader_flag_materials = 1 << 1,
    gltf_loader_flag_meshes = 1 << 2,

    gltf_loader_flag_default = gltf_loader_flag_textures | gltf_loader_flag_materials | gltf_loader_flag_meshes,

    gltf_loader_flag_flip_winding = 1 << 3,

    gltf_loader_flag_max_enum = 0xFF
} gltf_loader_flags;

void load_scene_from_path(const char* path, gltf_loader_flags flags, mgfx_scene* scene);
void scene_destroy(mgfx_scene* scene);

#define LOAD_GLTF_MODEL(model_name, gltf_flags, gltf_ptr)                                   \
    load_scene_from_path(GLTF_MODELS_PATH model_name "/glTF/" model_name ".gltf", gltf_flags, gltf_ptr)

#endif
