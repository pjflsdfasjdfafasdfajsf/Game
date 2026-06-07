#version 450

layout(location = 0) in vec3 vertex_input_position;
layout(location = 1) in vec4 vertex_input_color;
layout(location = 2) in vec2 vertex_input_texture_coordinate;

layout(location = 0) out vec4 vertex_output_color;
layout(location = 1) out vec2 vertex_output_texture_coordinate;

void main() {
    gl_Position = vec4(vertex_input_position, 1.0);
    
    vertex_output_color = vertex_input_color;
    vertex_output_texture_coordinate = vertex_input_texture_coordinate;
}