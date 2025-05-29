import mgfx;

struct VOut {
    float4 position : SV_Position;
};

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input) {
    VOut output;
    output.position = mul(mgfx::u.proj,
                      mul(mgfx::u.view,
                      mul(mgfx::u.model, float4(input.position, 1.0f))));

    return output;
}
