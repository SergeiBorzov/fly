#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

FLY_REGISTER_UNIFORM_BUFFER(UniformData, {
    mat4 projection;
    mat4 view;
    vec4 fetchSpeedDirSpread;
    vec4 domainTimePeak;
    vec4 screenSize;
})

layout(push_constant) uniform PushConstants
{
    uint uniformBufferIndex;
    uint skyBoxTextureIndex;
}
gPushConstants;

FLY_REGISTER_TEXTURE_BUFFER(Cubemaps, samplerCube)

void main()
{
    mat4 projection = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, projection);
    mat4 view = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, view);
    vec4 screenSize = FLY_ACCESS_UNIFORM_BUFFER(
        UniformData, gPushConstants.uniformBufferIndex, screenSize);

    vec2 ndc = (gl_FragCoord.xy / screenSize.xy) * 2.0f - 1.0f;
    vec4 clipSpace = vec4(ndc, 1.0, 1.0);
    vec4 viewSpace = inverse(projection) * clipSpace;
    viewSpace /= viewSpace.w;
    mat3 invView = transpose(mat3(view));
    vec3 world = normalize(invView * viewSpace.xyz);

    outFragColor = texture(
        FLY_ACCESS_TEXTURE_BUFFER(Cubemaps,
        gPushConstants.skyBoxTextureIndex), world);
}
