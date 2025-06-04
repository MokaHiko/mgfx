import mgfx;
import mgfxMaterial;

struct VOut {
    mgfx::SurfaceGeometry sg;
    float4 color;

    float4 position : SV_Position;
}

struct Scene {
    float3 camera_position;
    int point_light_count;
};

ConstantBuffer<Scene> scene;

ConstantBuffer<mgfx::DirectionalLight> dirLight;
StructuredBuffer<mgfx::PointLight> pointLights;

[shader("vertex")]
VOut vertexMain(mgfx::Vertex input) {
    VOut output;

    float4x4 proj_view = mul(mgfx::u.proj, mgfx::u.view);

    output.sg.normal = normalize(mul(transpose((float3x3)mgfx::u.model_inv), input.normal));
    output.sg.uv = input.uv;

    output.sg.frag_position = mul(mgfx::u.model, float4(input.position, 1.0)).xyz;

    output.position = mul(proj_view, float4(output.sg.frag_position, 1.0f));
    output.color = input.color;

    float3 t = normalize(mul((float3x3)mgfx::u.model, input.tangent.xyz));
    float3 b = input.tangent.w * cross(output.sg.normal, input.tangent.xyz);
    output.sg.TBN = float3x3(t, b, output.sg.normal);

    return output;
}

ConstantBuffer<mgfx::LitMaterial> material;

struct FOut {
    float4 color : SV_Target0;
    float4 bright : SV_Target1;
};

[shader("fragment")]
FOut fragmentMain(VOut input) {
    FOut out;

    mgfx::SurfaceGeometry sg = material.prepare_geometry(input.sg);
    mgfx::IBRDF brdf = material.prepare(sg);

    float3 wo = normalize(scene.camera_position - input.sg.frag_position);

    float3 lo = 0;

    lo += dirLight.illuminate(sg, brdf, wo);

    for (int i = 0; i < scene.point_light_count; i++) {
        lo += pointLights[i].illuminate(sg, brdf, wo);
    }

    lo += material.ambient(sg);
    lo += material.emissive(sg);
    out.color = float4(lo, 1.0f);

    float brightness = dot(out.color.rgb, float3(0.2126, 0.7152, 0.0722)) * length(material.emissive(sg));
    if (brightness > 1.0) {
        out.bright = float4(out.color.rgb, 1.0);
    } else {
        out.bright = 0;
    }

    return out;
}
