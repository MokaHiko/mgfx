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

double MGFX_TIME = 0.0f;
double last_time = 0.0f;
double MGFX_TIME_DELTA_TIME = 0.0;

camera g_example_camera;

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

void load_image_2d_from_path(const char* path, texture_vk* texture) {
    int width, height, channel_count;

    //stbi_set_flip_vertically_on_load(1);
    stbi_uc* data = stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);

    if(!data) {
        MX_LOG_ERROR("Texture at %s failed to load", path);
        return;
    }

    assert(data != NULL && "[MGFX_EXAMPLES]: Failed to load texture!");

    size_t size = width * height * 4;

    // stb loads images 1 byte per channel.
    image_create_2d(width, height,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    &texture->image);
    image_update(size, data, &texture->image);
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

void camera_create(camera* cam) {
    *cam = (camera){.position = {0.0f, 0.0f, 5.0f},
                    .forward  = {0.0f, 0.0f, -1.0f},
                    .up       = {0.0f, 1.0f, 0.0f},
                    .right    = {1.0f, 0.0f, 0.0f}};

    glm_perspective(glm_rad(60.0), 16.0 / 9.0, 1000.0f, 0.1f, cam->proj);
    cam->proj[1][1] *= -1;
}

void camera_update(camera* cam) {
    vec3 look_at = GLM_VEC3_ZERO;
    glm_vec3_add(cam->position, cam->forward, look_at);

    glm_lookat(cam->position, look_at, cam->up, cam->view);
    glm_mat4_inv(cam->view, cam->inverse_view);
    glm_mat4_mul(cam->proj, cam->view, cam->view_proj);

    vec3 direction = GLM_VEC3_ZERO;
    if(mgfx_get_key(GLFW_KEY_W) == MX_TRUE) {
        glm_vec3_add(direction, cam->forward, direction);
    }

    if(mgfx_get_key(GLFW_KEY_S) == MX_TRUE) {
        glm_vec3_sub(direction, cam->forward, direction);
    }

    if(mgfx_get_key(GLFW_KEY_D) == MX_TRUE) {
        glm_vec3_add(direction, cam->right, direction);
    }

    if(mgfx_get_key(GLFW_KEY_A) == MX_TRUE) {
        glm_vec3_sub(direction, cam->right, direction);
    }

    if(mgfx_get_key(GLFW_KEY_SPACE) == MX_TRUE) {
        glm_vec3_add(direction, cam->up, direction);
    }

    if(mgfx_get_key(GLFW_KEY_Q) == MX_TRUE) {
        glm_vec3_sub(direction, cam->up, direction);
    }

    double ms = 5.0f;
    if(!glm_vec3_eqv(direction, GLM_VEC3_ZERO)) {
        glm_vec3_norm(direction);
    }

    glm_vec3_scale(direction, ms * MGFX_TIME_DELTA_TIME, direction);
    glm_vec3_add(cam->position, direction, cam->position);
}

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

    camera_create(&g_example_camera);

    mgfx_example_init();

    while (!glfwWindowShouldClose(s_window)) {
        MGFX_TIME = glfwGetTime();
        MGFX_TIME_DELTA_TIME = MGFX_TIME - last_time;
        last_time = MGFX_TIME;

        camera_update(&g_example_camera);
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
