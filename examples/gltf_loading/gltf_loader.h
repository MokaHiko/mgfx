#ifndef MGFX_EXAMPLE_GLTF_H_
#define MGFX_EXAMPLE_GLTF_H_

// TODO: REMOVE WHEN API AGNOSTIC.
#include "../../mgfx/src/renderer_vk.h"
#include <cglm/cglm.h>

typedef enum mgfx_material_flags : uint8_t{
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
    buffer_vk properties_buffer;
    descriptor_info_vk u_properties_buffer;

    const texture_vk* albedo_texture;
    descriptor_info_vk u_albedo_texture;

    const texture_vk* metallic_roughness_texture;
    descriptor_info_vk u_metallic_roughness_texture;

    const texture_vk* normal_texture;
    descriptor_info_vk u_normal_texture;

    const texture_vk* occlusion_texture;
    descriptor_info_vk u_occlusion_texture;

    const texture_vk* emissive_texture;
    descriptor_info_vk u_emissive_texture;

    mgfx_material_flags flags;
    VkDescriptorSet ds;
} mgfx_material;

enum { MGFX_MESH_MAX_PRIMITIVES = 128};
typedef struct mgfx_mesh {
    struct primitive {
        vertex_buffer_vk vb;
        index_buffer_vk ib;

        void* vertices;
        uint32_t vertex_count;

        uint32_t* indices;
        uint32_t index_count;

        const mgfx_material* material;
        const mgfx_vertex_layout* vl;
    } primitives [MGFX_MESH_MAX_PRIMITIVES];
    uint32_t primitive_count;
} mgfx_mesh;

enum { MGFX_NODE_MAX_PRIMITIVES = 32};
typedef struct mgfx_node {
    mat4 matrix;
    const mgfx_mesh* mesh;
} mgfx_node;

enum { MGFX_SCENE_MAX_NODES = 128};
enum { MGFX_SCENE_MAX_ROOTS = 32};
enum { MGFX_SCENE_MAX_MATERIALS = 32};
enum { MGFX_SCENE_MAX_TEXTURES = MGFX_SCENE_MAX_MATERIALS * 4};
enum { MGFX_SCENE_MAX_MESHES = 64};
typedef struct mgfx_scene {
    mgfx_node roots[MGFX_SCENE_MAX_ROOTS];
    uint32_t root_count;

    mgfx_node nodes[MGFX_SCENE_MAX_NODES];
    uint32_t node_count;

    texture_vk textures[MGFX_SCENE_MAX_TEXTURES];
    uint32_t texture_count;

    mgfx_material materials[MGFX_SCENE_MAX_MATERIALS];
    uint32_t material_count;

    mgfx_mesh meshes[MGFX_SCENE_MAX_MESHES];
    uint32_t mesh_count;

    const mgfx_vertex_layout* vl;
    mx_arena allocator;
} mgfx_scene;

typedef enum gltf_loader_flags : uint8_t {
    gltf_loader_flag_textures  = 1 << 0,
    gltf_loader_flag_materials = 1 << 1,
    gltf_loader_flag_meshes    = 1 << 2,

    gltf_loader_flag_default = gltf_loader_flag_textures |
                               gltf_loader_flag_materials |
                               gltf_loader_flag_meshes,

    gltf_loader_flag_flip_winding = 1 << 3,

    gltf_loader_flag_max_enum = 0xFF
} gltf_loader_flags;

void load_scene_from_path(const char* path,
                          const program_vk* gfx_program,
                          gltf_loader_flags flags,
                          mgfx_scene* scene);
void scene_destroy(mgfx_scene* scene);

#define LOAD_GLTF_MODEL(model_name, gfx_program_ptr, gltf_flags, gltf_ptr) \
    load_scene_from_path(GLTF_MODELS_PATH model_name "/glTF/" model_name ".gltf", gfx_program_ptr, gltf_flags, gltf_ptr)

#endif 
