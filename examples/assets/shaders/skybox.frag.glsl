#version 450

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform sampler2D cube_map;

void main() {
    vec3 color = vec3(0.0f);

    // Gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    //frag_color = vec4(color, 1.0f);
    frag_color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
}
