#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_color;
layout(location = 2) out vec2 v_uv;

layout(push_constant) uniform graphics_pc {
	mat4 model;
	mat4 view;
	mat4 proj;
};

void main() {
	v_normal = normalize(transpose(inverse(mat3(model))) * normal);

	v_color = color.xyz;
	v_uv = uv;

	gl_Position = proj * view * model * vec4(position, 1.0f);
}
