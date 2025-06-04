import mgfx;

struct VOut {
    float4 position : SV_Position;
    float2 uv;
}

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input) {
    VOut output;
    output.position = float4(input.position.xy, 0.01f, 1.0f);
    output.uv = input.uv;
    return output;
}

Sampler2D fs_texture;
Sampler2D blur_map;

[shader("fragment")]
float4 fragmentMain(VOut input) {
    const float gamma = 2.2;
	float3 hdr_color = fs_texture.Sample(input.uv).rgb;

	// bloom
	hdr_color += blur_map.Sample(input.uv).rgb;

	// reinhard tone mapping
	float exposure = 0.25;
	float3 mapped = float3(1.0) - exp(-hdr_color * exposure);

	// gamma correction 
	mapped = pow(mapped, float3(1.0 / gamma));

    return float4(mapped, 1.0);
}
