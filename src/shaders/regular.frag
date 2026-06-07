#version 450

layout(location = 0) in vec4 fragment_input_color; 
layout(location = 1) in vec2 fragment_input_texture_coordinate;

layout(set = 2, binding = 0) uniform sampler2D diffuse_texture_sampler;

layout(location = 0) out vec4 final_color;

void main() {
    final_color = fragment_input_color * texture(diffuse_texture_sampler, fragment_input_texture_coordinate);
}