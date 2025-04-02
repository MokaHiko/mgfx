#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include "ex_common.h"

#include "mx/mx_log.h"

#include <mx/mx_file.h>
#include <mx/mx_memory.h>

#include <mgfx/mgfx.h>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLFWwindow* s_window = NULL;
mx_bool s_keys[GLFW_KEY_LAST];

double mouse_delta_x;
double mouse_delta_y;
double mouse_last_x;
double mouse_last_y;

double MGFX_TIME = 0.0f;
double last_time = 0.0f;
double MGFX_TIME_DELTA_TIME = 0.0;

camera g_example_camera;
texture_vk s_default_white;
texture_vk s_default_black;
texture_vk s_default_normal_map;

mx_bool mgfx_get_key(int key_code) {
    return s_keys[key_code];
}

float mgfx_get_axis(mgfx_input_axis axis) {
    switch (axis) {
        case MGFX_INPUT_AXIS_MOUSE_X: return mouse_delta_x / (double)APP_WIDTH;
        case MGFX_INPUT_AXIS_MOUSE_Y: return mouse_delta_y / (double)APP_HEIGHT;
        default: return 0.0f;
    }
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

void load_texture_2d_from_path(const char* path, VkFormat format, texture_vk* texture) {
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
    texture_create_2d(width, height, format, VK_FILTER_LINEAR, texture);
    image_update(size, data, &texture->image);

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

static mx_bool fist_move = MX_TRUE;
static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (fist_move) {
        mouse_last_x = xpos;
        mouse_last_y = ypos;

        fist_move = MX_FALSE;
        return;
    }

    mouse_delta_x = xpos - mouse_last_x;
    mouse_delta_y = ypos - mouse_last_y;

    mouse_last_x = xpos;
    mouse_last_y = ypos;
}

void camera_create(mgfx_camera_type type, camera* cam) {
    *cam = (camera){.position = {0.0f, 0.0f, 5.0f},
                    .forward  = {0.0f, 0.0f,-1.0f},
                    .up       = {0.0f, 1.0f, 0.0f},
                    .right    = {1.0f, 0.0f, 0.0f}};

    switch (type) {
        case mgfx_camera_type_orthographic:
            glm_ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 10000.0f, cam->proj);
            cam->proj[1][1] *= -1;
            break;
        case mgfx_camera_type_perspective:
            glm_perspective(glm_rad(60.0), 16.0 / 9.0, 0.1f, 10000.0f, cam->proj);
            cam->proj[1][1] *= -1;
            break;
    }

    cam->yaw = -90.0f;
    cam->pitch = 0.0f;
}

void camera_update(camera* cam) {
    vec3 direction;
    direction[0] = cos(glm_rad(cam->yaw)) * cos(glm_rad(cam->pitch));
    direction[1] = sin(glm_rad(cam->pitch));
    direction[2] = sin(glm_rad(cam->yaw)) * cos(glm_rad(cam->pitch));
    glm_normalize(direction);
    glm_vec3_copy(direction, cam->forward);

    vec3 WORLD_UP = {0, 1, 0};
    glm_cross(cam->forward, WORLD_UP, cam->right);
    glm_normalize(cam->right);

    vec3 look_at;
    glm_vec3_add(cam->position, cam->forward, look_at);
    glm_lookat(cam->position, look_at, cam->up, cam->view);

    glm_mat4_inv(cam->view, cam->inverse_view);
    glm_mat4_mul(cam->proj, cam->view, cam->view_proj);

    vec3 input = GLM_VEC3_ZERO;
    if (mgfx_get_key(GLFW_KEY_W) == MX_TRUE) {
        glm_vec3_add(input, cam->forward, input);
    }

    if (mgfx_get_key(GLFW_KEY_S) == MX_TRUE) {
        glm_vec3_sub(input, cam->forward, input);
    }

    if (mgfx_get_key(GLFW_KEY_D) == MX_TRUE) {
        glm_vec3_add(input, cam->right, input);
    }

    if (mgfx_get_key(GLFW_KEY_A) == MX_TRUE) {
        glm_vec3_sub(input, cam->right, input);
    }

    if (mgfx_get_key(GLFW_KEY_SPACE) == MX_TRUE) {
        glm_vec3_add(input, cam->up, input);
    }

    if (mgfx_get_key(GLFW_KEY_Q) == MX_TRUE) {
        glm_vec3_sub(input, cam->up, input);
    }

    float editor_sens = 100.0f;
    cam->yaw += mgfx_get_axis(MGFX_INPUT_AXIS_MOUSE_X) * editor_sens;
    cam->pitch -= mgfx_get_axis(MGFX_INPUT_AXIS_MOUSE_Y) * editor_sens;
    cam->pitch = glm_clamp(cam->pitch, -89.0f, 89.0f);

    if(glm_vec3_eqv(input, GLM_VEC3_ZERO)) {
        return;
    }

    double ms = 5.0f;

    if (!glm_vec3_eqv(input, GLM_VEC3_ZERO)) {
        glm_vec3_norm(input);
    }

    glm_vec3_scale(input, ms * MGFX_TIME_DELTA_TIME, input);
    glm_vec3_add(cam->position, input, cam->position);
}

void log_mat4(const mat4 mat) {
    for(int col = 0; col < 4; col++) {
        MX_LOG_INFO("%.2f %.2f %.2f %.2f",
                    mat[col][0],
                    mat[col][1],
                    mat[col][2],
                    mat[col][3]);
    }
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
    glfwSetCursorPosCallback(s_window, mouse_callback);

    glfwSetInputMode(s_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    mgfx_init_info mgfx_info = {"Clear Screen", s_window};

    if (mgfx_init(&mgfx_info) != 0) {
        return -1;
    }

    camera_create(mgfx_camera_type_perspective, &g_example_camera);

    uint8_t default_white_data[] = { 255, 255, 255, 255 };
    texture_create_2d(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_FILTER_NEAREST, &s_default_white);
    image_update(4, default_white_data, &s_default_white.image);

    uint8_t default_black_data[] = {0, 0, 0, 0};
    texture_create_2d(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_FILTER_NEAREST, &s_default_black);
    image_update(4, default_black_data, &s_default_black.image);

    uint8_t default_normal_data[] = { 128, 128, 255, 255 };
    texture_create_2d(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_FILTER_NEAREST, &s_default_normal_map);
    image_update(4, default_normal_data, &s_default_normal_map.image);

    mgfx_example_init();

    while (!glfwWindowShouldClose(s_window)) {
        glfwPollEvents();

        MGFX_TIME = glfwGetTime();
        MGFX_TIME_DELTA_TIME = MGFX_TIME - last_time;
        last_time = MGFX_TIME;

        if(mgfx_get_key(GLFW_KEY_ESCAPE)) {
            break;
        }

        camera_update(&g_example_camera);
        mgfx_example_update();

        mgfx_frame();

        mouse_delta_x = 0.0;
        mouse_delta_y = 0.0;
    }

    mgfx_example_shutdown();

    texture_destroy(&s_default_white);
    texture_destroy(&s_default_black);
    texture_destroy(&s_default_normal_map);

    mgfx_shutdown();
    glfwDestroyWindow(s_window);
    glfwTerminate();
    return 0;
}
