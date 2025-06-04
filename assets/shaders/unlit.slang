struct Vertex {
    float3 position;
    float3 normal;
    float uvx;
    float uvy;
    float4 color;
    float4 tangent;
}

struct MGFXBuiltIn {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 view_inv;
};

[vk::push_constant]
ConstantBuffer<MGFXBuiltIn> MGFX_BUILT_IN;

struct VOut {
    float4 position : SV_Position;
    float3 normal;
    float1 testing;
    float2 uv;
    float4 color;

    float3 frag_position;
    nointerpolation float3 cam_position;
}

[shader("vertex")]
VOut vertexMain(Vertex input) {
    VOut output;
    float4x4 proj_view = mul(MGFX_BUILT_IN.proj, MGFX_BUILT_IN.view);

    output.normal = input.normal;
    output.uv = float2(input.uvx, input.uvy);
    output.color = input.color * input.tangent;
    output.frag_position =
        mul(MGFX_BUILT_IN.model, float4(input.position, 1.0)).xyz;

    output.position = mul(proj_view, float4(output.frag_position, 1.0f));
    // output.cam_position = view_inv; // TODO: calc in cpu
    return output;
}

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

ConstantBuffer<PBRMaterial> material;
Sampler2D diffuse;

[shader("fragment")]
float4 fragmentMain(VOut input) : SV_Target {
    return diffuse.Sample(input.uv) * float4(material.albedo_factor, 1.0f);
}