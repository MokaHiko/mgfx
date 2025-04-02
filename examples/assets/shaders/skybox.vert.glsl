#version 450

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 v_tex_coords;

layout(push_constant) uniform graphics_pc {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 view_inv;
};

void main() {
	v_tex_coords = vec3(position);
	gl_Position = proj * view * vec4(position, 1.0f);
}
