#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color;
layout(location = 4) in vec4 tangent;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_color;

layout(location = 3) out vec3 world_position;
layout(location = 4) out vec4 frag_pos_light_space;
layout(location = 5) out flat vec3 cam_position;
layout(location = 6) out mat3 TBN;

layout(set = 0, binding = 0) uniform directional_light {
	vec3 direction;
	float _pad1;  // Padding to align vec3 to 16 bytes
	vec3 color;
	float _pad2;  // Padding to align vec3 to 16 bytes
	mat4 light_space_matrix;
} dir_light;

layout(push_constant) uniform graphics_pc {
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 view_inv;
};

void main() {
	v_normal = normalize(transpose(inverse(mat3(model))) * normal);
	v_uv = uv;
	v_color = color.xyz;

	vec3 t = normalize(mat3(model) * vec3(tangent));
	vec3 b = tangent.w * cross(v_normal, tangent.xyz);
	TBN = mat3(t, b, v_normal);

	world_position = (model * vec4(position, 1.0f)).xyz;
	frag_pos_light_space = (dir_light.light_space_matrix * vec4(world_position, 1.0f));

	// TODO: send from cpu
	//cam_position = view_inv[3].xyz;
	cam_position = inverse(view)[3].xyz;

	gl_Position = proj * view * model * vec4(position, 1.0f);
}
