// This example demonstrates how to define vertex and fragment shader entrypoints.
// This is a compile-only example, so there is no output from the playground.
// You can use the "Compile" button to compile this example to different targets.

struct Vertex
{
    float3 position;
    float uvx;
    float3 normal;
    float uvy;
    float4 color;
}

// struct Material
// {
//     int albedoTextureIndex;
//     int normalTextureIndex;
// }

// struct Scene
// {
//     StructuredBuffer<Material> materials;
//     Texture2D textures[128];
// }

// struct Camera
// {
//     float4x4 mvp;
// }

// ParameterBlock<Scene> scene;
// ConstantBuffer<Camera> camera;
// SamplerState samplerState;

struct MGFXBuiltIn{
	float4x4 model;
	float4x4 view;
	float4x4 proj;
	float4x4 view_inv;
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
    output.position = mul(MGFX_BUILT_IN.proj, float4(input.position, 1.0));
    output.normal = input.normal;
    output.uv = float2(input.uvx, input.uvy);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VOut input) : SV_Target
{
    // let texture = scene.textures[scene.materials[0].albedoTextureIndex];
    // let color = texture.Sample(samplerState, input.uv);
    // return color;
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}