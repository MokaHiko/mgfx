import mgfx;

struct VOut {
    float4 position : SV_Position;
    float2 uv;
}

// Post process settings
struct PostProcessSettings {
    float4 weight;
    float base;
    uint horizontal;
};

ConstantBuffer<PostProcessSettings> settings;
Sampler2D brightness;

[shader("fragment")]
float4 fragmentMain(VOut input) {
    uint width, height, levels;
    brightness.GetDimensions(0, width, height, levels);
    float2 tex_offset = 1.0 / float2(width, height); // gets size of single texel

    float3 result = brightness.Sample(input.uv).rgb * settings.base; // current fragment's contribution

    if (settings.horizontal == 1)
    {
        for (int i = 0; i < 4; ++i)
        {
            result += brightness.Sample( input.uv + float2(tex_offset.x * i, 0.0)).rgb * settings.weight[i];
            result += brightness.Sample( input.uv - float2(tex_offset.x * i, 0.0)).rgb * settings.weight[i];
        }
    }
    else
    {
        for (int i = 0; i < 4; ++i)
        {
            result += brightness.Sample( input.uv + float2(0.0, tex_offset.y * i)).rgb * settings.weight[i];
            result += brightness.Sample( input.uv - float2(0.0, tex_offset.y * i)).rgb * settings.weight[i];
        }
    }

    return float4(result, 1.0);
}