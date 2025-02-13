#include "ex_common.h"

#include <mgfx/mgfx.h>
#include <stdio.h>

#include <GLFW/glfw3.h>

GLFWwindow* s_window = NULL;

int mgfx_example_app() {
	if(glfwInit() != GLFW_TRUE) {
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	s_window = glfwCreateWindow(720, 480, "Examples", NULL, NULL);

	uint32_t extension_count = 0;
	const char** extensions;
	extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	for(int i = 0; i < extension_count; i++) {
		printf("%s\n", extensions[i]);
	}

	if(extension_count == 0) {
		if(glfwVulkanSupported() != GLFW_TRUE) {
			printf("Vulkan not supported in machine!\n");
			return 0;
		}
		return 0;
	}

	mgfx_init_info mgfx_info = {
		"Clear Screen",
		s_window,
	};

	if(mgfx_init(&mgfx_info) != 0) {
		return -1;
	}

	while(!glfwWindowShouldClose(s_window)) {
		glfwPollEvents();
	}

	mgfx_shutdown();

	glfwDestroyWindow(s_window);
	glfwTerminate();

	return 0;
}
