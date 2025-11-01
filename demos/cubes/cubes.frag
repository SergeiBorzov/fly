#version 450
#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"

layout(push_constant) uniform Indices
{
    uint sceneDataIndex;
    uint textureIndex;
} gIndices;

layout(location = 0) in vec2 inUVs;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outFragColor;

FLY_REGISTER_TEXTURE_BUFFER(Texture, sampler2D)

void main()
{
    outFragColor =
        texture(FLY_ACCESS_TEXTURE_BUFFER(Texture, gIndices.textureIndex),
                inUVs) *
        vec4(inColor, 1.0f);
}
