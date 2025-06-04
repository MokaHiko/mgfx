module mgfx;

#define MGFX_DEBUG

namespace mgfx {
public static const float PI = 3.14159265359;

public struct PreDefinedUniforms {
    public float4x4 model;
    public float4x4 view;
    public float4x4 proj;
    public float4x4 model_inv;
    public float4x4 view_inv;
};

[vk::push_constant]
public ConstantBuffer<PreDefinedUniforms> u;

public struct Vertex {
    [vk::location(0)] public float3 position : POSITION;
    [vk::location(1)] public float3 normal : NORMAL;
    [vk::location(2)] public float2 uv : TEXCOORD0;
    [vk::location(3)] public float4 color : COLOR;
    [vk::location(4)] public float4 tangent : TANGENT;
}

}
