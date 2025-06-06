#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform sampler2D diffuse;

void main() {
	if(texture(diffuse, v_uv).r < 0.0001) {
		discard;
	}
	frag_color = vec4(1.0f) * texture(diffuse, v_uv).r;
}
