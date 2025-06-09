import mgfx;

struct VOut {
    float4 position : SV_Position;
};

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input) {
    VOut output = {0};
    output.position = mul(mgfx::proj(),
                      mul(mgfx::view(),
                      mul(mgfx::transform(), float4(input.position, 1.0f))));

    return output;
}
