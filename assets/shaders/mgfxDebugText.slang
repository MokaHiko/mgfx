import mgfx;

struct VOut
{
    float4 position : SV_Position;
    float2 uv;
}

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input)
{
    VOut output;
    float4x4 mvp = mul(mgfx::view_proj(), mgfx::transform());
    output.position = mul(mvp, float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}

Sampler2D diffuse;

[shader("fragment")]
float4 fragmentMain(VOut input) : SV_Target
{
    if (diffuse.Sample(input.uv).r < 0.0001) {
        discard;
    }

    return float4(1.0f) * diffuse.Sample(input.uv).r;
}