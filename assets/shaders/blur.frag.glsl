#version 450

// Gaussian two pass blur

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform post_process {
	vec4 weight;
        float base;
	uint horizontal;
};

layout(set = 0, binding = 1) uniform sampler2D brightness;

void main() {
    vec2 tex_offset = 1.0 / textureSize(brightness, 0); // gets size of single texel
    vec3 result = texture(brightness, v_uv).rgb * base; // current fragment's contribution

    if(horizontal == 1)
    {
        for(int i = 0; i < 4; ++i)
        {
            result += texture(brightness, v_uv + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(brightness, v_uv - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    }
    else
    {
        for(int i = 0; i < 4; ++i)
        {
            result += texture(brightness, v_uv + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(brightness, v_uv - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }

    frag_color = vec4(result, 1.0);
}