
#version 460

#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D inTextures[128];

layout (push_constant) uniform PushConstant {
    uint textureIndex;  
};

void main() {
    if (textureIndex == 0) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
    else {
        vec4 textureColor = texture(inTextures[nonuniformEXT(textureIndex)], inUV);
        outColor = vec4(textureColor.xyz, 1.0);
    }
}

