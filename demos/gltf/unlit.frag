#version 450
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(push_constant) uniform Indices
{
    uint cameraIndex;
    uint materialBufferIndex;
    uint vertexBufferIndex;
    uint materialIndex;
}
gIndices;

struct TextureProperty
{
    vec2 offset;
    vec2 scale;
    uint textureIndex;
    uint pad;
};

HLS_REGISTER_STORAGE_BUFFER(readonly, MaterialData, { TextureProperty albedo; })
HLS_REGISTER_TEXTURE_BUFFER(AlbedoTexture, sampler2D)

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    MaterialData material = HLS_ACCESS_STORAGE_BUFFER(
        MaterialData, gIndices.materialBufferIndex)[gIndices.materialIndex];
    outColor = texture(
        HLS_ACCESS_TEXTURE_BUFFER(AlbedoTexture, material.albedo.textureIndex),
        inUV);
}
