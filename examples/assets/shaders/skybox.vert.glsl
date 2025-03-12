#version 450

layout(location = 0) in vec3 position;

layout(push_constant) uniform graphics_pc {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 view_inv;
};

void main() {
	gl_Position = vec4(position, 1.0f);
}
