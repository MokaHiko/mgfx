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

struct point_light {
    vec3 position;
    vec3 color;
    float intensity;
};

const int POINT_LIGHT_COUNT = 3;
point_light point_lights[] = point_light[](
    // HDR.
    // point_light(vec3(5.0, 5.0, 0.0), vec3(1.0, 1.0, 1.0) * 255.0, 5.0),
    // point_light(vec3(-10.0, 10.0, 4.0), vec3(0.7, 0.3, 0.3) * 255.0, 10.0),
    // point_light(vec3(0.0, -10.0, 5.0), vec3(160.0, 32.0, 240.0), 5.0)

    point_light(vec3(5.0, 5.0, 0.0), vec3(1.0, 1.0, 1.0), 5.0),
    point_light(vec3(-5.0, 5.0, 0.0), vec3(1.0, 1.0, 1.0), 5.0),
    point_light(vec3(0.0, 5.0, 0.0), vec3(1.0, 1.0, 1.0), 5.0)
    // point_light(vec3(-10.0, 10.0, 4.0), vec3(0.7, 0.3, 0.3), 10.0),
    // point_light(vec3(0.0, -10.0, 5.0), vec3(0.4, 0.2, 0.9), 5.0)
);

layout(set = 0, binding = 0) uniform directional_light {
    vec3 direction;
    float _pad1;  // Padding to align vec3 to 16 bytes
    vec3 color;
    float _pad2;  // Padding to align vec3 to 16 bytes
    mat4 light_space_matrix;
} dir_light;

layout(set = 0, binding = 1) uniform sampler2D shadow_map;

layout(set = 1, binding = 0) uniform material {
	vec3 albedo_factor;
	float metallic_factor;
	float roughness_factor;
	float ao_factor;
        float emissive_factor;
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
    float metallic = texture(metallic_roughness_map, v_uv).b;
    float roughness = texture(metallic_roughness_map, v_uv).g;
    float ao = texture(occlusion_map, v_uv).r * ao_factor;
    vec3 emissive = + pow(texture(emissive_map, v_uv).rgb, vec3(2.2)) * emissive_factor * emissive_strength;

    vec3 lo = vec3(0.0f);

    for(int i = 0; i < POINT_LIGHT_COUNT; i++) {
	    vec3 l = normalize(point_lights[i].position - world_position);
	    vec3 h = normalize(v + l);

	    float distance = length(point_lights[i].position - world_position);
	    float attenuation = 1.0f / (distance * distance);
	    vec3 radiance = point_lights[i].color * attenuation;

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
	    lo += (kd * albedo / PI + specular) * radiance * lambertian_factor;
    }

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
	    lo += (kd * albedo / PI + specular) * radiance * lambertian_factor;
    }

    // Shadow
    // perform perspective divide
    vec3 projCoords = frag_pos_light_space.xyz / frag_pos_light_space.w;

    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadow_map, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;
    lo *= (1.0f - shadow);

    // Ambient lighting
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + lo;

    // Emissive
    color += emissive;

    // Gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    frag_color = vec4(color, 1.0f);
}
