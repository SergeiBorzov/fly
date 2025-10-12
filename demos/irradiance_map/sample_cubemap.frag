#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec3 inDirection;

layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

vec3 Reinhard(vec3 hdr, float exposure)
{
    vec3 l = hdr * exposure;
    return l / (l + 1.0f);
}

void main()
{
    vec3 dir = normalize(inDirection);
    vec3 skyBoxColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                   Cubemaps, gPushConstants.skyBoxTextureIndex),
                               dir)
                           .rgb;
    outFragColor = vec4(Reinhard(skyBoxColor, 1.0f), 1.0f);
}
