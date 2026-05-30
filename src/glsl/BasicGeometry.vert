
#version 460

layout (location = 0) out vec2 outUV;

vec3 vertices[] = vec3[](
    vec3(0.0f, -0.5f, 0.0f),
    vec3(-0.5f, 0.5f, 0.0f),
    vec3(0.5f, 0.5f, 0.0f)
);

vec2 uv[] = vec2[](
    vec2(0.5f, 1.0f),
    vec2(0.0f, 0.0f),
    vec2(1.0f, 0.0f)       
);

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0);

    outUV = uv[gl_VertexIndex];
}
