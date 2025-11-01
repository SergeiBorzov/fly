#version 460
#extension GL_GOOGLE_include_directive : require
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

void main()
{
    vec3 dir = normalize(inDirection);
    vec3 skyBoxColor = texture(FLY_ACCESS_TEXTURE_BUFFER(
                                   Cubemaps, gPushConstants.skyBoxTextureIndex),
                               dir)
                           .rgb;
    vec3 fogColor = vec3(0.7f, 0.75f, 0.8f);

    float horizonStart = 0.0f;
    float horizonEnd = 0.2f;

    float fogFactor = smoothstep(horizonStart, horizonEnd, dir.y);

    vec3 finalColor = mix(fogColor, skyBoxColor, fogFactor);

    outFragColor = vec4(finalColor, 1.0f);
}
