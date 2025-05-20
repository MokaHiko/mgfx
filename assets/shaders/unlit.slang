struct Vertex
{
    float3 position;
    float uvx;
    float3 normal;
    float uvy;
    float4 color;
}

struct MGFXBuiltIn {
	matrix model;
	matrix view;
	matrix proj;
	matrix view_inv;
};

[vk::push_constant]
ConstantBuffer<MGFXBuiltIn> MGFX_BUILT_IN;

struct VOut
{
    float4 position : SV_Position;
    float3 normal;
    float2 uv;
}

[shader("vertex")]
VOut vertexMain(Vertex input)
{
    VOut output;
    output.position = mul(float4(input.position, 1.0), MGFX_BUILT_IN.proj);
    output.normal = input.normal;
    output.uv = float2(input.uvx, input.uvy);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VOut input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}