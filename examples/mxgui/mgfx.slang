module mgfx;

#define MGFX_DEBUG

namespace mgfx {

public static const float PI = 3.14159265359;

public struct Vertex {
    [vk::location(0)] public float3 position : POSITION;
    [vk::location(1)] public float3 normal : NORMAL;
    [vk::location(2)] public float2 uv : TEXCOORD0;
    [vk::location(3)] public float4 color : COLOR;
    [vk::location(4)] public float4 tangent : TANGENT;
}

struct TransformData {
    float4x4 model;
    float4x4 view;
    float4x4 proj;

    float4x4 model_inv;
    float4x4 view_inv;
};

struct Draw {
    uint32_t id;
};

[vk_push_constant]
ConstantBuffer<Draw> draw;

StructuredBuffer<TransformData> transforms;

public float4x4 transform() {
    return transforms[draw.id].model;
}

public float4x4 inverse_transform() {
    return transforms[draw.id].model_inv;
}

public float4x4 view() {
    return transforms[draw.id].view;
}

public float4x4 inverse_view() {
    return transforms[draw.id].view_inv;
}

public float4x4 proj() {
    return transforms[draw.id].proj;
}

public float4x4 view_proj() {
    return mul(proj(), view());
}

}
