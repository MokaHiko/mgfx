#include "gltf_loader.h"
#include "ex_common.h"

#include <GLFW/glfw3.h>
#include <mx/mx_log.h>

#include <mx/mx_file.h>
#include <mx/mx_memory.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

#define CGLTF_IMPLEMENTATION
#include <third_party/cgltf.h>
#include <third_party/mikktspace/mikktspace.h>

static int mikk_get_num_faces(const SMikkTSpaceContext* ctx) {
    const struct primitive* primitive = ctx->m_pUserData;
    return primitive->index_count / 3;
}

static int mikk_get_num_vertices_of_face(const SMikkTSpaceContext* ctx, const int iface) { return 3; }

static void
mikk_get_position(const SMikkTSpaceContext* ctx, float fv_pos_out[], const int iface, const int ivert) {
    const struct primitive* primitive = ctx->m_pUserData;
    const uint32_t idx = primitive->indices[iface * 3 + ivert];

    size_t offset = primitive->vl->stride * idx;
    const float* position = (float*)(primitive->vertices + offset +
                                     primitive->vl->attribute_offsets[MGFX_VERTEX_ATTRIBUTE_POSITION]);
    fv_pos_out[0] = position[0];
    fv_pos_out[1] = position[1];
    fv_pos_out[2] = position[2];
}

static void
mikk_get_normal(const SMikkTSpaceContext* ctx, float fv_norm_out[], const int iface, const int ivert) {
    const struct primitive* primitive = ctx->m_pUserData;
    const uint32_t idx = primitive->indices[iface * 3 + ivert];

    size_t offset = primitive->vl->stride * idx;
    const float* normal = (float*)(primitive->vertices + offset +
                                   primitive->vl->attribute_offsets[MGFX_VERTEX_ATTRIBUTE_NORMAL]);
    fv_norm_out[0] = normal[0];
    fv_norm_out[1] = normal[1];
    fv_norm_out[2] = normal[2];
}

static void
mikk_get_tex_coord(const SMikkTSpaceContext* ctx, float fv_texc_out[], const int iface, const int ivert) {
    const struct primitive* primitive = ctx->m_pUserData;
    const uint32_t idx = primitive->indices[iface * 3 + ivert];

    size_t offset = primitive->vl->stride * idx;
    const float* texc = (float*)(primitive->vertices + offset +
                                 primitive->vl->attribute_offsets[MGFX_VERTEX_ATTRIBUTE_TEXCOORD]);
    fv_texc_out[0] = texc[0];
    fv_texc_out[1] = texc[1];
}

static void mikk_set_tspace_basic(const SMikkTSpaceContext* ctx,
                                  const float fv_tangent[],
                                  const float fsign,
                                  const int iface,
                                  const int ivert) {
    struct primitive* primitive = ctx->m_pUserData;
    const uint32_t idx = primitive->indices[iface * 3 + ivert];

    size_t offset = primitive->vl->stride * idx;
    float* tangent = (float*)(primitive->vertices + offset +
                              primitive->vl->attribute_offsets[MGFX_VERTEX_ATTRIBUTE_TANGENT]);
    tangent[0] = fv_tangent[0];
    tangent[1] = fv_tangent[1];
    tangent[2] = fv_tangent[2];
    tangent[3] = fsign;
}

void vertex_layout_begin(mgfx_vertex_layout* vl) { memset(vl, 0, sizeof(mgfx_vertex_layout)); }

void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size) {
    vl->attribute_offsets[attribute] = vl->stride;
    vl->attribute_sizes[attribute] = size;
    vl->stride += size;
};

void vertex_layout_end(mgfx_vertex_layout* vl) {}

static void gltf_process_node(const cgltf_data* gltf, mat4 parent_mtx, mgfx_scene* scene, cgltf_node* node) {
    if (!node) {
        return;
    }

    const size_t vertex_size = scene->vl->stride;
    mgfx_node* scene_node = &scene->nodes[scene->node_count++];

    if (node->has_matrix) {
        memcpy(scene_node->matrix, node->matrix, sizeof(mat4));
    } else {
        glm_mat4_identity(scene_node->matrix);
        glm_translate(scene_node->matrix, node->translation);

        mat4 rotation_mtx;
        glm_mat4_identity(rotation_mtx);

#ifdef MX_MACOS
        glm_quat_mat4(node->rotation, rotation_mtx);
#endif
        glm_mat4_mul(scene_node->matrix, rotation_mtx, scene_node->matrix);

        glm_scale(scene_node->matrix, node->scale);
    }

    glm_mat4_mul(parent_mtx, scene_node->matrix, scene_node->matrix);

    if (node->mesh) {
        scene_node->mesh = &scene->meshes[node->mesh - gltf->meshes];
    };

    for (size_t child_idx = 0; child_idx < node->children_count; child_idx++) {
        gltf_process_node(gltf, scene_node->matrix, scene, node->children[child_idx]);
    }
};

#define MGFX_MAX_DIR_LEN 256
void load_scene_from_path(const char* path, gltf_loader_flags flags, mgfx_scene* scene) {
    cgltf_options options = {0};
    cgltf_data* data = NULL;

    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        MX_LOG_ERROR("Failed to load gltf at path: %s", path);
        exit(-1);
    };

    cgltf_options buffer_load_opt = {0};
    if (cgltf_load_buffers(&buffer_load_opt, data, path) != cgltf_result_success) {
        MX_LOG_ERROR("Failed to load buffers at path: %s", path);
        exit(-1);
    }

    // TODO: Estimate properly
    // Allocate memory for scene
    size_t buffers_size = 0;
    for (size_t i = 0; i < data->buffers_count; i++) {
        buffers_size += data->buffers[i].size;
    }
    size_t scene_allocator_size = (size_t)(buffers_size * 2);
    scene->allocator = mx_arena_alloc(scene_allocator_size);

    const char* file_name = strrchr(path, '/');
    const size_t dir_len = file_name - path;
    char dir_name[MGFX_MAX_DIR_LEN];
    strncpy(dir_name, path, dir_len);
    dir_name[dir_len] = '/';
    dir_name[dir_len + 1] = '\0';

    if (data->textures_count > MGFX_SCENE_MAX_TEXTURES) {
        MX_LOG_ERROR("%s has more textures than MGFX_SCENE_MAX_TEXTURES!", file_name);
        exit(-1);
    }
    scene->texture_count = data->textures_count;

    if (data->materials_count > MGFX_SCENE_MAX_MATERIALS) {
        MX_LOG_ERROR("%s has more materials than MGFX_SCENE_MAX_MATERIALS!", file_name);
        exit(-1);
    }
    scene->material_count = data->materials_count;
    if ((flags & gltf_loader_flag_materials) == gltf_loader_flag_materials) {
        for (size_t i = 0; i < data->materials_count; i++) {
            const cgltf_material* mat = &data->materials[i];
            MX_LOG_INFO("(%u) Material: %s", mat - data->materials, mat->name ? mat->name : "<unamed>");

            // PBR
            if (mat->has_pbr_metallic_roughness) {
                memcpy(&scene->materials[i].properties.albedo,
                       &mat->pbr_metallic_roughness.base_color_factor,
                       sizeof(vec3));
                if (mat->pbr_metallic_roughness.base_color_texture.texture) {
                    size_t tex_idx = mat->pbr_metallic_roughness.base_color_texture.texture - data->textures;

                    // Check if texture already loaded
                    if ((VkImageView)scene->textures[tex_idx].idx == VK_NULL_HANDLE) {
                        char absolute_path[MGFX_MAX_DIR_LEN];
                        strcpy(absolute_path, dir_name);
                        strcat(absolute_path, data->textures[tex_idx].image->uri);

                        scene->textures[tex_idx] = load_texture_2d_from_path(absolute_path, VK_FORMAT_R8G8B8A8_SRGB);
                    }

                    scene->materials[i].albedo_texture = scene->textures[tex_idx];
                } else {
                    scene->materials[i].albedo_texture = s_default_white;
                }

                scene->materials[i].properties.metallic = mat->pbr_metallic_roughness.metallic_factor;
                scene->materials[i].properties.roughness = mat->pbr_metallic_roughness.roughness_factor;
                scene->materials[i].properties.ao = 1.0f;

                scene->materials[i].properties.emissive = 1.0f;
                if (mat->has_emissive_strength) {
                    scene->materials[i].properties.emissive_strength =
                        mat->emissive_strength.emissive_strength;
                } else {
                    scene->materials[i].properties.emissive_strength = 1.0f;
                }

                if (mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                    size_t tex_idx =
                        mat->pbr_metallic_roughness.metallic_roughness_texture.texture - data->textures;

                    // Check if texture already loaded
                    if ((VkImageView)scene->textures[tex_idx].idx == VK_NULL_HANDLE) {
                        char absolute_path[MGFX_MAX_DIR_LEN];
                        strcpy(absolute_path, dir_name);
                        strcat(absolute_path, data->textures[tex_idx].image->uri);

                        scene->textures[tex_idx] =
                            load_texture_2d_from_path(absolute_path, VK_FORMAT_R8G8B8A8_SRGB);
                    }

                    scene->materials[i].metallic_roughness_texture = scene->textures[tex_idx];
                } else {
                    scene->materials[i].metallic_roughness_texture = s_default_white;
                }

                if (mat->normal_texture.texture) {
                    size_t tex_idx = mat->normal_texture.texture - data->textures;

                    // Check if texture already loaded
                    if ((VkImageView)scene->textures[tex_idx].idx == VK_NULL_HANDLE) {
                        char absolute_path[MGFX_MAX_DIR_LEN];
                        strcpy(absolute_path, dir_name);
                        strcat(absolute_path, data->textures[tex_idx].image->uri);

                        scene->textures[tex_idx] =
                            load_texture_2d_from_path(absolute_path, VK_FORMAT_R8G8B8A8_SRGB);
                    }

                    scene->materials[i].normal_texture = scene->textures[tex_idx];
                } else {
                    scene->materials[i].normal_texture = s_default_normal_map;
                }

                if (mat->occlusion_texture.texture) {
                    size_t tex_idx = mat->occlusion_texture.texture - data->textures;

                    // Check if texture already loaded
                    if ((VkImageView)scene->textures[tex_idx].idx == VK_NULL_HANDLE) {
                        char absolute_path[MGFX_MAX_DIR_LEN];
                        strcpy(absolute_path, dir_name);
                        strcat(absolute_path, data->textures[tex_idx].image->uri);

                        scene->textures[tex_idx] =
                            load_texture_2d_from_path(absolute_path, VK_FORMAT_R8G8B8A8_SRGB);
                    }

                    scene->materials[i].occlusion_texture = scene->textures[tex_idx];
                } else {
                    scene->materials[i].occlusion_texture = s_default_white;
                }

                if (mat->emissive_texture.texture) {
                    size_t tex_idx = mat->emissive_texture.texture - data->textures;

                    // Check if texture already loaded
                    if ((VkImageView)scene->textures[tex_idx].idx == VK_NULL_HANDLE) {
                        char absolute_path[MGFX_MAX_DIR_LEN];
                        strcpy(absolute_path, dir_name);
                        strcat(absolute_path, data->textures[tex_idx].image->uri);

                        scene->textures[tex_idx] = load_texture_2d_from_path(absolute_path, VK_FORMAT_R8G8B8A8_SRGB);
                    }

                    scene->materials[i].emissive_texture = scene->textures[tex_idx];
                } else {
                    scene->materials[i].emissive_texture = s_default_black;
                }

                continue;
            }

            // PHONG
        }
    }

    if (data->meshes_count > MGFX_SCENE_MAX_MESHES) {
        MX_LOG_ERROR("%s has more meshes than MGFX_SCENE_MAX_MESHES!", file_name);
        exit(-1);
    }
    scene->mesh_count = data->meshes_count;
    for (size_t mesh_idx = 0; mesh_idx < data->meshes_count; mesh_idx++) {
        const cgltf_mesh* mesh = &data->meshes[mesh_idx];
        const size_t vertex_size = scene->vl->stride;

        if (mesh->primitives_count > MGFX_MESH_MAX_PRIMITIVES) {
            MX_LOG_ERROR("%s has more primitives than MGFX_MESH_MAX_PRIMITIVES!", file_name);
            exit(-1);
        }

        scene->meshes[mesh_idx].primitive_count = mesh->primitives_count;
        for (size_t primitive_idx = 0; primitive_idx < mesh->primitives_count; primitive_idx++) {
            const cgltf_primitive* primitive = &mesh->primitives[primitive_idx];
            struct primitive* mesh_primitive = &scene->meshes[mesh_idx].primitives[primitive_idx];

            uint32_t material_idx = primitive->material - data->materials;
            if (material_idx >= scene->material_count) {
                MX_LOG_ERROR("scene %s has no material at index %u!", file_name, material_idx);
                exit(-1);
            }
            mesh_primitive->material = &scene->materials[material_idx];
            mesh_primitive->vl = scene->vl;

            if (primitive->type != cgltf_primitive_type_triangles) {
                MX_LOG_WARN("Primitive does not use triangles as type!");
            }

            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                const cgltf_attribute* attribute = &primitive->attributes[attrib_idx];

                if (attribute->type == cgltf_attribute_type_position) {
                    mesh_primitive->vertex_count = attribute->data->count;
                    mesh_primitive->vertices =
                        mx_arena_push(&scene->allocator, mesh_primitive->vertex_count * vertex_size);
                    break;
                }
            }

            assert(mesh_primitive->vertices != NULL);

            mx_bool has_tangents = MX_FALSE;
            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                const cgltf_attribute* attribute = &primitive->attributes[attrib_idx];

                if (attribute->type == cgltf_attribute_type_tangent) {
                    has_tangents = MX_TRUE;
                }

                if (mesh_primitive->vl->attribute_sizes[attribute->type] <= 0) {
                    continue;
                }

                const uint8_t* attrib_buffer = (uint8_t*)attribute->data->buffer_view->buffer->data +
                                               attribute->data->buffer_view->offset + attribute->data->offset;

                const uint32_t attrib_offset = scene->vl->attribute_offsets[attribute->type];

                if (attribute->data->stride != scene->vl->attribute_sizes[attribute->type]) {
                    MX_LOG_WARN("%d strides not equal (%u != %u)",
                                attribute->type,
                                attribute->data->stride,
                                scene->vl->attribute_sizes[attribute->type]);
                    assert(0);
                }

                for (size_t attrib_data_idx = 0; attrib_data_idx < attribute->data->count;
                     attrib_data_idx++) {
                    memcpy((uint8_t*)mesh_primitive->vertices + (attrib_data_idx * vertex_size) +
                               attrib_offset,
                           attrib_buffer + (attrib_data_idx * attribute->data->stride),
                           attribute->data->stride);
                }

                switch (attribute->type) {
                case cgltf_attribute_type_normal:
                    break;

                case cgltf_attribute_type_tangent:
                    break;

                case cgltf_attribute_type_texcoord:
                    break;

                case cgltf_attribute_type_color:
                    break;

                case cgltf_attribute_type_position:
                case cgltf_attribute_type_joints:
                case cgltf_attribute_type_weights:
                case cgltf_attribute_type_invalid:
                case cgltf_attribute_type_custom:
                case cgltf_attribute_type_max_enum:
                    break;
                };
            }

            if (primitive->indices) {
                mesh_primitive->index_count = primitive->indices->count;
                mesh_primitive->indices =
                    mx_arena_push(&scene->allocator, primitive->indices->count * sizeof(uint32_t));

                uint8_t* idx_buffer = (uint8_t*)primitive->indices->buffer_view->buffer->data +
                                      primitive->indices->buffer_view->offset + primitive->indices->offset;

                if (primitive->indices->component_type == cgltf_component_type_r_16u) {
                    for (int i = 0; i < primitive->indices->count; i++) {
                        int insert_index = i;

                        if ((flags & gltf_loader_flag_flip_winding) == gltf_loader_flag_flip_winding) {
                            insert_index = primitive->indices->count - 1 - i;
                        }

                        mesh_primitive->indices[insert_index] =
                            *(uint16_t*)(idx_buffer + (i * primitive->indices->stride));
                    }
                } else if (primitive->indices->component_type == cgltf_component_type_r_32u) {
                    for (int i = 0; i < primitive->indices->count; i++) {
                        int insert_index = i;

                        if ((flags & gltf_loader_flag_flip_winding) == gltf_loader_flag_flip_winding) {
                            insert_index = primitive->indices->count - 1 - i;
                        }

                        mesh_primitive->indices[insert_index] =
                            *(uint32_t*)(idx_buffer + (i * primitive->indices->stride));
                    }
                }
            }

            if (!has_tangents) {
                MX_LOG_WARN("Model has no tangents. Computing them manually...");
                SMikkTSpaceInterface mikk_interface = {
                    .m_getNumFaces = mikk_get_num_faces,
                    .m_getNumVerticesOfFace = mikk_get_num_vertices_of_face,
                    .m_getPosition = mikk_get_position,
                    .m_getNormal = mikk_get_normal,
                    .m_getTexCoord = mikk_get_tex_coord,
                    .m_setTSpaceBasic = mikk_set_tspace_basic,
                    .m_setTSpace = NULL,
                };

                SMikkTSpaceContext mikk_ctx = {
                    .m_pInterface = &mikk_interface,
                    .m_pUserData = mesh_primitive,
                };

                if (genTangSpaceDefault(&mikk_ctx) > 0) {
                    MX_LOG_SUCCESS("Tangents Calculated!");
                }
            }

            mesh_primitive->vbh = mgfx_vertex_buffer_create(mesh_primitive->vertices,
                                                            mesh_primitive->vertex_count * scene->vl->stride);

            mesh_primitive->ibh = mgfx_index_buffer_create(mesh_primitive->indices,
                                                           mesh_primitive->index_count * sizeof(uint32_t));

            MX_LOG_SUCCESS("Mesh loaded: (%u vertices, %d indices) %s",
                           mesh_primitive->vertex_count,
                           mesh_primitive->index_count,
                           mesh->name ? mesh->name : "<unamed>");
        }
    }

    assert(data->scenes > 0);

    cgltf_scene* cgltf_scene = &data->scenes[0];
    if (cgltf_scene->nodes_count > MGFX_SCENE_MAX_NODES) {
        MX_LOG_ERROR("%s has more nodes than > MGFX_SCENE_MAX_NODES!", file_name);
        exit(-1);
    }

    mat4 identity;
    glm_mat4_identity(identity);
    for (size_t root_idx = 0; root_idx < cgltf_scene->nodes_count; root_idx++) {
        gltf_process_node(data, identity, scene, cgltf_scene->nodes[root_idx]);
    };

    if ((flags & gltf_loader_flag_materials) == gltf_loader_flag_materials) {
        for (int mat_idx = 0; mat_idx < scene->material_count; mat_idx++) {
            mgfx_material* mat = &scene->materials[mat_idx];

            mat->properties_buffer = mgfx_uniform_buffer_create(&mat->properties, sizeof(mat->properties));
            mat->u_properties_buffer =
                mgfx_descriptor_create("u_properties_buffer", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            mgfx_set_buffer(mat->u_properties_buffer, mat->properties_buffer);

            mat->u_albedo_texture =
                mgfx_descriptor_create("u_albedo_texture", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mgfx_set_texture(mat->u_albedo_texture, mat->albedo_texture);

            mat->u_metallic_roughness_texture = mgfx_descriptor_create(
                "u_metallic_roughness_texture", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mgfx_set_texture(mat->u_metallic_roughness_texture, mat->metallic_roughness_texture);

            mat->u_normal_texture =
                mgfx_descriptor_create("u_normal_texture", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mgfx_set_texture(mat->u_normal_texture, mat->normal_texture);

            mat->u_occlusion_texture =
                mgfx_descriptor_create("u_occlusion_texture", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mgfx_set_texture(mat->u_occlusion_texture, mat->occlusion_texture);

            mat->u_emissive_texture =
                mgfx_descriptor_create("u_emissive_texture", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mgfx_set_texture(mat->u_emissive_texture, mat->emissive_texture);
        }
    }

    cgltf_free(data);
};

void scene_destroy(mgfx_scene* scene) {
    for (uint32_t i = 0; i < scene->texture_count; i++) {
        mgfx_texture_destroy(scene->textures[i], MX_TRUE);
    }

    for (uint32_t i = 0; i < scene->material_count; i++) {
        mgfx_buffer_destroy(scene->materials[i].properties_buffer.idx);
    }

    for (uint32_t i = 0; i < scene->mesh_count; i++) {
        for (size_t primitive_idx = 0; primitive_idx < scene->meshes[i].primitive_count; primitive_idx++) {
            mgfx_buffer_destroy(scene->meshes[i].primitives[primitive_idx].vbh.idx);
            mgfx_buffer_destroy(scene->meshes[i].primitives[primitive_idx].ibh.idx);
        }
    }
}
