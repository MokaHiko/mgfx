#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

void main() {
	//frag_color = vec4(v_color, 1.0f);
	frag_color = vec4(1.0f);
}
