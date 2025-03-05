#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_color;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform sampler2D diffuse;

void main() {
	frag_color = texture(diffuse, v_uv);
}
