struct VertexShaderInput {
    float3 localSpacePosition : POSITION;
    float4 vertexColor        : COLOR;
    float2 textureCoordinate  : TEXCOORD;
};

struct PixelShaderInput {
    float4 clipSpacePosition  : SV_POSITION;
    float4 vertexColor        : COLOR;
    float2 textureCoordinate  : TEXCOORD;
};

cbuffer RootConstant : register(b0) {
    uint activeTextureIndex;
};

SamplerState linearClampSampler : register(s0);

// NOTE: Vertex shader
// -------------------------------------------------------------------------

PixelShaderInput VSMain(VertexShaderInput input) {
    PixelShaderInput output;
    
    output.clipSpacePosition = float4(input.localSpacePosition, 1.0f);
    
    output.vertexColor = input.vertexColor;
    output.textureCoordinate = input.textureCoordinate;
    
    return output;
}

// NOTE: Pixel shader
// -------------------------------------------------------------------------

float4 PSMain(PixelShaderInput input) : SV_TARGET {
    Texture2D texture = ResourceDescriptorHeap[activeTextureIndex];
    
    float4 sampledTextureColor = texture.Sample(linearClampSampler, input.textureCoordinate);
    float4 finalResolvedColor = sampledTextureColor * input.vertexColor;
    
    return finalResolvedColor;
}
