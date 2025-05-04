#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_color;

layout(location = 3) in vec3 world_position;
layout(location = 4) in vec4 frag_pos_light_space;
layout(location = 5) in flat vec3 cam_position;
layout(location = 6) in mat3 TBN;

layout(location = 0) out vec4 frag_color;

const float PI = 3.14159265359;

layout(set = 0, binding = 0) uniform directional_light {
    mat4 light_space_matrix;
    vec3 direction;
    float distance;  // Padding to align vec3 to 16 bytes
    vec3 color;
    float _pad2;    // Padding to align vec3 to 16 bytes
} dir_light;

layout(set = 0, binding = 1) uniform sampler2D shadow_map;

layout(set = 1, binding = 0) uniform material {
	vec3 albedo_factor;
	float metallic_factor;

	float roughness_factor;
	float ao_factor;
	float normal_factor;
	float padding;

        vec3 emissive_factor;
        float emissive_strength;
};

layout(set = 1, binding = 1) uniform sampler2D albedo_map;
layout(set = 1, binding = 2) uniform sampler2D metallic_roughness_map;
layout(set = 1, binding = 3) uniform sampler2D normal_map;
layout(set = 1, binding = 4) uniform sampler2D occlusion_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;

vec3 fresnel_schlick(float cos_theta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_GGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometry_schlick_ggx(NdotV, roughness);
    float ggx1  = geometry_schlick_ggx(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main() {
    vec3 n = texture(normal_map, v_uv).xyz * 2.0 - 1.0;
    n = normalize(TBN * n);

    vec3 v = normalize(cam_position - world_position);

    vec3 albedo = pow(texture(albedo_map, v_uv).rgb, vec3(2.2));
    albedo *= albedo_factor;

    float metallic = texture(metallic_roughness_map, v_uv).b;
    float roughness = texture(metallic_roughness_map, v_uv).g;
    float ao = texture(occlusion_map, v_uv).r * ao_factor;
    vec3 emissive = + pow(texture(emissive_map, v_uv).rgb, vec3(2.2)) * emissive_factor * emissive_strength;

    vec3 lo = vec3(0.0f);

    // Directional light
    {
	vec3 l = normalize(-dir_light.direction);
	vec3 h = normalize(v + l);

	vec3 radiance = dir_light.color;

	// Specular factor
	vec3 f0 = vec3(0.04); // surface reflection at 0
	f0 = mix(f0, albedo, metallic);
	vec3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);

	float ndf = distribution_GGX(n, h, roughness);
	float g = geometry_smith(n, v, l, roughness);

	vec3 numerator = ndf * g * f;
	float denominator = 4 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0)  + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 ks = f;
	vec3 kd = vec3(1.0f) - ks;
	kd *= (1.0f - metallic);

	float lambertian_factor = max(dot(n,l), 0.0f);

	// Shadow
	// perform perspective divide
	vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

	// Store before normalizing
	// get depth of current fragment from light's perspective // TODO: VULKAN ALREADY [0,1]
	float current_depth = proj_coords.z;

	// get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
	proj_coords = proj_coords * 0.5f + 0.5f;

	float shadow = 0.0f;

	vec2 texel_size = 1.0f / textureSize(shadow_map, 0);
	for(int x = -1; x <= 1; ++x) {
	    for(int y = -1; y <= 1; ++y) {
		float pcf_depth = texture(shadow_map, vec2(proj_coords.x, 1 - proj_coords.y) + texel_size * vec2(x,y)).r;
		float bias = 0.0005f;
		shadow += current_depth - bias > pcf_depth  ? 1.0f : 0.0f;
	    }
	}
	shadow /= 9.0f;

	if(current_depth > 1.0f) {
	    shadow = 0.0f;
	}

	lo += (kd * albedo / PI + specular) * radiance * lambertian_factor * (1.0f - shadow);
    }

    // Ambient lighting
    vec3 ambient = vec3(0.001) * albedo * ao;
    vec3 color = ambient + lo;

    // Emissive
    color += emissive;

    frag_color = vec4(color, 1.0f);
}
