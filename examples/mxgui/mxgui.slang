import mgfx;

struct VOut {
    float4 position : SV_Position;
    float2 uv;
}

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input) {
    VOut output;
    output.position = mul(mgfx::proj(), float4(input.position, 1.0f));
    output.uv = input.uv;
    return output;
}

Sampler2D fs_texture;

[shader("fragment")]
float4 fragmentMain(VOut input) {
	float3 color = fs_texture.Sample(input.uv).rgb;
    return float4(color, 1.0);
}
