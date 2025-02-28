#include "ex_common.h"

#include <mx/mx_memory.h>
#include <mx/mx_file.h>

#include <mgfx/mgfx.h>

#include <GLFW/glfw3.h>
#include <stdlib.h>

GLFWwindow* s_window = NULL;

int load_shader_from_path(const char* file_path, shader_vk* shader) {
	MxArena arena = mx_arena_alloc(MX_MB);

	size_t size;
	if(mx_read_file(file_path, &size, NULL) != MX_SUCCESS) {
		printf("[MGfxError] : Failed to load shader!");
		exit(-1);
	}

	char* shader_code = mx_arena_push(&arena, size);
	mx_read_file(file_path, &size, shader_code);

	shader_create(size, shader_code, shader);

	mx_arena_free(&arena);
	return MX_SUCCESS;
}

static void on_window_resize(GLFWwindow* window, int width, int height) {
	mgfx_reset(width, height);
}

int mgfx_example_app() {
	if(glfwInit() != GLFW_TRUE) {
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	s_window = glfwCreateWindow(720, 480, "Examples", NULL, NULL);

	mgfx_init_info mgfx_info = { "Clear Screen", s_window};

	if(mgfx_init(&mgfx_info) != 0) {
		return -1;
	}

	glfwSetFramebufferSizeCallback(s_window, on_window_resize);

	mgfx_example_init();

	while(!glfwWindowShouldClose(s_window)) {
		mgfx_frame();
		glfwPollEvents();
	}

	mgfx_example_shutdown();
	mgfx_shutdown();

	glfwDestroyWindow(s_window);
	glfwTerminate();
	return 0;
}
