#include "ex_common.h"

#include <mx/mx_memory.h>
#include <mx/mx_file.h>

#include <cglm/cglm.h>
#include <string.h>

#define CGLTF_IMPLEMENTATION
#include "../third_party/cgltf.h"

// Frame buffer.
texture_vk color_attachment;
texture_vk depth_attachment;
framebuffer_vk mesh_pass_fb;

shader_vk vertex_shader;
shader_vk fragment_shader;
VkDescriptorSet pbr_descriptor_set;
program_vk gfx_program;

texture_vk diffuse;
VkImageView diffuse_view;
VkSampler diffuse_sampler;

shader_vk compute_shader;
VkDescriptorSet compute_ds_set;
program_vk compute_program;

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

typedef struct mgfx_node {
    // Mesh
    void* vertrices;
    uint32_t vertex_count;

    uint16_t* indices;
    uint32_t index_count;

    vertex_buffer_vk vb;
    index_buffer_vk ib;
} mgfx_node;

typedef struct mgfx_material {
    const texture_vk* diffuse_texture;
} mgfx_material;

enum { MGFX_SCENE_MAX_NODES = 32};
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

void gltf_process_node(const float* mtx, mgfx_scene* scene, cgltf_node* node) {
    if (!node) {
        return;
    }

    if (!node->mesh) {
        return;
    }

    mgfx_node* scene_node = &scene->nodes[scene->node_count++];

    for (size_t primitive_index = 0; primitive_index < node->mesh->primitives_count; primitive_index++) {
        const cgltf_primitive* primitive = &node->mesh->primitives[primitive_index];

        if (primitive->type != cgltf_primitive_type_triangles) {
        }

        for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
            const cgltf_attribute* attribute = &primitive->attributes[attrib_idx];

            if (attribute->type == cgltf_attribute_type_position) {
                scene_node->vertex_count = attribute->data->count;
                scene_node->vertrices = mx_arena_push(&scene->allocator,
                                                      scene_node->vertex_count * scene->vl->stride); 
                break;
            }
        }

        for (size_t attrib_idx = 0; attrib_idx < primitive->attributes_count; attrib_idx++) {
            const cgltf_attribute* attribute = &primitive->attributes[attrib_idx];
            if(scene->vl->attribute_sizes[attribute->type] <= 0) {
                continue;
            }

            const uint8_t* attrib_buffer = (uint8_t*)attribute->data->buffer_view->buffer->data +
                                           attribute->data->buffer_view->offset +
                                           attribute->data->offset;

            const uint32_t attribute_offset = scene->vl->attribute_offsets[attribute->type];

            MX_LOG_INFO("attrib: %d", attribute->type);
            MX_LOG_INFO("- offset: %u", attribute_offset);
            MX_LOG_INFO("- size: %u", scene->vl->attribute_sizes[attribute->type]);
            for (size_t i = 0; i < attribute->data->count; i++) {
                memcpy((uint8_t*)scene_node->vertrices + (i * scene->vl->stride) + attribute_offset,
                       attrib_buffer + (attribute->data->stride * i),
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
            scene_node->index_count = primitive->indices->count;
            scene_node->indices = mx_arena_push(&scene->allocator, primitive->indices->count * sizeof(uint32_t));

            void* idx_buffer = primitive->indices->buffer_view->buffer->data +
                               primitive->indices->buffer_view->offset + primitive->indices->offset;

            for (int i = 0; i < primitive->indices->count; i++) {
                scene_node->indices[i] = *(uint16_t*)(idx_buffer + (i *primitive->indices->stride));
            }
        }
    }

    for (size_t child_idx = 0; child_idx < node->children_count; child_idx++) {
        gltf_process_node(NULL, scene, node->children[child_idx]);
    }
};

void load_scene_from_path(const char* path, mgfx_scene* scene) {
    cgltf_options options = {0};
    cgltf_data* data      = NULL;

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

        load_texture2d_from_path(absolute_path, &scene->textures[i]);
    }

    for(size_t i = 0; i < data->materials_count; i++) {
        const cgltf_material* mat = &data->materials[i];
        MX_LOG_INFO("(%u) %s", mat - data->materials, mat->name);

        // PBR
        if(mat->has_pbr_metallic_roughness) {
            size_t tex_idx = mat->pbr_metallic_roughness.base_color_texture.texture - data->textures;
            scene->materials[i].diffuse_texture = &scene->textures[tex_idx];
        }

        // PHONG
    }

    size_t buffers_size = 0;
    for (size_t i = 0; i < data->buffers_count; i++) {
        buffers_size += data->buffers[i].size;
    }

    scene->allocator = mx_arena_alloc((size_t)(buffers_size * 1.75));
    gltf_process_node(NULL, scene, data->nodes);

    cgltf_free(data);
};

void destroy_scene(mgfx_scene* scene) {
    for(uint32_t i = 0; i < scene->texture_count; i++) {
        texture_destroy(&scene->textures[i]);
    }

    for(uint32_t i = 0; i < scene->node_count; i++) {
        buffer_destroy(&scene->nodes[i].vb);
        buffer_destroy(&scene->nodes[i].ib);
    }
}

mgfx_scene gltf = {};
void mgfx_example_init() {
    mgfx_vertex_layout vl;

    vertex_layout_begin(&vl);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_POSITION, sizeof(float) * 3);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_NORMAL, sizeof(float) * 3);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_COLOR, sizeof(float) * 4);
        vertex_layout_add(&vl, MGFX_VERTEX_ATTRIBUTE_TEXCOORD, sizeof(float) * 2);
    vertex_layout_end(&vl);

    gltf.vl = &vl;

    load_scene_from_path("/Users/christianmarkg.solon/Dev/cgltf/test/glTF-Sample-Models/2.0/"
                         "DamagedHelmet/glTF/DamagedHelmet.gltf",
                         &gltf);

    texture_create_2d(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                      &color_attachment);

    texture_create_2d(width, height,
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

    // TODO: descriptor_set_vk.
    load_texture2d_from_path("/Users/christianmarkg.solon/Downloads/statue.jpg", &diffuse);
    texture_create_view(&diffuse, VK_IMAGE_ASPECT_COLOR_BIT, &diffuse_view);

    create_sampler(VK_FILTER_NEAREST, &diffuse_sampler);

    VkDescriptorImageInfo diffuse_image_desc_info = {
        .sampler     = diffuse_sampler,
        .imageView   = diffuse_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    program_create_descriptor_sets(&gfx_program,
                                   NULL,
                                   &diffuse_image_desc_info,
                                   &pbr_descriptor_set);

    // Mesh load
    vertex_buffer_create(gltf.nodes[0].vertex_count * gltf.vl->stride,
                         gltf.nodes[0].vertrices,
                         &vertex_buffer);

    index_buffer_create(gltf.nodes[0].index_count * sizeof(uint16_t),
                        gltf.nodes[0].indices,
                        &index_buffer);
}

void mgfx_example_updates(const DrawCtx* ctx) {
    VkClearColorValue clear       = {.float32 = {0.0f, 1.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vk_cmd_transition_image(ctx->cmd, &color_attachment.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_clear_image(ctx->cmd, &color_attachment.image, &range, &clear);

    vk_cmd_transition_image(ctx->cmd, &color_attachment.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_program.pipeline);

    static struct ComputePC {
        float time;
    } pc;
    pc.time += 0.001;
    vkCmdPushConstants(ctx->cmd, compute_program.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc),
                       &pc);

    vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_program.layout, 0, 1,
                            &compute_ds_set, 0, NULL);

    vkCmdDispatch(ctx->cmd, (int)width / 16, (int)height / 16, 1);

    vk_cmd_begin_rendering(ctx->cmd, &mesh_pass_fb);

    VkViewport view_port = {
        .x        = 0,
        .y        = 0,
        .width    = width,
        .height   = height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(ctx->cmd, 0, 1, &view_port);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {width, height},
    };
    vkCmdSetScissor(ctx->cmd, 0, 1, &scissor);

    vkCmdBindPipeline(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_program.pipeline);

    static struct graphics_pc {
        mat4 model;
        mat4 view;
        mat4 proj;
    } graphics_pc;
    glm_perspective(glm_rad(60.0), 16.0 / 9.0, 0.1, 1000.0f, graphics_pc.proj);

    {
        vec3 position = {0.0f, 0.0f, 5.0f};
        vec3 forward  = {0.0f, 0.0f, 0.0f};
        vec3 up       = {0.0f, 1.0f, 0.0f};
        glm_lookat(position, forward, up, graphics_pc.view);
    }

    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(ctx->cmd, 0, 1, &vertex_buffer.handle, &offsets);
    vkCmdBindIndexBuffer(ctx->cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(ctx->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_program.layout, 0, 1,
                            &pbr_descriptor_set, 0, NULL);

    vec3 position = {0.0f, 0.0f, 0.0f};
    glm_mat4_identity(graphics_pc.model);

    glm_translate(graphics_pc.model, position);

    vec3 axis = {1.0, 1.0, 1.0};
    glm_rotate(graphics_pc.model, pc.time * 3.141593, axis);

    vkCmdPushConstants(ctx->cmd, gfx_program.layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(graphics_pc), &graphics_pc);

    // Draw scene
    for(uint32_t i = 0; i < gltf.node_count; i++) {
        vkCmdDrawIndexed(ctx->cmd, gltf.nodes[i].index_count, 1, 0, 0, 0);
    }

    vk_cmd_end_rendering(ctx->cmd);

    vk_cmd_transition_image(ctx->cmd, &color_attachment.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vk_cmd_transition_image(ctx->cmd, ctx->frame_target, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_cmd_copy_image_to_image(ctx->cmd, &color_attachment.image, VK_IMAGE_ASPECT_COLOR_BIT,
                               ctx->frame_target);
}

void mgfx_example_shutdown() {
    buffer_destroy(&vertex_buffer);
    buffer_destroy(&index_buffer);

    texture_destroy(&diffuse);
    framebuffer_destroy(&mesh_pass_fb);
    texture_destroy(&depth_attachment);
    texture_destroy(&color_attachment);

    program_destroy(&compute_program);
    shader_destroy(&compute_shader);

    program_destroy(&gfx_program);
    shader_destroy(&fragment_shader);
    shader_destroy(&vertex_shader);
}

int main() { mgfx_example_app(); }
