#version 460
#extension GL_GOOGLE_include_directive : enable
#include "bindless.glsl"

layout(push_constant) uniform Indices
{
    uint instanceDataBufferIndex;
    uint meshDataBufferIndex;
    uint cameraIndex;
    uint materialBufferIndex;
}
gIndices;

struct TextureProperty
{
    vec2 offset;
    vec2 scale;
    uint textureIndex;
    uint pad;
};

FLY_REGISTER_STORAGE_BUFFER(readonly, MaterialData, { TextureProperty albedo; })

FLY_REGISTER_TEXTURE_BUFFER(AlbedoTexture, sampler2D)

layout(location = 0) in vec2 inUV;
layout(location = 1) in flat uint inMaterialIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    MaterialData material = FLY_ACCESS_STORAGE_BUFFER(
        MaterialData, gIndices.materialBufferIndex)[inMaterialIndex];
    outColor = texture(
        FLY_ACCESS_TEXTURE_BUFFER(AlbedoTexture, material.albedo.textureIndex),
        inUV);
}
