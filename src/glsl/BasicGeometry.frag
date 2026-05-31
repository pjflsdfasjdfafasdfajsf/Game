
#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D inTextures[128];

layout (push_constant) uniform PushConstant {
    uint textureIndex;  
};

void main() {
    if (textureIndex == 0) {
        outColor = inColor;
    }
    else {
        vec4 textureColor = texture(inTextures[nonuniformEXT(textureIndex - 1)], inUV);
        outColor = textureColor;
    }
}

