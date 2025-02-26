#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float uv_x;
layout(location = 2) in vec3 normal;
layout(location = 3) in float uv_y;
layout(location = 4) in vec4 color;

layout(location = 0) out vec3 v_color;

layout(push_constant) uniform graphics_pc{
	mat4 model;
	mat4 view;
	mat4 proj;
};

void main() {
	v_color = (color.xyz);
	gl_Position = proj * view * model * vec4(position, 1.0f);
}
