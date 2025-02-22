#version 450

vec3 vertices[] = {
	{-0.5,  0.5 , 1.0},
	{ 0.0, -0.5 , 1.0},
	{ 0.5,  0.5 , 1.0}
};

void main() {
	gl_Position = vec4(vertices[gl_VertexIndex], 1.0f);
}
