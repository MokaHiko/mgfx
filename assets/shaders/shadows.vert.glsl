#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
layout(location = 4) in vec4 tangent;

layout(push_constant) uniform graphics_pc {
	mat4 model;
	mat4 view;
	mat4 proj;

	mat4 model_inv;
	mat4 view_inv;
};

void main() {
	vec3 cam_position = inverse(view)[3].xyz;
	gl_Position = proj * view * model * vec4(position, 1.0f);
}
