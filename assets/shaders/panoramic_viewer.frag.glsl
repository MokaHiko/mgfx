#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform panoramic_data {
        float a;
};

layout(set = 0, binding = 1) uniform sampler2D start_tex;
layout(set = 0, binding = 2) uniform sampler2D end_tex;

void main() {
	frag_color = mix(texture(start_tex, v_uv), texture(end_tex, v_uv), a);
}
