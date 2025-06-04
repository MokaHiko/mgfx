module mgfxMaterial;

import mgfx;

namespace mgfx {

// The first building block in our model will be the representation of
// the geometry attributes of a surface as fed into the material.
//
public struct SurfaceGeometry {
    public float3 frag_position;
    public float3 frag_light_pos;

    public float3 normal;
    public float3x3 TBN;

    public float3 tangentU;
    public float3 tangentV;

    public float2 shadow_map_uv;
    public float2 uv;
};

public interface IBRDF {
    // Technically, a BRDF is only a function of the incident
    // (`wi`) and exitant (`wo`) directions, but for simplicity
    // we are passing in the surface normal (`N`) as well.
    //
    float3 evaluate(float3 wo, float3 wi, float3 N);
};

public struct CookTorrenceBRDF : IBRDF {
    float3 albedo;
    float metallic;
    float roughness;

    public float3 evaluate(float3 wo, float3 wi, float3 n) {
        float3 h = normalize(wo + wi);

        // Specular factor
        float3 f0 = float3(0.04, 0.04, 0.04); // surface reflection at 0
        f0 = lerp(f0, albedo, metallic);
        float3 f = fresnel_schlick(max(dot(h, wo), 0.0), f0);

        float ndf = distribution_GGX(n, h, roughness);
        float g = geometry_smith(n, wo, wi, roughness);

        float3 numerator = ndf * g * f;
        float denominator = 4 * max(dot(n, wo), 0.0) * max(dot(n, wi), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        float3 ks = f;
        float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
        kd *= (1.0f - metallic);

        float lambertian_factor = max(dot(n, wi), 0.0f);
        return (kd * albedo / PI + specular) * lambertian_factor;
    }

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
};

// It is important to note that a reflectance function is *not*
// a "material." In most cases, a material will have spatially-varying
// properties so that it cannot be summarized as a single `IBRDF`
// instance.
//
// Thus a "material" is a value that can produce a BRDF for any point
// on a surface (e.g., by sampling texture maps, etc.).
//
public interface IMaterial {
    public associatedtype BRDF : IBRDF;

    // For our simple example program, it is enough for a material to
    // be able to return a BRDF given a point on the surface.
    //
    // A more complex implementation of material shading might also
    // have the material return updated surface geometry to reflect
    // the result of normal mapping, occlusion mapping, etc. or
    // return an opacity/coverage value for partially transparent
    // surfaces.
    //
    public BRDF prepare(SurfaceGeometry sg);
    public SurfaceGeometry prepare_geometry(SurfaceGeometry sg);

    public float3 emissive(SurfaceGeometry sg);
    public float3 ambient(SurfaceGeometry sg);
};

public struct LitMaterial : IMaterial {
    float3 albedo_factor;
    float metallic_factor;
    float roughness_factor;
    float ao_factor;
    float normal_factor;
    float3 emissive_factor;
    float emissive_strength;

    Sampler2D albedo;
    Sampler2D metallic_roughness_map;
    Sampler2D normal_map;
    Sampler2D occlusion_map;
    Sampler2D emissive_map;

    public typedef CookTorrenceBRDF BRDF;

    public SurfaceGeometry prepare_geometry(SurfaceGeometry sg) {
        float3 n = normal_map.Sample(sg.uv).xyz * 2.0 - 1.0;
        sg.normal = normalize(mul(sg.TBN, n));

        return sg;
    }

    public CookTorrenceBRDF prepare(SurfaceGeometry sg) {
        CookTorrenceBRDF brdf;

        brdf.albedo = pow(albedo.Sample(sg.uv).rgb, float3(2.2));
        brdf.albedo *= albedo_factor;

        brdf.metallic = metallic_roughness_map.Sample(sg.uv).b;
        brdf.roughness = metallic_roughness_map.Sample(sg.uv).g;

        return brdf;
    }

    public float3 emissive(SurfaceGeometry sg) {
        return pow(emissive_map.Sample(sg.uv).rgb, float3(2.2)) *
                emissive_factor * emissive_strength;
    }

    public float3 ambient(SurfaceGeometry sg) {
        // Ambient & AO lighting
        float ao = occlusion_map.Sample(sg.uv).r * ao_factor;
        return mul(float3(0.00003), albedo.Sample(sg.uv).rgb) * ao;
    }
};

// A light, or an entire lighting *environment* is an object
// that can illuminate a surface using some BRDF implemented
// with our abstractions above.
//
public interface ILightEnv {
    // The `illuminate` method is intended to integrate incoming
    // illumination from this light (environment) incident at the
    // surface point given by `g` (which has the reflectance function
    // `brdf`) and reflected into the outgoing direction `wo`.
    //
    float3 illuminate<B : IBRDF>(SurfaceGeometry g, B brdf, float3 wo);
    //
    // Note that the `illuminate()` method is allowed as an interface
    // requirement in Slang even though it is a generic. Contrast that
    // with C++ where a `template` method cannot be `virtual`.
};

// Given the `ILightEnv` interface, we can write up almost textbook
// definition of directional and point lights.

public struct DirectionalLight : ILightEnv {
    float4x4 lightSpaceMatrix;
    float4 direction;
    float4 intensity;

    public Sampler2D shadow_map;

    public float3 illuminate<B : IBRDF>(SurfaceGeometry g, B brdf, float3 wo) {
        // Shadow
	    float4 frag_pos_light_space = mul(lightSpaceMatrix, float4(g.frag_position, 1.0f));

        // perform perspective divide
        float3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

        // Store before normalizing
        // get depth of current fragment from light's perspective // TODO: VULKAN ALREADY [0,1]
        float current_depth = proj_coords.z;

        // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
        proj_coords = proj_coords * 0.5f + 0.5f;

        float shadow = 0.0f;

        uint width, height, levels;
        shadow_map.GetDimensions(0, width, height, levels);
        float2 texel_size = 1.0f / float2(width, height);
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                float pcf_depth = shadow_map.Sample(float2(proj_coords.x, 1 - proj_coords.y) + texel_size * float2(x, y)).r;
                float bias = 0.0005f;
                shadow += current_depth - bias > pcf_depth ? 1.0f : 0.0f;
            }
        }
        shadow /= 9.0f;

        if (current_depth > 1.0f) {
            shadow = 0.0f;
        }

        return intensity.rgb * brdf.evaluate(wo, -direction.xyz, g.normal) * (1.0f - shadow);
    }
};

public struct PointLight : ILightEnv {
    float4 position;
    float4 intensity;

    public float3 illuminate<B : IBRDF>(SurfaceGeometry g, B brdf, float3 wo) {
        float3 delta = position.xyz - g.frag_position;
        float d = length(delta);
        float3 direction = normalize(delta);
        float3 illuminance = intensity.rgb / (d * d);
        return illuminance * brdf.evaluate(wo, -direction, g.normal);
    }
};

}
