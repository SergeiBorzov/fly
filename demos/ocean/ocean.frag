#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint vertexBufferIndex;
    uint heightMapCascades[4];
    uint diffDisplacementCascades[4];
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

void main()
{
    // float height = 0.0f;
    // float scale = 1.0f;
    // for (int i = 0; i < 2; i++)
    // {
    //     float h = texture(FLY_ACCESS_TEXTURE_BUFFER(
    //                           Textures, gPushConstants.heightMapCascades[i]),
    //                       inUV * scale)
    //                   .r;
    //     height += h / scale;
    //     scale *= 4;
    // }

    outColor = vec4(0.2f, 0.0f, 0.0f, 1.0f);
}
