import mgfx;

struct VOut
{
    float4 position : SV_Position;
    float3 normal;
    float2 uv;
    float4 color;
}

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input)
{
    VOut output;
    float4x4 mvp = mul(mgfx::u.proj, mul(mgfx::u.view, mgfx::u.model));
    output.position = mul(mvp, float4(input.position, 1.0));
    output.normal = input.normal;
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VOut input) : SV_Target
{
    return input.color;
}