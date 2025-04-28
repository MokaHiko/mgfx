#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform sampler2D diffuse;

void main() {
	// if(length(texture(diffuse, v_uv).r) < 0.00001) {
	// 	discard;
	// }

	const float gamma = 2.2;
	vec3 hdrColor = texture(diffuse, v_uv).rgb;

	// reinhard tone mapping
	// vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
	float exposure = 0.25;
	vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);

	// gamma correction 
	mapped = pow(mapped, vec3(1.0 / gamma));

	frag_color = vec4(mapped, 1.0);
}
