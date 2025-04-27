#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(push_constant) uniform Indices
{
    uint cameraIndex;
    uint vertexBufferIndex;
    uint albedoTextureIndex;
} gIndices;

HLS_REGISTER_TEXTURE_BUFFER(AlbedoTexture, sampler2D)

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(
        HLS_ACCESS_TEXTURE_BUFFER(AlbedoTexture, gIndices.albedoTextureIndex),
        inUV);
}
