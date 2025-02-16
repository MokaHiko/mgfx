#include "ex_common.h"

ShaderVk vertex_shader;
ShaderVk fragment_shader;

void mgfx_example_init() {
	load_shader_from_path("assets/shaders/triangle.vert.spv", &vertex_shader);
	load_shader_from_path("assets/shaders/triangle.frag.spv", &fragment_shader);
}

void mgfx_example_shutdown() {
	shader_destroy(&fragment_shader);
	shader_destroy(&vertex_shader);
}

int main() {
	mgfx_example_app();
}
