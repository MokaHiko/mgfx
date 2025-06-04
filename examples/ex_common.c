#include "ex_common.h"

#include "mx/mx_log.h"

#include <mx/mx_asserts.h>
#include <mx/mx_file.h>
#include <mx/mx_math_mtx.h>
#include <mx/mx_memory.h>

#include <mgfx/mgfx.h>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan_core.h>

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
mgfx_th s_default_white;
mgfx_th s_default_black;
mgfx_th s_default_normal_map;

mgfx_sh MGFX_GIZMO_VSH;
mgfx_sh MGFX_GIZMO_FSH;
mgfx_ph MGFX_GIZMO_PH;

mgfx_vbh MGFX_DEFAULT_CUBE_VBH;
mgfx_ibh MGFX_DEFAULT_CUBE_IBH;

mx_bool mgfx_get_key(int key_code) { return s_keys[key_code]; }

float mgfx_get_axis(mgfx_input_axis axis) {
    switch (axis) {
    case MGFX_INPUT_AXIS_MOUSE_X:
        return mouse_delta_x / (double)APP_WIDTH;
    case MGFX_INPUT_AXIS_MOUSE_Y:
        return mouse_delta_y / (double)APP_HEIGHT;
    default:
        return 0.0f;
    }
}

typedef struct vertex {
    float position[3];
    float uv_x;
    float normal[3];
    float uv_y;
    float color[4];
} vertex;

static mgfx_pntuc32f k_cube_vertices[] = {
    // Front face
    {{-0.5f, -0.5f, 0.5f},
     0.0f,
     {0.0f, 0.0f, 1.0f},
     0.0f,
     {1.0f, 0.0f, 0.0f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, 0.5f},
     1.0f,
     {0.0f, 0.0f, 1.0f},
     0.0f,
     {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{0.5f, 0.5f, 0.5f},
     1.0f,
     {0.0f, 0.0f, 1.0f},
     1.0f,
     {0.0f, 0.0f, 1.0f, 1.0f}}, // Top-right
    {{-0.5f, 0.5f, 0.5f},
     0.0f,
     {0.0f, 0.0f, 1.0f},
     1.0f,
     {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Back face
    {{0.5f, -0.5f, -0.5f},
     0.0f,
     {0.0f, 0.0f, -1.0f},
     0.0f,
     {1.0f, 0.0f, 1.0f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, -0.5f},
     1.0f,
     {0.0f, 0.0f, -1.0f},
     0.0f,
     {0.0f, 1.0f, 1.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, -0.5f},
     1.0f,
     {0.0f, 0.0f, -1.0f},
     1.0f,
     {1.0f, 1.0f, 1.0f, 1.0f}}, // Top-right
    {{0.5f, 0.5f, -0.5f},
     0.0f,
     {0.0f, 0.0f, -1.0f},
     1.0f,
     {0.5f, 0.5f, 0.5f, 1.0f}}, // Top-left

    // Left face
    {{-0.5f, -0.5f, -0.5f},
     0.0f,
     {-1.0f, 0.0f, 0.0f},
     0.0f,
     {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{-0.5f, -0.5f, 0.5f},
     1.0f,
     {-1.0f, 0.0f, 0.0f},
     0.0f,
     {0.5f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{-0.5f, 0.5f, 0.5f},
     1.0f,
     {-1.0f, 0.0f, 0.0f},
     1.0f,
     {0.0f, 0.5f, 1.0f, 1.0f}}, // Top-right
    {{-0.5f, 0.5f, -0.5f},
     0.0f,
     {-1.0f, 0.0f, 0.0f},
     1.0f,
     {1.0f, 1.0f, 0.5f, 1.0f}}, // Top-left

    // Right face
    {{0.5f, -0.5f, 0.5f},
     0.0f,
     {1.0f, 0.0f, 0.0f},
     0.0f,
     {1.0f, 0.5f, 0.0f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f},
     1.0f,
     {1.0f, 0.0f, 0.0f},
     0.0f,
     {0.5f, 0.5f, 1.0f, 1.0f}}, // Bottom right
    {{0.5f, 0.5f, -0.5f},
     1.0f,
     {1.0f, 0.0f, 0.0f},
     1.0f,
     {0.5f, 0.0f, 1.0f, 1.0f}}, // Top-right
    {{0.5f, 0.5f, 0.5f},
     0.0f,
     {1.0f, 0.0f, 0.0f},
     1.0f,
     {1.0f, 1.0f, 1.0f, 1.0f}}, // Top-left

    // Top face
    {{-0.5f, 0.5f, 0.5f},
     0.0f,
     {0.0f, 1.0f, 0.0f},
     0.0f,
     {1.0f, 0.0f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, 0.5f, 0.5f},
     1.0f,
     {0.0f, 1.0f, 0.0f},
     0.0f,
     {0.5f, 1.0f, 0.5f, 1.0f}}, // Bottom-right
    {{0.5f, 0.5f, -0.5f},
     1.0f,
     {0.0f, 1.0f, 0.0f},
     1.0f,
     {0.0f, 1.0f, 1.0f, 1.0f}}, // Top-right
    {{-0.5f, 0.5f, -0.5f},
     0.0f,
     {0.0f, 1.0f, 0.0f},
     1.0f,
     {1.0f, 1.0f, 0.0f, 1.0f}}, // Top-left

    // Bottom face
    {{-0.5f, -0.5f, -0.5f},
     0.0f,
     {0.0f, -1.0f, 0.0f},
     0.0f,
     {1.0f, 0.5f, 0.5f, 1.0f}}, // Bottom-left
    {{0.5f, -0.5f, -0.5f},
     1.0f,
     {0.0f, -1.0f, 0.0f},
     0.0f,
     {0.5f, 1.0f, 0.0f, 1.0f}}, // Bottom-right
    {{0.5f, -0.5f, 0.5f},
     1.0f,
     {0.0f, -1.0f, 0.0f},
     1.0f,
     {1.0f, 0.0f, 0.5f, 1.0f}}, // Top-right
    {{-0.5f, -0.5f, 0.5f},
     0.0f,
     {0.0f, -1.0f, 0.0f},
     1.0f,
     {0.5f, 0.5f, 1.0f, 1.0f}}, // Top-left
};
static const size_t k_cube_vertex_count = sizeof(k_cube_vertices) / sizeof(mgfx_pntuc32f);

static uint32_t k_cube_indices[] = {
    // Front face
    0,
    1,
    2,
    0,
    2,
    3,
    // Back face
    4,
    5,
    6,
    4,
    6,
    7,
    // Left face
    8,
    9,
    10,
    8,
    10,
    11,
    // Right face
    12,
    13,
    14,
    12,
    14,
    15,
    // Top face
    16,
    17,
    18,
    16,
    18,
    19,
    // Bottom face
    20,
    21,
    22,
    20,
    22,
    23};

static const size_t k_cube_index_count = sizeof(k_cube_indices) / sizeof(uint32_t);

mgfx_th load_texture_2d_from_path(const char* path, mgfx_format format) {
    int width, height, channel_count;

    stbi_uc* data = stbi_load(path, &width, &height, &channel_count, STBI_rgb_alpha);

    MX_ASSERT(data, "Texture at %s failed to load", path);
    MX_ASSERT(data != NULL, "[MGFX_EXAMPLES]: Failed to load texture!");

    size_t size = width * height * 4;
    MX_LOG_TRACE("Texture: %s (%.2f mb)!", path, (float)size / MX_MB);

    mgfx_image_info info = {
        .format = format,

        .width = width,
        .height = height,
        .layers = 1,

        .cube_map = MX_FALSE,
    };

    mgfx_th th = mgfx_texture_create_from_memory(&info, VK_FILTER_LINEAR, data, size);

    stbi_image_free(data);
    return th;
}

static void window_resize_callback(GLFWwindow* window, int width, int height) {
    mgfx_reset(width, height);
}

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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
                    .forward = {0.0f, 0.0f, -1.0f},
                    .up = {0.0f, 1.0f, 0.0f},
                    .right = {1.0f, 0.0f, 0.0f}};

    real_t ortho_s = 25.0f;
    real_t inverse_fov = (9.0f / 16.0f);
    switch (type) {
    case mgfx_camera_type_orthographic:
        cam->proj = mx_ortho(-ortho_s,
                             ortho_s,
                             inverse_fov * -ortho_s,
                             inverse_fov * ortho_s,
                             -0.001f,
                             1000.0f);
        break;
    case mgfx_camera_type_perspective:
        cam->proj = mx_perspective(MX_DEG_TO_RAD(60.0), 16.0 / 9.0, 0.1f, 1000.0f);
        break;
    }

    cam->yaw = -90.0f;
    cam->pitch = 0.0f;
}

void camera_update(camera* cam) {
    cam->right = mx_vec3_cross(cam->forward, MX_VEC3_UP);
    cam->right = mx_vec3_norm(cam->right);

    mx_vec3 look_at = mx_vec3_add(cam->position, cam->forward);
    cam->view = mx_look_at(cam->position, look_at, cam->up);

    // cam->inverse_view = mx_mat4_inv(cam->view);
    cam->view_proj = mx_mat4_mul(cam->proj, cam->view);
}

void editor_update(camera* cam) {
    float editor_sens = 100.0f;
    cam->yaw += mgfx_get_axis(MGFX_INPUT_AXIS_MOUSE_X) * editor_sens;
    cam->pitch -= mgfx_get_axis(MGFX_INPUT_AXIS_MOUSE_Y) * editor_sens;
    cam->pitch = mx_clamp(cam->pitch, -89.0f, 89.0f);

    mx_vec3 direction;
    direction.x = cos(MX_DEG_TO_RAD(cam->yaw)) * cos(MX_DEG_TO_RAD(cam->pitch));
    direction.y = sin(MX_DEG_TO_RAD(cam->pitch));
    direction.z = sin(MX_DEG_TO_RAD(cam->yaw)) * cos(MX_DEG_TO_RAD(cam->pitch));
    direction = mx_vec3_norm(direction);
    cam->forward = direction;

    mx_vec3 input = {0, 0, 0};
    if (mgfx_get_key(GLFW_KEY_W) == MX_TRUE) {
        input = mx_vec3_add(input, cam->forward);
    }

    if (mgfx_get_key(GLFW_KEY_S) == MX_TRUE) {
        input = mx_vec3_sub(input, cam->forward);
    }

    if (mgfx_get_key(GLFW_KEY_D) == MX_TRUE) {
        input = mx_vec3_add(input, cam->right);
    }

    if (mgfx_get_key(GLFW_KEY_A) == MX_TRUE) {
        input = mx_vec3_sub(input, cam->right);
    }

    if (mgfx_get_key(GLFW_KEY_SPACE) == MX_TRUE) {
        input = mx_vec3_add(input, cam->up);
    }

    if (mgfx_get_key(GLFW_KEY_Q) == MX_TRUE) {
        input = mx_vec3_sub(input, cam->up);
    }

    if (mx_vec3_eqv(input, MX_VEC3_ZERO)) {
        return;
    }

    double ms = 5.0f;

    if (!mx_vec3_eqv(input, MX_VEC3_ZERO)) {
        input = mx_vec3_norm(input);
    }

    input = mx_vec3_scale(input, ms * MGFX_TIME_DELTA_TIME);
    cam->position = mx_vec3_add(cam->position, input);
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

    mgfx_init_info mgfx_info = {"Clear Screen", s_window};

    if (mgfx_init(&mgfx_info) != 0) {
        return -1;
    }

    camera_create(mgfx_camera_type_perspective, &g_example_camera);

    const mgfx_image_info texture_info = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .width = 1,
        .height = 1,
        .layers = 1,
        .cube_map = MX_FALSE,
    };

    uint8_t default_white_data[] = {255, 255, 255, 255};
    s_default_white = mgfx_texture_create_from_memory(&texture_info,
                                                      VK_FILTER_NEAREST,
                                                      default_white_data,
                                                      4);

    uint8_t default_black_data[] = {0, 0, 0, 0};
    s_default_black = mgfx_texture_create_from_memory(&texture_info,
                                                      VK_FILTER_NEAREST,
                                                      default_black_data,
                                                      4);

    uint8_t default_normal_data[] = {128, 128, 255, 255};
    s_default_normal_map = mgfx_texture_create_from_memory(&texture_info,
                                                           VK_FILTER_NEAREST,
                                                           default_normal_data,
                                                           4);

    // Init gizmos
    mgfx_vertex_layout gizmo_vl = {0};
    MGFX_DEFAULT_CUBE_VBH =
        mgfx_vertex_buffer_create(k_cube_vertices, sizeof(k_cube_vertices), &gizmo_vl);
    MGFX_DEFAULT_CUBE_IBH =
        mgfx_index_buffer_create(k_cube_indices, sizeof(k_cube_indices));

    MGFX_GIZMO_VSH = mgfx_shader_create(MGFX_ASSET_PATH "shaders/gizmos.vert.glsl.spv");
    MGFX_GIZMO_FSH = mgfx_shader_create(MGFX_ASSET_PATH "shaders/gizmos.frag.glsl.spv");

    MGFX_GIZMO_PH =
        mgfx_program_create_graphics(MGFX_GIZMO_VSH,
                                     MGFX_GIZMO_FSH,
                                     {
                                         .polygon_mode = MGFX_LINE,
                                         .primitive_topology = MGFX_TRIANGLE_LIST,
                                     });

    mgfx_example_init();

    while (!glfwWindowShouldClose(s_window)) {
        glfwPollEvents();

        MGFX_TIME = glfwGetTime();
        MGFX_TIME_DELTA_TIME = MGFX_TIME - last_time;
        last_time = MGFX_TIME;

        if (mgfx_get_key(GLFW_KEY_LEFT_CONTROL)) {
            if (mgfx_get_key(GLFW_KEY_Q)) {
                break;
            }
        }

        if (mgfx_get_key(GLFW_KEY_LEFT_SHIFT) || mgfx_get_key(GLFW_KEY_RIGHT_SHIFT)) {
            glfwSetInputMode(s_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            editor_update(&g_example_camera);

            // TODO: Add quit ui button
            if (mgfx_get_key(GLFW_KEY_ESCAPE)) {
                MX_LOG_INFO("Attempt quit");
                break;
            }
        } else {
            glfwSetInputMode(s_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        camera_update(&g_example_camera);

        mgfx_example_update();

        mgfx_frame();

        mouse_delta_x = 0.0;
        mouse_delta_y = 0.0;
    }

    // Destroy gizmos
    mgfx_buffer_destroy(MGFX_DEFAULT_CUBE_IBH.idx);
    mgfx_buffer_destroy(MGFX_DEFAULT_CUBE_VBH.idx);

    mgfx_program_destroy(MGFX_GIZMO_PH);
    mgfx_shader_destroy(MGFX_GIZMO_VSH);
    mgfx_shader_destroy(MGFX_GIZMO_FSH);

    // Destroy default
    mgfx_texture_destroy(s_default_white, MX_TRUE);
    mgfx_texture_destroy(s_default_black, MX_TRUE);
    mgfx_texture_destroy(s_default_normal_map, MX_TRUE);

    mgfx_example_shutdown();

    mgfx_shutdown();
    glfwDestroyWindow(s_window);
    glfwTerminate();
    return 0;
}

// TODO: Candidate functions
void mgfx_gizmo_draw_cube(const float* view,
                          const float* proj,
                          const mx_vec3 position,
                          const mx_quat rotation,
                          const mx_vec3 scale) {
    mgfx_set_proj(proj);
    mgfx_set_view(view);

    mgfx_set_transform(mx_translate(position).val);

    mgfx_bind_vertex_buffer(MGFX_DEFAULT_CUBE_VBH);
    mgfx_bind_index_buffer(MGFX_DEFAULT_CUBE_IBH);

    mgfx_submit(MGFX_DEFAULT_VIEW_TARGET, MGFX_GIZMO_PH);
}
