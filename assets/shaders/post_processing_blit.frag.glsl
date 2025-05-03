#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform sampler2D diffuse;
layout(set = 0, binding = 1) uniform sampler2D blur;

// TODO: Make exposure a unform

void main() {
	const float gamma = 2.2;
	vec3 hdr_color = texture(diffuse, v_uv).rgb;

	// bloom
	hdr_color += texture(blur, v_uv).rgb;

	// reinhard tone mapping
	float exposure = 1.0f;
	vec3 mapped = vec3(1.0) - exp(-hdr_color * exposure);

	// gamma correction 
	mapped = pow(mapped, vec3(1.0 / gamma));

	frag_color = vec4(mapped, 1.0);
}
