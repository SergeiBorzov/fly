#version 460
#extension GL_GOOGLE_include_directive : require

#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint srcDepthMapIndex;
    uint srcMipLevel;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Textures, sampler2D)

void main()
{
    float depth = textureLod(FLY_ACCESS_TEXTURE_BUFFER(
                                 Textures, gPushConstants.srcDepthMapIndex),
                             inUV, float(gPushConstants.srcMipLevel))
                      .r;

    outFragColor = vec4(depth, 0.0f, 0.0f, 0.0f);
}
