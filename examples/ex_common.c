#include "ex_common.h"
#include "mx/mx_log.h"

#include <mx/mx_file.h>
#include <mx/mx_memory.h>

#include <mgfx/mgfx.h>

#include <GLFW/glfw3.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLFWwindow* s_window = NULL;
mx_bool s_keys[GLFW_KEY_LAST];

mx_bool mgfx_get_key(int key_code) {
    return s_keys[key_code];
}

int load_shader_from_path(const char* file_path, shader_vk* shader) {
    mx_arena arena = mx_arena_alloc(MX_MB);

    size_t size;
    if (mx_read_file(file_path, &size, NULL) != MX_SUCCESS) {
        MX_LOG_ERROR("Failed to load shader: %s!", file_path);
        exit(-1);
    }

    char* shader_code = mx_arena_push(&arena, size);
    mx_read_file(file_path, &size, shader_code);

    shader_create(size, shader_code, shader);

    mx_arena_free(&arena);
    return MX_SUCCESS;
}

void load_texture2d_from_path(const char* path, texture_vk* texture) {
    int width, height, channel_count;

    stbi_uc* data = stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);

    if(!data) {
        MX_LOG_ERROR("Texture at %s failed to load", path);
        return;
    }

    assert(data != NULL && "[MGFX_EXAMPLES]: Failed to load texture!");

    size_t size = width * height * 4;

    // stb loads images 1 byte per channel.
    texture_create_2d(width, height, VK_FORMAT_R8G8B8A8_SRGB,
                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, texture);
    texture_update(size, data, texture);
    MX_LOG_SUCCESS("Texture at %s loaded!", path);

    stbi_image_free(data);
}

static void window_resize_callback(GLFWwindow* window, int width, int height) {
    mgfx_reset(width, height);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        s_keys[key] = MX_TRUE;
        return;
    }

    if (action == GLFW_RELEASE) {
        s_keys[key] = MX_FALSE;
        return;
    }
}

#include <cglm/cglm.h>

typedef struct camera {
    vec3 position;

    vec3 forward;
    vec3 right;
    vec3 up;

    mat4 view;
    mat4 proj;
    mat4 view_proj;
} camera;

void camera_create(camera* cam) {
    *cam = (camera){
        .position = {0.0f, 0.0f, 5.0f},
        .forward  = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
    };

    glm_perspective(glm_rad(60.0), 16.0 / 9.0, 0.1, 1000.0f, cam->proj);
}

void camera_update(camera* cam) {
    glm_lookat(cam->position, cam->forward, cam->forward, cam->view);
    glm_mat4_mul(cam->proj, cam->view, cam->view_proj);

    if(mgfx_get_key(GLFW_KEY_W) == MX_TRUE) {
        glm_vec3_add(cam->position, cam->forward, cam->position);
    }

    if(mgfx_get_key(GLFW_KEY_S) == MX_TRUE) {
        glm_vec3_sub(cam->position, cam->forward, cam->position);
    }

    if(mgfx_get_key(GLFW_KEY_D) == MX_TRUE) {
        glm_vec3_add(cam->position, cam->right, cam->position);
    }
}

static camera k_example_camera;

int mgfx_example_app() {
    if (glfwInit() != GLFW_TRUE) {
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    s_window = glfwCreateWindow(APP_WIDTH, APP_HEIGHT, "Examples", NULL, NULL);

    glfwSetFramebufferSizeCallback(s_window, window_resize_callback);
    glfwSetKeyCallback(s_window, key_callback);

    mgfx_init_info mgfx_info = {"Clear Screen", s_window};

    if (mgfx_init(&mgfx_info) != 0) {
        return -1;
    }

    mgfx_example_init();

    while (!glfwWindowShouldClose(s_window)) {
        camera_update(&k_example_camera);
        // mgfx_example_update();

        mgfx_frame();
        glfwPollEvents();
    }

    mgfx_example_shutdown();

    mgfx_shutdown();
    glfwDestroyWindow(s_window);
    glfwTerminate();
    return 0;
}
