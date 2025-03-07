#include "ex_common.h"

#include <mx/mx_memory.h>
#include <mx/mx_file.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

#define CGLTF_IMPLEMENTATION
#include "../third_party/cgltf.h"

// Frame buffer
image_vk color_attachment;
image_vk depth_attachment;
framebuffer_vk mesh_pass_fb;

shader_vk vertex_shader;
shader_vk fragment_shader;
program_vk gfx_program;

shader_vk compute_shader;
program_vk compute_program;
VkDescriptorSet compute_ds_set;

vertex_buffer_vk vertex_buffer;
index_buffer_vk index_buffer;

int width  = APP_WIDTH;
int height = APP_HEIGHT;

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

void vertex_layout_begin(mgfx_vertex_layout* vl) { memset(vl, 0, sizeof(mgfx_vertex_layout)); }

void vertex_layout_add(mgfx_vertex_layout* vl, mgfx_vertex_attribute attribute, size_t size) {
    vl->attribute_offsets[attribute] = vl->stride;
    vl->attribute_sizes[attribute] = size;
    vl->stride += size;
};

void vertex_layout_end(mgfx_vertex_layout* vl) {}

typedef struct mgfx_material {
    struct properties {
        vec3 albedo;
        float metallic;
        float roughness;
        float ao;
    } properties;
    buffer_vk properties_buffer;

    const texture_vk* albedo_texture;
    const texture_vk* metallic_roughness_texture;

    VkDescriptorSet ds;
} mgfx_material;

enum { MGFX_NODE_MAX_PRIMITIVES = 32};
typedef struct mgfx_node {
    struct primitive {
        void* vertrices;
        uint32_t vertex_count;

        uint16_t* indices;
        uint32_t index_count;

        vertex_buffer_vk vb;
        index_buffer_vk ib;

        const mgfx_material* material;
    } primitives[MGFX_NODE_MAX_PRIMITIVES];

    mat4 matrix;
    uint32_t primitive_count;
} mgfx_node;

enum { MGFX_SCENE_MAX_NODES = 128};
enum { MGFX_SCENE_MAX_TEXTURES = 32};
typedef struct mgfx_scene {
    mgfx_node nodes[MGFX_SCENE_MAX_NODES];
    uint32_t node_count;

    texture_vk textures[MGFX_SCENE_MAX_TEXTURES];
    uint32_t texture_count;

    mgfx_material materials[MGFX_SCENE_MAX_TEXTURES];
    uint32_t material_count;

    const mgfx_vertex_layout* vl;
    mx_arena allocator;
} mgfx_scene;

void gltf_process_node(const cgltf_data* gltf, mgfx_scene* scene, cgltf_node* node) {
    if (!node) {
        return;
    }

    const size_t vertex_size = scene->vl->stride;

    mgfx_node* scene_node = &scene->nodes[scene->node_count++];
    memcpy(scene_node->matrix, node->matrix, sizeof(mat4));

    MX_LOG_INFO("vertex_size: %u", vertex_size);

    if(node->mesh) {
        scene_node->primitive_count = node->mesh->primitives_count;
        for (size_t primitive_idx = 0; primitive_idx < node->mesh->primitives_count; primitive_idx++) {
            const cgltf_primitive* primitive = &node->mesh->primitives[primitive_idx];
            struct primitive* scene_primitive = &scene_node->primitives[primitive_idx]; 

            if (primitive->type != cgltf_primitive_type_triangles) {
                MX_LOG_WARN("Primitive does not use triangles as type!");
            }

            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                const cgltf_attribute* attribute = &primitive->attributes[attrib_idx];
                if (attribute->type == cgltf_attribute_type_position) {
                    scene_primitive->vertex_count = attribute->data->count;
                    scene_primitive->vertrices = mx_arena_push(&scene->allocator, scene_primitive->vertex_count * vertex_size); 
                    break;
                }
            }

            assert(scene_primitive->vertrices != NULL);

            for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
                const cgltf_attribute* attribute = &primitive->attributes[attrib_idx];

                if(scene->vl->attribute_sizes[attribute->type] <= 0) {
                    continue;
                }

                const uint8_t* attrib_buffer = (uint8_t*)attribute->data->buffer_view->buffer->data +
                                               attribute->data->buffer_view->offset +
                                               attribute->data->offset;

                const uint32_t attribute_offset = scene->vl->attribute_offsets[attribute->type];

                for (size_t i = 0; i < attribute->data->count; i++) {
                    memcpy((uint8_t*)scene_primitive->vertrices +
                           (i * vertex_size) + attribute_offset,
                           attrib_buffer + (i * attribute->data->stride),
                           attribute->data->stride);
                }

                MX_LOG_INFO("attrib: %u offset: %u stride: %u",
                            attribute->type,
                            attribute_offset,
                            attribute->data->stride);

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
                scene_primitive->index_count = primitive->indices->count;
                scene_primitive->indices = mx_arena_push(&scene->allocator, primitive->indices->count * sizeof(uint32_t));

                void* idx_buffer = primitive->indices->buffer_view->buffer->data +
                                   primitive->indices->buffer_view->offset + primitive->indices->offset;

                for (int i = 0; i < primitive->indices->count; i++) {
                    scene_primitive->indices[i] = *(uint16_t*)(idx_buffer + (i *primitive->indices->stride));
                }
            }

            vertex_buffer_create(scene_primitive->vertex_count * scene->vl->stride,
                                 scene_primitive->vertrices,
                                 &vertex_buffer);

            index_buffer_create(scene_primitive->index_count * sizeof(uint16_t),
                                scene_primitive->indices,
                                &index_buffer);

            const uint32_t material_idx = primitive->material - gltf->materials;
            assert(material_idx < scene->material_count && "Material index out of bounds!");

            scene_primitive->material = &scene->materials[material_idx];
        }
    }

    for (size_t child_idx = 0; child_idx < node->children_count; child_idx++) {
        gltf_process_node(gltf, scene, node->children[child_idx]);
    }
};

void load_scene_from_path(const char* path, mgfx_scene* scene) {
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

    const char* file_name = strrchr(path, '/');

    const size_t dir_len = file_name - path;
    char dir_name[strlen(path) + 2];
    strncpy(dir_name, path, dir_len);
    dir_name[dir_len] = '/';
    dir_name[dir_len + 1] = '\0';

    if(data->textures_count > MGFX_SCENE_MAX_TEXTURES) { 
        MX_LOG_ERROR("%s has more textures than MGFX_SCENE_MAX_TEXTURES!", file_name);
        exit(-1);
    }

    scene->texture_count = data->textures_count;

    for(size_t i = 0; i < data->textures_count; i++) {
        char absolute_path[strlen(path) + strlen(data->textures[i].image->uri) + 1];
        strcpy(absolute_path, dir_name);
        strcat(absolute_path, data->textures[i].image->uri);

        create_sampler(VK_FILTER_LINEAR, &scene->textures[i].sampler);
        load_image_2d_from_path(absolute_path, &scene->textures[i]);
        image_create_view(&scene->textures[i].image, VK_IMAGE_ASPECT_COLOR_BIT, &scene->textures[i].view);
    }

    scene->material_count = data->materials_count;
    for(size_t i = 0; i < data->materials_count; i++) {
        const cgltf_material* mat = &data->materials[i];
        MX_LOG_INFO("(%u) %s", mat - data->materials, mat->name);

        // PBR
        if(mat->has_pbr_metallic_roughness) {
            memcpy(&scene->materials[i].properties.albedo, &mat->pbr_metallic_roughness.base_color_factor, sizeof(vec3));
            if(mat->pbr_metallic_roughness.base_color_texture.texture) {
                size_t tex_idx = mat->pbr_metallic_roughness.base_color_texture.texture - data->textures;
                scene->materials[i].albedo_texture = &scene->textures[tex_idx];
            }

            scene->materials[i].properties.metallic = mat->pbr_metallic_roughness.metallic_factor;
            scene->materials[i].properties.roughness = mat->pbr_metallic_roughness.roughness_factor;
            if(mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                size_t tex_idx = mat->pbr_metallic_roughness.metallic_roughness_texture.texture - data->textures;
                scene->materials[i].metallic_roughness_texture= &scene->textures[tex_idx];
            }

            scene->materials[i].properties.ao = 1.0f;

            if(mat->normal_texture.texture) {
            }

            if(mat->occlusion_texture.texture) {
            }

            if(mat->emissive_texture.texture) {
            }

            continue;
        }

        // PHONG
    }

    size_t buffers_size = 0;
    for (size_t i = 0; i < data->buffers_count; i++) {
        buffers_size += data->buffers[i].size;
    }

    // TODO: Estimate properly
    size_t scene_allocator_size = (size_t)(buffers_size * 1.75);
    scene->allocator = mx_arena_alloc(scene_allocator_size);

    gltf_process_node(data, scene, data->nodes);

    cgltf_free(data);
};

void scene_destroy(mgfx_scene* scene) {
    for(uint32_t i = 0; i < scene->texture_count; i++) {
        // image_destroy(&scene->textures[i]);
    }

    for(uint32_t i = 0; i < scene->material_count ; i++) {
        // Destroy property_buffer
        // Destroy ds
    }

    for(uint32_t i = 0; i < scene->node_count; i++) {
        /*buffer_destroy(&scene->nodes[i].vb);*/
        /*buffer_destroy(&scene->nodes[i].ib);*/
    }
}

void log_mat4(const mat4* mat) {
    for(int col = 0; col < 4; col++) {
        MX_LOG_INFO("%.2f %.2f %.2f %.2f",
                    *mat[col][0],
                    *mat[col][1],
                    *mat[col][2],
                    *mat[col][3]);
    }
}

#define LOAD_GLTF_MODEL(model_name, gltf_ptr) \
    load_scene_from_path("/Users/christianmarkg.solon/Dev/cgltf/test/glTF-Sample-Models/2.0/" model_name "/glTF/" model_name ".gltf", gltf_ptr)

mgfx_scene gltf = {};
void mgfx_example_init() {
    mgfx_vertex_layout vl;

    vertex_layout_begin(&vl);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
    vertex_layout_end(&vl);

    gltf.vl = &vl;

    image_create_2d(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                      &color_attachment);

    image_create_2d(width, height,
                      VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                      &depth_attachment);
    framebuffer_create(1, &color_attachment, &depth_attachment, &mesh_pass_fb);

    load_shader_from_path("assets/shaders/gradient.comp.glsl.spv", &compute_shader);
    program_create_compute(&compute_shader, &compute_program);

    VkDescriptorImageInfo color_attachment_desc_info = {
        .sampler     = VK_NULL_HANDLE,
        .imageView   = mesh_pass_fb.color_attachment_views[0],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    program_create_descriptor_sets(&compute_program, NULL, &color_attachment_desc_info,
                                   &compute_ds_set);

    load_shader_from_path("assets/shaders/lit.vert.glsl.spv", &vertex_shader);
    load_shader_from_path("assets/shaders/lit.frag.glsl.spv", &fragment_shader);

    program_create_graphics(&vertex_shader, &fragment_shader, &mesh_pass_fb, &gfx_program);

    #define GLTF_PATH "/Users/christianmarkg.solon/Dev/cgltf/test/glTF-Sample-Models/2.0/"

    /*LOAD_GLTF_MODEL("Suzanne", &gltf);*/
    LOAD_GLTF_MODEL("Lantern", &gltf);
    /*LOAD_GLTF_MODEL("Cube", &gltf);*/
    /*LOAD_GLTF_MODEL("DamagedHelmet", &gltf);*/
    /*LOAD_GLTF_MODEL("MetalRoughSpheresNoTextures", &gltf);*/
    /*LOAD_GLTF_MODEL("MetalRoughSpheres", &gltf);*/
    /*LOAD_GLTF_MODEL("BoxTextured", &gltf);*/

    for(int i = 0; i < gltf.material_count; i++) {
        // TODO: Global materials buffer
        uniform_buffer_create(sizeof(gltf.materials[i].properties),
                              &gltf.materials[i].properties,
                              &gltf.materials[i].properties_buffer);

        VkDescriptorBufferInfo material_properties_buffer_info = {
            .buffer = gltf.materials[i].properties_buffer.handle,
            .offset = 0,
            .range = sizeof(gltf.materials[i].properties_buffer),
        };

        if(gltf.materials[i].albedo_texture) {
        }

        VkDescriptorImageInfo diffuse_image_desc_info = {
            .sampler     = gltf.textures[i].sampler,
            .imageView   = gltf.textures[i].view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        program_create_descriptor_sets(&gfx_program,
                                       &material_properties_buffer_info,
                                       &diffuse_image_desc_info,
                                       &gltf.materials[i].ds);
    }
}

void mgfx_example_updates(const DrawCtx* ctx) {
    VkClearColorValue clear = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_transition_image(ctx->cmd, &color_attachment, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_clear_image(ctx->cmd, &color_attachment, &range, &clear);

    vk_cmd_transition_image(ctx->cmd, &color_attachment, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL);

    /*vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_program.pipeline);*/
    /**/
    /*static struct ComputePC {*/
    /*    float time;*/
    /*} pc;*/
    /*pc.time += 0.001;*/
    /*vkCmdPushConstants(ctx->cmd, compute_program.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);*/
    /**/
    /*vkCmdBindDescriptorSets(ctx->cmd, */
    /*                        VK_PIPELINE_BIND_POINT_COMPUTE,*/
    /*                        compute_program.layout, 0, 1, &compute_ds_set, 0, NULL);*/
    /**/
    /*vkCmdDispatch(ctx->cmd, (int)width / 16, (int)height / 16, 1);*/

    vk_cmd_begin_rendering(ctx->cmd, &mesh_pass_fb);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_program.pipeline);

    static struct graphics_pc {
        mat4 model;
        mat4 view;
        mat4 proj;

        mat4 inverse_view;
    } graphics_pc;

    glm_mat4_copy(g_example_camera.view, graphics_pc.view);
    glm_mat4_copy(g_example_camera.proj, graphics_pc.proj);
    glm_mat4_copy(g_example_camera.inverse_view, graphics_pc.inverse_view);

    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &vertex_buffer.handle, &offsets);
    vkCmdBindIndexBuffer(ctx->cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);

    // Draw scene
    for(uint32_t i = 0; i < gltf.node_count; i++) {
        glm_mat4_identity(graphics_pc.model);
        glm_mat4_mul(gltf.nodes[i].matrix, graphics_pc.model, graphics_pc.model);

        vec3 position = {0.0f, 0.0f, 0.0f};
        glm_translate(graphics_pc.model, position);

        vec3 axis = {0.0, 1.0, 0.0};
        glm_rotate(graphics_pc.model, MGFX_TIME * 3.141593 / 5.0, axis);

        vec3 uniform_scale = {1.0, 1.0, 1.0};
        glm_vec3_scale(uniform_scale, 1.0, uniform_scale);
        glm_scale(graphics_pc.model, uniform_scale);

        vkCmdPushConstants(ctx->cmd, gfx_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(graphics_pc), &graphics_pc);

        for(uint32_t p = 0; p < gltf.nodes[i].primitive_count; p++) {
            const struct primitive* primitive = &gltf.nodes[i].primitives[p];
            vkCmdBindDescriptorSets(ctx->cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    gfx_program.layout, 0, 1, &primitive->material->ds, 0, NULL);
            vkCmdDrawIndexed(ctx->cmd, primitive->index_count, 1, 0, 0, 0);
        }
    }

    vk_cmd_end_rendering(ctx->cmd);

    vk_cmd_transition_image(ctx->cmd, &color_attachment,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_transition_image(ctx->cmd, ctx->frame_target,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_copy_image_to_image(ctx->cmd,
                               &color_attachment,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               ctx->frame_target);
}

void mgfx_example_shutdown() {
    buffer_destroy(&vertex_buffer);
    buffer_destroy(&index_buffer);

    framebuffer_destroy(&mesh_pass_fb);
    image_destroy(&depth_attachment);
    image_destroy(&color_attachment);

    program_destroy(&compute_program);
    shader_destroy(&compute_shader);

    program_destroy(&gfx_program);
    shader_destroy(&fragment_shader);
    shader_destroy(&vertex_shader);
}

int main() { mgfx_example_app(); }
