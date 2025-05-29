import mgfx;

struct PBRMaterial {
    float3 albedo_factor;
    float metallic_factor;

    float roughness_factor;
    float ao_factor;
    float normal_factor;
    float padding;

    float3 emissive_factor;
    float emissive_strength;
};

float3 fresnel_schlick(float cos_theta, float3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_GGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = mgfx::PI * denom * denom;

    return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

struct VOut {
    float4 position : SV_Position;
    float3 normal;
    float2 uv;
    float4 color;

    float3 frag_position;
    nointerpolation float3 cam_position;
    float3x3 TBN;
}

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input) {
    VOut output;
    float4x4 proj_view = mul(mgfx::u.proj, mgfx::u.view);

    output.normal = normalize(
        mul(transpose((float3x3)mgfx::u.model_inv), input.normal));
    output.uv = input.uv;
    output.color = input.color * input.tangent;
    output.frag_position =
        mul(mgfx::u.model, float4(input.position, 1.0)).xyz;

    output.position = mul(proj_view, float4(output.frag_position, 1.0f));
    output.cam_position = mgfx::u.view_inv[3].xyz;

    float3 t = normalize(mul((float3x3)mgfx::u.model, input.tangent.xyz));
    float3 b = input.tangent.w * cross(output.normal, input.tangent.xyz);
    output.TBN = float3x3(t, b, output.normal);
    return output;
}

struct directional_light {
    float3 direction;
    float3 color;
};

static const int DIR_LIGHT_COUNT = 1;
static const directional_light directional_lights[] = { { float3(1.0, -1.0, 0.0),
                                             float3(1.0, 1.0f, 1.0) } };

ConstantBuffer<PBRMaterial> material;

Sampler2D diffuse;
Sampler2D metallic_roughness_map;
Sampler2D normal_map;
Sampler2D occlusion_map;
Sampler2D emissive_map;

[shader("fragment")]
float4 fragmentMain(VOut input) : SV_Target {
    float3 n = normal_map.Sample(input.uv).xyz * 2.0 - 1.0;
    n = normalize(mul(input.TBN, n));

    float3 v = normalize(input.cam_position - input.frag_position);

    float3 albedo = pow(diffuse.Sample(input.uv).rgb, float3(2.2));
    float metallic = metallic_roughness_map.Sample(input.uv).b;
    float roughness = metallic_roughness_map.Sample(input.uv).g;
    float ao = occlusion_map.Sample(input.uv).r * material.ao_factor;
    float3 emissive = pow(emissive_map.Sample(input.uv).rgb, float3(2.2)) *
                      material.emissive_factor * material.emissive_strength;

    float3 lo = float3(0.0f);

    for (int i = 0; i < DIR_LIGHT_COUNT; i++) {
        float3 l = normalize(-directional_lights[i].direction);
        float3 h = normalize(v + l);

        float3 radiance = directional_lights[i].color;

        // Specular factor
        float3 f0 = float3(0.04); // surface reflection at 0
        f0 = lerp(f0, albedo, metallic);
        float3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);

        float ndf = distribution_GGX(n, h, roughness);
        float g = geometry_smith(n, v, l, roughness);

        float3 numerator = ndf * g * f;
        float denominator =
            4 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float3 ks = f;
        float3 kd = float3(1.0f) - ks;
        kd *= (1.0f - metallic);

        float lambertian_factor = max(dot(n, l), 0.0f);
        lo += (kd * albedo / mgfx::PI + specular) * radiance * lambertian_factor;
    }

    // Ambient lighting
    float3 ambient = float3(0.03) * albedo * ao;
    float3 color = ambient + lo;

    // Emissive 
    color += emissive;

    return float4(color.xyz, 1.0);
}