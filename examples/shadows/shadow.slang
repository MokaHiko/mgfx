static const float PI = 3.14159265359;

struct MGFXBuiltIn {
    float4x4 model;
    float4x4 view;
    float4x4 proj;

    float4x4 model_inv;
    float4x4 view_inv;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float4 color;
    float4 tangent;
}

[vk::push_constant]
ConstantBuffer<MGFXBuiltIn> MGFX_BUILT_IN;

struct VOut {
    float4 position : SV_Position;
}

[shader("vertex")]
VOut vertexMain(Vertex input) {
    VOut output;

    float4x4 proj_view = mul(MGFX_BUILT_IN.proj, MGFX_BUILT_IN.view);
    output.position = mul(proj_view, mul(MGFX_BUILT_IN.model, float4(input.position, 1.0f)));

    return output;
}