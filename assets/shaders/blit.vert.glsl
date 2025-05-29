#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float uv_x;
layout(location = 2) in vec3 normal;
layout(location = 3) in float uv_y;
layout(location = 4) in vec4 color;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_color;
layout(location = 2) out vec2 v_uv;

layout(push_constant) uniform graphics_pc {
	mat4 model;
	mat4 view;
	mat4 proj;

	mat4 model_inv;
	mat4 view_inv;
};

void main() {
	v_normal = (normal.xyz);
	v_color = (color.xyz);
	v_uv = vec2(uv_x, uv_y);
	gl_Position = vec4(position, 1.0f);
}
